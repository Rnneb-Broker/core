#include "broker/storage_manager.hpp"
#include "broker/broker.hpp"
#include <algorithm>
#include <boost/crc.hpp>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace highway {

// Forward declarations
static void create_segment_file_with_prealloc(const std::string &path,
                                              uint64_t size);

// ============================================================================
// Buffer Implementation
// ============================================================================

Buffer::Buffer(size_t capacity)
    : position_(0), capacity_(capacity), segment_start_(0) {
  data_.reserve(capacity);
}

bool Buffer::append_message(uint64_t offset,
                            const std::vector<uint8_t> &payload) {
  // Message layout: [offset(8) | size(4) | crc(4) | payload(var)]
  size_t total_size = sizeof(MessageHeader) + payload.size();

  if (position_ + total_size > capacity_) {
    return false; // Buffer full
  }

  // Compute CRC32 over payload
  boost::crc_32_type crc;
  crc.process_bytes(payload.data(), payload.size());
  uint32_t crc_value = crc.checksum();

  // Write header
  MessageHeader header{offset, static_cast<uint32_t>(payload.size()),
                       crc_value};
  data_.resize(position_ + total_size);
  std::memcpy(data_.data() + position_, &header, sizeof(MessageHeader));
  std::memcpy(data_.data() + position_ + sizeof(MessageHeader), payload.data(),
              payload.size());

  position_ += total_size;
  return true;
}

bool Buffer::is_full() const {
  // Use 95% threshold to avoid edge cases
  return position_ >= static_cast<size_t>(capacity_ * 0.95);
}

void Buffer::clear() {
  position_ = 0;
  data_.clear();
}

// ============================================================================
// SparseIndex Implementation
// ============================================================================

void SparseIndex::add(uint64_t offset, uint64_t file_position) {
  std::unique_lock lock(mutex_);
  entries_.push_back(SparseIndexEntry{offset, file_position});
}

std::optional<uint64_t> SparseIndex::find_file_position(uint64_t target) const {
  std::shared_lock lock(mutex_);

  if (entries_.empty())
    return 0;

  // Binary search for largest offset <= target
  auto it = std::upper_bound(
      entries_.begin(), entries_.end(), target,
      [](uint64_t t, const SparseIndexEntry &e) { return t < e.offset; });

  if (it == entries_.begin())
    return entries_.front().file_position;

  return (--it)->file_position;
}

void SparseIndex::clear() {
  std::unique_lock lock(mutex_);
  entries_.clear();
}

size_t SparseIndex::size() const {
  std::shared_lock lock(mutex_);
  return entries_.size();
}

// ============================================================================
// SegmentLog Implementation
// ============================================================================

SegmentLog::SegmentLog(const std::string &topic, const std::string&  storage_root)
    : topic_(topic),
      storage_root_(storage_root),
      buffer_pool_{Buffer(16 * 1024 * 1024), Buffer(16 * 1024 * 1024)} {
  active_buffer_.store(&buffer_pool_[0], std::memory_order_release);
  flush_buffer_.store(&buffer_pool_[1], std::memory_order_release);
  active_index_ = std::make_unique<SparseIndex>();
}

SegmentLog::~SegmentLog() = default;

uint64_t SegmentLog::append(const std::vector<uint8_t> &payload) {
  // Assign offset (single-writer, lock-free, no memory ordering needed locally)
  uint64_t assigned_offset =
      next_offset_.fetch_add(1, std::memory_order_relaxed);

  // Get active buffer (acquire semantics for visibility)
  Buffer *buf = active_buffer_.load(std::memory_order_acquire);

  // Append to buffer (hot path)
  if (!buf->append_message(assigned_offset, payload)) {
    // Buffer full, trigger rotation
    if (!flush_signaled_.exchange(true, std::memory_order_release)) {
      signal_flush();
    }

    // Try again on swapped buffer
    buf = active_buffer_.load(std::memory_order_acquire);
    buf->append_message(assigned_offset, payload);
  }

  return assigned_offset;
}

bool SegmentLog::should_flush() const {
  auto *buf = active_buffer_.load(std::memory_order_acquire);
  return buf->is_full();
}

void SegmentLog::signal_flush() {
  // Notify waiting flush worker that buffer needs flushing
  {
    std::lock_guard<std::mutex> lock(flush_signal_mutex_);
    // Lock held, now notify condition variable
  }
  flush_signal_cv_.notify_one();
}

Buffer *SegmentLog::acquire_flush_buffer() {
  // Atomically swap buffers
  Buffer *to_flush = active_buffer_.load(std::memory_order_acquire);

  // Get a clean buffer and make it active
  Buffer *clean_buf = flush_buffer_.load(std::memory_order_acquire);
  active_buffer_.store(clean_buf, std::memory_order_release);
  flush_signaled_.store(false, std::memory_order_release);

  return to_flush;
}

void SegmentLog::return_buffer(Buffer *buf) {
  buf->clear();
  flush_buffer_.store(buf, std::memory_order_release);
}

SegmentMetadata *SegmentLog::get_or_create_active_segment() {
  std::unique_lock lock(segments_mutex_);

  // Find active segment
  for (auto &[id, seg] : segments_) {
    if (seg.is_active && seg.byte_position < seg.max_size_bytes) {
      return &seg;
    }
  }

  // Create new segment
  uint64_t base_offset = next_offset_.load(std::memory_order_acquire);
  uint64_t segment_id = base_offset;

  SegmentMetadata seg;
  seg.id = segment_id;
  seg.base_offset = base_offset;
  seg.last_offset = 0;
  seg.byte_position = 0;
  seg.max_size_bytes = SEGMENT_SIZE_BYTES;
  seg.file_path = make_segment_filename(base_offset);
  seg.created_at = std::chrono::system_clock::now();
  seg.is_active = true;

  // Preallocate segment file
  try {
    create_segment_file_with_prealloc(seg.file_path, SEGMENT_SIZE_BYTES);
  } catch (const std::exception &e) {
    std::cerr << "[SegmentLog] Error creating segment file: " << e.what()
              << std::endl;
    throw;
  }

  segments_[segment_id] = seg;
  return &segments_[segment_id];
}

void SegmentLog::rotate_segment() {
  std::unique_lock lock(segments_mutex_);

  // Mark current as inactive
  for (auto &[id, seg] : segments_) {
    if (seg.is_active) {
      seg.is_active = false;
    }
  }

  // Next append will create new segment
}

SparseIndex *SegmentLog::get_active_index() { return active_index_.get(); }

std::optional<SegmentMetadata>
SegmentLog::load_segment(const std::string &file_path) {
  // Load segment metadata by scanning file for message boundaries
  try {
    if (!fs::exists(file_path)) {
      return std::nullopt;  // File doesn't exist
    }

    uint64_t file_size = fs::file_size(file_path);
    if (file_size == 0) {
      return std::nullopt;  // Empty file
    }

    // Read file and scan messages to get metadata
    int fd = ::open(file_path.c_str(), O_RDONLY);
    if (fd < 0) {
      throw std::runtime_error("Failed to open segment file: " + file_path);
    }

    std::vector<uint8_t> file_data(file_size);
    ssize_t read_bytes = ::read(fd, file_data.data(), file_size);
    ::close(fd);

    if (read_bytes != (ssize_t)file_size) {
      throw std::runtime_error("Failed to read entire segment file");
    }

    // Scan messages to extract metadata
    SegmentMetadata seg{};
    seg.file_path = file_path;
    seg.byte_position = 0;
    seg.max_size_bytes = SEGMENT_SIZE_BYTES;
    seg.is_active = false;
    seg.created_at = std::chrono::system_clock::now();

    size_t pos = 0;
    uint64_t last_offset = 0;
    bool found_valid_message = false;
    
    while (pos < file_data.size()) {
      if (pos + sizeof(MessageHeader) > file_data.size()) {
        break;  // Not enough bytes for header
      }

      MessageHeader hdr{};
      std::memcpy(&hdr, file_data.data() + pos, sizeof(MessageHeader));

      // Preallocated/tail padding is zero-filled, stop scan here.
      if (hdr.size == 0) {
        break;
      }
      
      // Validate CRC
      boost::crc_32_type crc;
      const uint8_t *payload = file_data.data() + pos + sizeof(MessageHeader);
      if (pos + sizeof(MessageHeader) + hdr.size > file_data.size()) {
        std::cerr << "[SegmentLog] Corrupted message at offset " << pos 
                  << ", truncating" << std::endl;
        break;  // Corruption detected, truncate here
      }

      crc.process_bytes(payload, hdr.size);
      if (crc.checksum() != hdr.crc32) {
        std::cerr << "[SegmentLog] CRC mismatch at offset " << pos 
                  << ", truncating segment" << std::endl;
        break;  // CRC error, truncate
      }

      if (!found_valid_message) {
        seg.base_offset = hdr.offset;  // First valid message sets base offset
        seg.id = hdr.offset;
        found_valid_message = true;
      }
      
      last_offset = hdr.offset;
      size_t msg_size = sizeof(MessageHeader) + hdr.size;
      pos += msg_size;
    }

    if (!found_valid_message) {
      return std::nullopt;  // No valid messages in this file
    }

    seg.last_offset = last_offset;
    seg.byte_position = pos;  // Valid bytes read

    // Truncate file if corruption was detected
    if (pos < file_data.size()) {
      truncate_segment_at_position(file_path, pos);
    }

    return seg;
  } catch (const std::exception &e) {
    std::cerr << "[SegmentLog] Error loading segment: " << e.what() << std::endl;
    return std::nullopt;
  }
}

void SegmentLog::recover() {
  // Scan segments directory and rebuild state from disk
  try {
    std::string topic_dir = storage_root_ + "/" + topic_;
    
    if (!fs::exists(topic_dir)) {
      std::cout << "[SegmentLog] Topic directory doesn't exist: " << topic_dir 
                << " (first startup)" << std::endl;
      return;
    }

    std::cout << "[SegmentLog] Recovering topic: " << topic_ << std::endl;

    // Scan all .log files in topic directory
    std::vector<std::string> segment_files;
    for (const auto &entry : fs::directory_iterator(topic_dir)) {
      if (entry.path().extension() == ".log") {
        segment_files.push_back(entry.path().string());
      }
    }

    // Sort by filename (which is based on offset)
    std::sort(segment_files.begin(), segment_files.end());

    // Load and validate each segment
    uint64_t max_offset = 0;
    std::unique_lock lock(segments_mutex_);

    for (const auto &file_path : segment_files) {
      auto seg_opt = load_segment(file_path);
      if (!seg_opt) {
        std::cerr << "[SegmentLog] Failed to load segment: " << file_path << std::endl;
        continue;
      }

      SegmentMetadata seg = seg_opt.value();
      segments_[seg.id] = seg;
      max_offset = std::max(max_offset, seg.last_offset);

        uint64_t recovered_count =
          (seg.last_offset >= seg.base_offset)
            ? (seg.last_offset - seg.base_offset + 1)
            : 0;

        std::cout << "[SegmentLog] Recovered segment " << seg.id 
            << " with " << recovered_count << " messages" << std::endl;
    }

    // Update offset counter to continue from where we left off
    next_offset_.store(max_offset + 1, std::memory_order_release);

    std::cout << "[SegmentLog] Recovery complete. Next offset: " 
              << next_offset_.load() << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "[SegmentLog] Recovery error: " << e.what() << std::endl;
  }
}

std::vector<uint8_t> SegmentLog::read_at_offset(uint64_t offset) {
  // Binary search segments, use sparse index, bounded scan, validate CRC
  try {
    std::shared_lock lock(segments_mutex_);

    // Find segment containing this offset
    SegmentMetadata *target_seg = nullptr;
    for (auto &[id, seg] : segments_) {
      if (seg.base_offset <= offset && offset <= seg.last_offset) {
        target_seg = &seg;
        break;
      }
    }

    if (!target_seg) {
      return {};  // Offset not found
    }

    // Use sparse index to find approximate file position
    auto file_pos_opt = active_index_->find_file_position(offset);
    if (!file_pos_opt) {
      return {};  // Index lookup failed
    }

    uint64_t start_pos = file_pos_opt.value();

    // Open segment file
    int fd = ::open(target_seg->file_path.c_str(), O_RDONLY);
    if (fd < 0) {
      throw std::runtime_error("Failed to open segment file: " + target_seg->file_path);
    }

    // Seek to approx position and bounded scan (max 64KB search window)
    if (::lseek(fd, start_pos, SEEK_SET) != (off_t)start_pos) {
      ::close(fd);
      throw std::runtime_error("Failed to seek in segment file");
    }

    std::vector<uint8_t> buffer(65536);  // 64KB search window
    ssize_t read_bytes = ::read(fd, buffer.data(), buffer.size());
    ::close(fd);

    if (read_bytes <= 0) {
      return {};
    }

    // Scan through buffer to find exact offset
    size_t pos = 0;
    while (pos < (size_t)read_bytes) {
      if (pos + sizeof(MessageHeader) > (size_t)read_bytes) {
        break;  // Not enough bytes for header
      }

      const MessageHeader *hdr = 
          reinterpret_cast<const MessageHeader *>(buffer.data() + pos);

      if (hdr->offset == offset) {
        // Found it! Validate CRC and return payload
        const uint8_t *payload = buffer.data() + pos + sizeof(MessageHeader);

        boost::crc_32_type crc;
        crc.process_bytes(payload, hdr->size);
        
        if (crc.checksum() != hdr->crc32) {
          std::cerr << "[SegmentLog] CRC mismatch for offset " << offset << std::endl;
          return {};  // CRC validation failed
        }

        // Return payload
        return std::vector<uint8_t>(payload, payload + hdr->size);
      }

      if (hdr->offset > offset) {
        break;  // Passed the target offset, not found
      }

      pos += sizeof(MessageHeader) + hdr->size;
    }

    return {};  // Offset not found in scanned region

  } catch (const std::exception &e) {
    std::cerr << "[SegmentLog] Error reading at offset " << offset 
              << ": " << e.what() << std::endl;
    return {};
  }
}

std::vector<uint64_t> SegmentLog::list_segment_ids() const {
  std::shared_lock lock(segments_mutex_);
  std::vector<uint64_t> ids;
  for (const auto &[id, seg] : segments_) {
    ids.push_back(id);
  }
  return ids;
}

std::optional<SegmentMetadata>
SegmentLog::get_segment(uint64_t segment_id) const {
  std::shared_lock lock(segments_mutex_);
  auto it = segments_.find(segment_id);
  if (it != segments_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void SegmentLog::delete_segment(uint64_t segment_id) {
  std::unique_lock lock(segments_mutex_);
  auto it = segments_.find(segment_id);
  if (it != segments_.end()) {
    // Delete file
    fs::remove(it->second.file_path);
    segments_.erase(it);
  }
}

uint64_t SegmentLog::get_total_disk_size() const {
  std::shared_lock lock(segments_mutex_);
  uint64_t total = 0;
  for (const auto &[id, seg] : segments_) {
    total += seg.byte_position;
  }
  return total;
}

SegmentLog::Stats SegmentLog::get_stats() const {
  std::shared_lock lock(segments_mutex_);
  Stats s;
  s.message_count = next_offset_.load(std::memory_order_acquire);
  s.disk_bytes = get_total_disk_size();
  s.segment_count = segments_.size();
  s.last_offset = next_offset_.load(std::memory_order_acquire) - 1;
  return s;
}

void SegmentLog::write_message_to_buffer(Buffer *buf, uint64_t offset,
                                         const std::vector<uint8_t> &payload) {
  // Helper for recovery
  buf->append_message(offset, payload);
}

std::string SegmentLog::make_segment_filename(uint64_t base_offset) const {
  std::ostringstream oss;
  oss << storage_root_ << "/" << topic_ << "/";
  oss << std::setfill('0') << std::setw(20) << base_offset << ".log";
  return oss.str();
}

// Helper: Create segment file with preallocate
static void create_segment_file_with_prealloc(const std::string &path,
                                              uint64_t size) {
  // Create parent directories if needed
  fs::create_directories(fs::path(path).parent_path());

  // Open and create file
  int fd = ::open(path.c_str(), O_CREAT | O_WRONLY, 0644);
  if (fd < 0) {
    throw std::runtime_error(std::string("Failed to create segment file: ") +
                             path);
  }

  // Preallocate space using fallocate (Linux) or ftruncate fallback
#ifdef __linux__
  if (::fallocate(fd, 0, 0, size) != 0) {
    ::close(fd);
    throw std::runtime_error(std::string("Failed to fallocate segment file: ") +
                             path);
  }
#else
  // Fallback for non-Linux
  if (::ftruncate(fd, size) != 0) {
    ::close(fd);
    throw std::runtime_error(std::string("Failed to ftruncate segment file: ") +
                             path);
  }
#endif

  ::close(fd);
}

// Helper: Write data to file at position
static void write_to_file(const std::string &path, uint64_t position,
                          const uint8_t *data, size_t size) {
  int fd = ::open(path.c_str(), O_WRONLY);
  if (fd < 0) {
    throw std::runtime_error(std::string("Failed to open segment file: ") +
                             path);
  }

  if (::lseek(fd, position, SEEK_SET) != (off_t)position) {
    ::close(fd);
    throw std::runtime_error(std::string("Failed to seek in segment file: ") +
                             path);
  }

  ssize_t written = ::write(fd, data, size);
  if (written != (ssize_t)size) {
    ::close(fd);
    throw std::runtime_error(std::string("Failed to write to segment file: ") +
                             path);
  }

  ::close(fd);
}

void SegmentLog::truncate_segment_at_position(const std::string &path,
                                              uint64_t position) {
  // Truncate segment file at given position (for corruption recovery)
  try {
    fs::resize_file(path, position);
    std::cout << "[SegmentLog] Truncated file " << path << " to position " << position
              << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "[SegmentLog] Failed to truncate file: " << e.what() << std::endl;
    throw;
  }
}

// ============================================================================
// FlushWorker Implementation
// ============================================================================

FlushWorker::FlushWorker(SegmentLog *log, DurabilityMode mode)
    : log_(log), mode_(mode), running_(false) {}

FlushWorker::~FlushWorker() { stop(); }

void FlushWorker::start() {
  if (running_.exchange(true, std::memory_order_acquire))
    return; // Already running

  worker_ = std::thread([this] { worker_loop(); });
}

void FlushWorker::stop() {
  if (!running_.exchange(false, std::memory_order_release))
    return; // Not running

  signal_cv_.notify_one();
  if (worker_.joinable()) {
    worker_.join();
  }
}

void FlushWorker::signal() { signal_cv_.notify_one(); }

FlushWorker::Stats FlushWorker::get_stats() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return Stats{
      messages_flushed_.load(std::memory_order_acquire),
      bytes_written_.load(std::memory_order_acquire),
      last_latency_
  };
}

void FlushWorker::worker_loop() {
  
  while (running_.load(std::memory_order_acquire)) {
    // Wait for signal from SegmentLog or timeout
    {
      std::unique_lock<std::mutex> lock(log_->flush_signal_mutex_);
      log_->flush_signal_cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
        return !running_.load(std::memory_order_acquire);
      });
    }

    // Acquire buffer to flush
    Buffer *buf = log_->acquire_flush_buffer();
    if (!buf || buf->size() == 0)
      continue;

    try {
      auto *segment = log_->get_or_create_active_segment();
      if (segment) {
        flush_buffer(buf);
      }
    } catch (const std::exception &e) {
      int error_code = errno;
      if (failure_cb_) {
        failure_cb_(log_->topic(), error_code, 0);
      }
    }

    log_->return_buffer(buf);
  }
}

void FlushWorker::flush_buffer(Buffer *buf) {
  if (!buf || buf->size() == 0)
    return;

  auto *index = log_->get_active_index();
  if (!index)
    return;

  auto *segment = log_->get_or_create_active_segment();
  if (!segment)
    return;

  // Record starting file position for this buffer
  uint64_t file_position = segment->byte_position;
  const uint8_t *data = buf->data();
  size_t pos = 0;

  // Scan messages in buffer
  while (pos < buf->size()) {
    MessageHeader *hdr = reinterpret_cast<MessageHeader *>(
        const_cast<uint8_t *>(data) + pos);

    // Validate CRC before writing
    boost::crc_32_type crc;
    const uint8_t *payload = data + pos + sizeof(MessageHeader);
    crc.process_bytes(payload, hdr->size);
    if (crc.checksum() != hdr->crc32) {
      throw std::runtime_error("CRC mismatch in flush_buffer");
    }

    // Add to sparse index every 1024 messages
    if (hdr->offset % 1024 == 0) {
      index->add(hdr->offset, file_position);
    }

    size_t msg_size = sizeof(MessageHeader) + hdr->size;
    file_position += msg_size;
    pos += msg_size;
  }

  // Write entire buffer to segment file
  write_to_segment(buf, *segment);

  // Check if segment is full, rotate if needed
  if (segment->byte_position >= segment->max_size_bytes) {
    log_->rotate_segment();
  }
}

void FlushWorker::write_to_segment(Buffer *buf, SegmentMetadata &segment) {
 

  if (!buf || buf->size() == 0)
    return;

  int fd = ::open(segment.file_path.c_str(), O_WRONLY);
  if (fd < 0) {
    throw std::runtime_error(std::string("Failed to open segment: ") +
                             segment.file_path);
  }

  // Seek to current write position
  if (::lseek(fd, segment.byte_position, SEEK_SET) !=
      (off_t)segment.byte_position) {
    ::close(fd);
    throw std::runtime_error(std::string("Failed to seek in segment: ") +
                             segment.file_path);
  }

  // Write buffer data
  const uint8_t *data = buf->data();
  size_t remaining = buf->size();
  uint64_t written_pos = 0;

  while (remaining > 0) {
    ssize_t n = ::write(fd, data + written_pos, remaining);
    if (n <= 0) {
      ::close(fd);
      throw std::runtime_error(std::string("Failed to write to segment: ") +
                               segment.file_path);
    }
    written_pos += n;
    remaining -= n;
  }

  // fsync based on durability mode
  if (mode_ == DurabilityMode::SAFE ||
      (mode_ == DurabilityMode::BALANCED &&
       segment.byte_position + buf->size() >= segment.max_size_bytes)) {
    if (::fsync(fd) != 0) {
      ::close(fd);
      throw std::runtime_error(std::string("Failed to fsync segment: ") +
                               segment.file_path);
    }
  }

  ::close(fd);

  // Update segment metadata
  segment.byte_position += buf->size();

  // Track statistics
  bytes_written_.fetch_add(buf->size(), std::memory_order_release);
  
  // Count messages in buffer (approximate by message headers)
  size_t msg_count = 0;
  size_t pos = 0;
  const uint8_t *stat_data = buf->data();
  while (pos < buf->size()) {
    const MessageHeader *hdr = reinterpret_cast<const MessageHeader *>(stat_data + pos);
    msg_count++;
    pos += sizeof(MessageHeader) + hdr->size;
  }
  messages_flushed_.fetch_add(msg_count, std::memory_order_release);
  flush_count_.fetch_add(1, std::memory_order_release);
}

void FlushWorker::handle_flush_error(const std::string &topic, int error_code,
                                     uint64_t segment_id) {
  if (failure_cb_) {
    failure_cb_(topic, error_code, segment_id);
  }
}

// ============================================================================
// StorageManager Implementation
// ============================================================================

StorageManager::StorageManager(Broker *broker, Config config = Config())
    : broker_(broker), config_(config) {}

StorageManager::~StorageManager() { shutdown(); }

void StorageManager::initialize() {
  // Create storage directory if it doesn't exist
  fs::create_directories(config_.storage_root);

  std::cout << "[StorageManager] Initializing storage engine..." << std::endl;
  std::cout << "[StorageManager] Storage root: " << config_.storage_root << std::endl;
  std::cout << "[StorageManager] Retention period: "
            << config_.retention_period.count() << " hours" << std::endl;

  // Recover all topics from disk (full cold-start recovery)
  recover_all();

  // Start background retention cleanup
  start_retention_cleanup();

  std::cout << "[StorageManager] Initialization complete" << std::endl;
}

void StorageManager::shutdown() {
  stop_retention_cleanup();

  // Stop all flush workers
  {
    std::unique_lock lock(workers_mutex_);
    for (auto &[topic, worker] : flush_workers_) {
      worker->stop();
    }
    flush_workers_.clear();
  }

  // Clear logs
  {
    std::unique_lock lock(logs_mutex_);
    logs_.clear();
  }
}

void StorageManager::on_publish(const std::string &topic,
                                const std::vector<uint8_t> &payload) {
  auto *log = get_or_create_log(topic);
  if (log) {
    log->append(payload);

    // Always get or create flush worker and signal it
    // This ensures immediate flushing even for small messages
    auto *worker = get_or_create_flush_worker(log);
    if (worker) {
      // Signal the worker to wake up and check for data
      log->signal_flush();
    }
  }
}

std::vector<uint8_t> StorageManager::read(const std::string &topic,
                                          uint64_t offset) {
  auto *log = get_or_create_log(topic);
  if (!log)
    return {};

  return log->read_at_offset(offset);
}

bool StorageManager::has_offset(const std::string &topic,
                                uint64_t offset) const {
  std::shared_lock lock(logs_mutex_);
  auto it = logs_.find(topic);
  if (it == logs_.end())
    return false;

  auto log = it->second.get();
  
  // Check if offset is within valid range
  uint64_t last_offset = log->get_last_offset();
  if (offset > last_offset) {
    return false;  // Haven't written that far yet
  }

  // Try to read - if it succeeds, offset exists
  auto payload = log->read_at_offset(offset);
  return !payload.empty();
}

void StorageManager::on_storage_failure(const std::string &topic,
                                        int error_code, uint64_t segment_id) {
  std::string error_msg = std::strerror(errno);
  std::cerr << "[StorageManager] FAILURE: topic=" << topic
            << " error_code=" << error_code << " (" << error_msg << ")"
            << " segment_id=" << segment_id << std::endl;

  // Emit broker-level alert for critical storage failures
  if (broker_) {
    // Log system event for monitoring/alerting
    std::cerr << "[ALERT] Storage failure detected! Topic: " << topic
              << ", Error: " << error_msg << std::endl;
    // In production: broker_->emit_event("sys.storage.failure", {topic, error_code});
  }
}

void StorageManager::recover_all() {
  std::cout << "[StorageManager] Starting cold start recovery..." << std::endl;

  // Phase 1: Discover topics from disk
  auto topics = discover_topics_from_disk();
  std::cout << "[StorageManager] Found " << topics.size()
            << " topics on disk" << std::endl;

  // Phase 2: Create SegmentLog instances for each discovered topic
  {
    std::unique_lock lock(logs_mutex_);
    for (const auto &topic : topics) {
      if (logs_.find(topic) == logs_.end()) {
        std::cout << "[StorageManager] Registering topic: " << topic << std::endl;
        logs_[topic] = std::make_unique<SegmentLog>(topic, config_.storage_root);
        
        // Register with broker's topic manager for statistics
        if (broker_) {
          broker_->topics().register_topic(topic);
        }
      }
    }
  }

  // Phase 3: Recover each log (scan segments, validate CRC, rebuild index)
  {
    std::unique_lock lock(logs_mutex_);
    for (auto &[topic, log] : logs_) {
      std::cout << "[StorageManager] Recovering topic: " << topic << std::endl;
      log->recover();

    }
  }

  std::cout << "[StorageManager] Recovery complete. "
            << logs_.size() << " topics loaded" << std::endl;
}

void StorageManager::start_retention_cleanup() {
  if (retention_running_.exchange(true, std::memory_order_release))
    return; // Already running

  retention_thread_ = std::thread([this] { retention_cleanup_loop(); });
}

void StorageManager::stop_retention_cleanup() {
  if (!retention_running_.exchange(false, std::memory_order_release))
    return; // Not running

  retention_cv_.notify_one();
  if (retention_thread_.joinable()) {
    retention_thread_.join();
  }
}

std::vector<std::string> StorageManager::list_topics() const {
  std::shared_lock lock(logs_mutex_);
  std::vector<std::string> topics;
  for (const auto &[name, log] : logs_) {
    topics.push_back(name);
  }
  return topics;
}

std::optional<SegmentLog::Stats>
StorageManager::get_topic_stats(const std::string &topic) const {
  std::shared_lock lock(logs_mutex_);
  auto it = logs_.find(topic);
  if (it != logs_.end()) {
    return it->second->get_stats();
  }
  return std::nullopt;
}

uint64_t StorageManager::get_total_storage_size() const {
  std::shared_lock lock(logs_mutex_);
  uint64_t total = 0;
  for (const auto &[topic, log] : logs_) {
    total += log->get_total_disk_size();
  }
  return total;
}

SegmentLog *StorageManager::get_or_create_log(const std::string &topic) {
  // Fast path: read lock
  {
    std::shared_lock lock(logs_mutex_);
    auto it = logs_.find(topic);
    if (it != logs_.end()) {
      return it->second.get();
    }
  }

  // Slow path: write lock
  {
    std::unique_lock lock(logs_mutex_);
    auto it = logs_.find(topic);
    if (it == logs_.end()) {
      logs_[topic] = std::make_unique<SegmentLog>(topic, config_.storage_root);
    }
    return logs_[topic].get();
  }
}

FlushWorker *StorageManager::get_or_create_flush_worker(SegmentLog *log) {
  const auto &topic = log->topic();

  std::unique_lock lock(workers_mutex_);
  auto it = flush_workers_.find(topic);
  if (it != flush_workers_.end()) {
    return it->second.get();
  }

  // Create new worker
  auto worker = std::make_unique<FlushWorker>(log, config_.durability_mode);
  worker->set_failure_callback(
      [this](const std::string &t, int err, uint64_t seg) {
        on_storage_failure(t, err, seg);
      });
  worker->start();

  auto *ptr = worker.get();
  flush_workers_[topic] = std::move(worker);
  return ptr;
}

void StorageManager::retention_cleanup_loop() {
  while (retention_running_.load(std::memory_order_acquire)) {
    std::unique_lock lock(retention_mutex_);
    retention_cv_.wait_for(lock, std::chrono::minutes(10), [this] {
      return !retention_running_.load(std::memory_order_acquire);
    });

    cleanup_old_segments();
  }
}

void StorageManager::cleanup_old_segments() {
  auto now = std::chrono::system_clock::now();
  auto cutoff = now - config_.retention_period;

  std::shared_lock lock(logs_mutex_);
  for (auto &[topic, log] : logs_) {
    auto segment_ids = log->list_segment_ids();
    for (auto seg_id : segment_ids) {
      auto seg_opt = log->get_segment(seg_id);
      if (seg_opt && seg_opt->created_at < cutoff && !seg_opt->is_active) {
        log->delete_segment(seg_id);
      }
    }
  }
}

std::vector<std::string> StorageManager::discover_topics_from_disk() const {
  std::vector<std::string> topics;
  std::unordered_set<std::string> unique_topics;

  if (!fs::exists(config_.storage_root)) {
    std::cout << "[StorageManager] Storage root doesn't exist: " << config_.storage_root
              << " (first startup)" << std::endl;
    return topics; // Empty storage, first start
  }

  try {
    // Recursively scan storage root and infer topic from parent directory of
    // each segment file (.log).
    for (const auto &entry : fs::recursive_directory_iterator(config_.storage_root)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".log") {
        continue;
      }

      // Topic path is the parent directory relative to storage root.
      // Example: ./storage/highway/1001/00000000000000000000.log -> highway/1001
      const fs::path topic_path = entry.path().parent_path();
      const fs::path rel_path = fs::relative(topic_path, config_.storage_root);

      if (rel_path.empty() || rel_path == ".") {
        continue;
      }

      std::string topic_name = rel_path.generic_string();
      if (!topic_name.empty()) {
        unique_topics.insert(topic_name);
      }
    }

    topics.assign(unique_topics.begin(), unique_topics.end());
  } catch (const std::exception &e) {
    std::cerr << "[StorageManager] Error discovering topics: " << e.what()
              << std::endl;
  }

  // Sort topics for deterministic ordering
  std::sort(topics.begin(), topics.end());

  return topics;
}

} // namespace highway
