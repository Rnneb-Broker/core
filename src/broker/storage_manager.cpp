#include "broker/storage_manager.hpp"
#include <boost/crc.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

namespace highway
{

// ============================================================================
// Buffer Implementation
// ============================================================================

Buffer::Buffer(size_t capacity)
    : position_(0), capacity_(capacity), segment_start_(0)
{
    data_.reserve(capacity);
}

bool Buffer::append_message(uint64_t offset, const std::vector<uint8_t>& payload)
{
    // Message layout: [offset(8) | size(4) | crc(4) | payload(var)]
    size_t total_size = sizeof(MessageHeader) + payload.size();

    if (position_ + total_size > capacity_)
    {
        return false; // Buffer full
    }

    // Compute CRC32 over payload
    boost::crc_32_type crc;
    crc.process_bytes(payload.data(), payload.size());
    uint32_t crc_value = crc.checksum();

    // Write header
    MessageHeader header{offset, static_cast<uint32_t>(payload.size()), crc_value};
    data_.resize(position_ + total_size);
    std::memcpy(data_.data() + position_, &header, sizeof(MessageHeader));
    std::memcpy(data_.data() + position_ + sizeof(MessageHeader), payload.data(),
                payload.size());

    position_ += total_size;
    return true;
}

bool Buffer::is_full() const
{
    // Use 95% threshold to avoid edge cases
    return position_ >= static_cast<size_t>(capacity_ * 0.95);
}

void Buffer::clear()
{
    position_ = 0;
    data_.clear();
}

// ============================================================================
// SparseIndex Implementation
// ============================================================================

void SparseIndex::add(uint64_t offset, uint64_t file_position)
{
    std::unique_lock lock(mutex_);
    entries_.push_back(SparseIndexEntry{offset, file_position});
}

std::optional<uint64_t> SparseIndex::find_file_position(uint64_t target) const
{
    std::shared_lock lock(mutex_);

    if (entries_.empty())
        return 0;

    // Binary search for largest offset <= target
    auto it = std::upper_bound(
        entries_.begin(), entries_.end(), target,
        [](uint64_t t, const SparseIndexEntry& e) { return t < e.offset; });

    if (it == entries_.begin())
        return entries_.front().file_position;

    return (--it)->file_position;
}

void SparseIndex::clear()
{
    std::unique_lock lock(mutex_);
    entries_.clear();
}

size_t SparseIndex::size() const
{
    std::shared_lock lock(mutex_);
    return entries_.size();
}

// ============================================================================
// SegmentLog Implementation
// ============================================================================

SegmentLog::SegmentLog(const std::string& topic)
    : topic_(topic), buffer_pool_{Buffer(16 * 1024 * 1024), Buffer(16 * 1024 * 1024)}
{
    active_buffer_.store(&buffer_pool_[0], std::memory_order_release);
    flush_buffer_.store(&buffer_pool_[1], std::memory_order_release);
    active_index_ = std::make_unique<SparseIndex>();
}

SegmentLog::~SegmentLog() = default;

uint64_t SegmentLog::append(const std::vector<uint8_t>& payload)
{
    // Assign offset (single-writer, lock-free, no memory ordering needed locally)
    uint64_t assigned_offset = next_offset_.fetch_add(1, std::memory_order_relaxed);

    // Get active buffer (acquire semantics for visibility)
    Buffer* buf = active_buffer_.load(std::memory_order_acquire);

    // Append to buffer (hot path)
    if (!buf->append_message(assigned_offset, payload))
    {
        // Buffer full, trigger rotation
        if (!flush_signaled_.exchange(true, std::memory_order_release))
        {
            signal_flush();
        }

        // Try again on swapped buffer
        buf = active_buffer_.load(std::memory_order_acquire);
        buf->append_message(assigned_offset, payload);
    }

    return assigned_offset;
}

bool SegmentLog::should_flush() const
{
    auto* buf = active_buffer_.load(std::memory_order_acquire);
    return buf->is_full();
}

void SegmentLog::signal_flush()
{
    // Flush worker will call this or monitor should_flush()
    // Placeholder for notification mechanism (handled by FlushWorker)
}

Buffer* SegmentLog::acquire_flush_buffer()
{
    // Atomically swap buffers
    Buffer* to_flush = active_buffer_.load(std::memory_order_acquire);

    // Get a clean buffer and make it active
    Buffer* clean_buf = flush_buffer_.load(std::memory_order_acquire);
    active_buffer_.store(clean_buf, std::memory_order_release);
    flush_signaled_.store(false, std::memory_order_release);

    return to_flush;
}

void SegmentLog::return_buffer(Buffer* buf)
{
    buf->clear();
    flush_buffer_.store(buf, std::memory_order_release);
}

SegmentMetadata* SegmentLog::get_or_create_active_segment()
{
    std::unique_lock lock(segments_mutex_);

    // Find active segment
    for (auto& [id, seg] : segments_)
    {
        if (seg.is_active)
        {
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

    // TODO: fallocate segment file

    segments_[segment_id] = seg;
    return &segments_[segment_id];
}

void SegmentLog::rotate_segment()
{
    std::unique_lock lock(segments_mutex_);

    // Mark current as inactive
    for (auto& [id, seg] : segments_)
    {
        if (seg.is_active)
        {
            seg.is_active = false;
        }
    }

    // Next append will create new segment
}

SparseIndex* SegmentLog::get_active_index()
{
    return active_index_.get();
}

std::optional<SegmentMetadata> SegmentLog::load_segment(const std::string& file_path)
{
    // TODO: Load segment metadata from file
    return std::nullopt;
}

void SegmentLog::recover()
{
    // TODO: Scan segments, validate CRC, truncate on corruption, rebuild index
    std::cout << "[StorageManager] Recovering topic: " << topic_ << std::endl;
}

std::vector<uint8_t> SegmentLog::read_at_offset(uint64_t offset)
{
    // TODO: Binary search segments, use sparse index, bounded scan, validate CRC
    return {};
}

std::vector<uint64_t> SegmentLog::list_segment_ids() const
{
    std::shared_lock lock(segments_mutex_);
    std::vector<uint64_t> ids;
    for (const auto& [id, seg] : segments_)
    {
        ids.push_back(id);
    }
    return ids;
}

std::optional<SegmentMetadata> SegmentLog::get_segment(uint64_t segment_id) const
{
    std::shared_lock lock(segments_mutex_);
    auto it = segments_.find(segment_id);
    if (it != segments_.end())
    {
        return it->second;
    }
    return std::nullopt;
}

void SegmentLog::delete_segment(uint64_t segment_id)
{
    std::unique_lock lock(segments_mutex_);
    auto it = segments_.find(segment_id);
    if (it != segments_.end())
    {
        // Delete file
        fs::remove(it->second.file_path);
        segments_.erase(it);
    }
}

uint64_t SegmentLog::get_total_disk_size() const
{
    std::shared_lock lock(segments_mutex_);
    uint64_t total = 0;
    for (const auto& [id, seg] : segments_)
    {
        total += seg.byte_position;
    }
    return total;
}

SegmentLog::Stats SegmentLog::get_stats() const
{
    std::shared_lock lock(segments_mutex_);
    Stats s;
    s.message_count = next_offset_.load(std::memory_order_acquire);
    s.disk_bytes = get_total_disk_size();
    s.segment_count = segments_.size();
    s.last_offset = next_offset_.load(std::memory_order_acquire) - 1;
    return s;
}

void SegmentLog::write_message_to_buffer(Buffer* buf, uint64_t offset,
                                          const std::vector<uint8_t>& payload)
{
    // Helper for recovery
    buf->append_message(offset, payload);
}

std::string SegmentLog::make_segment_filename(uint64_t base_offset) const
{
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(20) << base_offset << ".log";
    return oss.str();
}

void SegmentLog::truncate_segment_at_position(const std::string& path, uint64_t position)
{
    // TODO: Truncate file at given position
    fs::resize_file(path, position);
}

// ============================================================================
// FlushWorker Implementation
// ============================================================================

FlushWorker::FlushWorker(SegmentLog* log, DurabilityMode mode)
    : log_(log), mode_(mode), running_(false)
{
}

FlushWorker::~FlushWorker()
{
    stop();
}

void FlushWorker::start()
{
    if (running_.exchange(true, std::memory_order_acquire))
        return; // Already running

    worker_ = std::thread([this] { worker_loop(); });
}

void FlushWorker::stop()
{
    if (!running_.exchange(false, std::memory_order_release))
        return; // Not running

    signal_cv_.notify_one();
    if (worker_.joinable())
    {
        worker_.join();
    }
}

void FlushWorker::signal()
{
    signal_cv_.notify_one();
}

FlushWorker::Stats FlushWorker::get_stats() const
{
    // TODO: Track statistics
    return Stats{};
}

void FlushWorker::worker_loop()
{
    while (running_.load(std::memory_order_acquire))
    {
        // Wait for signal or timeout
        {
            std::unique_lock lock(signal_mutex_);
            signal_cv_.wait_for(lock, std::chrono::milliseconds(100),
                               [this] { return !running_.load(std::memory_order_acquire); });
        }

        // Acquire buffer to flush
        Buffer* buf = log_->acquire_flush_buffer();
        if (!buf || buf->size() == 0)
            continue;

        try
        {
            auto* segment = log_->get_or_create_active_segment();
            if (segment)
            {
                flush_buffer(buf);
            }
        }
        catch (const std::exception& e)
        {
            int error_code = errno;
            if (failure_cb_)
            {
                failure_cb_(log_->topic(), error_code, 0);
            }
        }

        log_->return_buffer(buf);
    }
}

void FlushWorker::flush_buffer(Buffer* buf)
{
    // TODO: Write buffer to segment, update index, fsync
    auto* index = log_->get_active_index();
    if (!index)
        return;

    // Scan messages in buffer and populate sparse index
    const uint8_t* data = buf->data();
    size_t pos = 0;
    uint64_t file_position = buf->segment_start();

    while (pos < buf->size())
    {
        MessageHeader* hdr = reinterpret_cast<MessageHeader*>((uint8_t*)data + pos);

        // Add to sparse index every 1024 messages
        if (hdr->offset % 1024 == 0)
        {
            index->add(hdr->offset, file_position);
        }

        size_t msg_size = sizeof(MessageHeader) + hdr->size;
        file_position += msg_size;
        pos += msg_size;
    }
}

void FlushWorker::write_to_segment(Buffer* buf, SegmentMetadata& segment)
{
    // TODO: Actually write to segment file
}

void FlushWorker::handle_flush_error(const std::string& topic, int error_code,
                                      uint64_t segment_id)
{
    if (failure_cb_)
    {
        failure_cb_(topic, error_code, segment_id);
    }
}

// ============================================================================
// StorageManager Implementation
// ============================================================================

StorageManager::StorageManager(Broker* broker, Config config)
    : broker_(broker), config_(config)
{
}

StorageManager::~StorageManager()
{
    shutdown();
}

void StorageManager::initialize()
{
    // Create storage directory
    fs::create_directories(config_.storage_root);

    // TODO: Recover from disk
    recover_all();

    // Start retention cleanup
    start_retention_cleanup();
}

void StorageManager::shutdown()
{
    stop_retention_cleanup();

    // Stop all flush workers
    {
        std::unique_lock lock(workers_mutex_);
        for (auto& [topic, worker] : flush_workers_)
        {
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

void StorageManager::on_publish(const std::string& topic, const std::vector<uint8_t>& payload)
{
    auto* log = get_or_create_log(topic);
    if (log)
    {
        log->append(payload);

        // Check if buffer needs flushing
        if (log->should_flush())
        {
            auto* worker = get_or_create_flush_worker(log);
            if (worker)
            {
                worker->signal();
            }
        }
    }
}

std::vector<uint8_t> StorageManager::read(const std::string& topic, uint64_t offset)
{
    auto* log = get_or_create_log(topic);
    if (!log)
        return {};

    return log->read_at_offset(offset);
}

bool StorageManager::has_offset(const std::string& topic, uint64_t offset) const
{
    std::shared_lock lock(logs_mutex_);
    auto it = logs_.find(topic);
    if (it == logs_.end())
        return false;

    // TODO: Check if offset is in in-memory buffer or storage
    return true;
}

void StorageManager::on_storage_failure(const std::string& topic, int error_code,
                                         uint64_t segment_id)
{
    std::cerr << "[StorageManager] FAILURE: topic=" << topic << " error_code=" << error_code
             << " segment_id=" << segment_id << std::endl;

    // TODO: Emit broker-level alert
    // broker_->emit_event("sys.storage.failure", {...});
}

void StorageManager::recover_all()
{
    std::unique_lock lock(logs_mutex_);
    for (auto& [topic, log] : logs_)
    {
        log->recover();
    }
}

void StorageManager::start_retention_cleanup()
{
    if (retention_running_.exchange(true, std::memory_order_release))
        return; // Already running

    retention_thread_ = std::thread([this] { retention_cleanup_loop(); });
}

void StorageManager::stop_retention_cleanup()
{
    if (!retention_running_.exchange(false, std::memory_order_release))
        return; // Not running

    retention_cv_.notify_one();
    if (retention_thread_.joinable())
    {
        retention_thread_.join();
    }
}

std::vector<std::string> StorageManager::list_topics() const
{
    std::shared_lock lock(logs_mutex_);
    std::vector<std::string> topics;
    for (const auto& [name, log] : logs_)
    {
        topics.push_back(name);
    }
    return topics;
}

std::optional<SegmentLog::Stats> StorageManager::get_topic_stats(const std::string& topic) const
{
    std::shared_lock lock(logs_mutex_);
    auto it = logs_.find(topic);
    if (it != logs_.end())
    {
        return it->second->get_stats();
    }
    return std::nullopt;
}

uint64_t StorageManager::get_total_storage_size() const
{
    std::shared_lock lock(logs_mutex_);
    uint64_t total = 0;
    for (const auto& [topic, log] : logs_)
    {
        total += log->get_total_disk_size();
    }
    return total;
}

SegmentLog* StorageManager::get_or_create_log(const std::string& topic)
{
    // Fast path: read lock
    {
        std::shared_lock lock(logs_mutex_);
        auto it = logs_.find(topic);
        if (it != logs_.end())
        {
            return it->second.get();
        }
    }

    // Slow path: write lock
    {
        std::unique_lock lock(logs_mutex_);
        auto it = logs_.find(topic);
        if (it == logs_.end())
        {
            logs_[topic] = std::make_unique<SegmentLog>(topic);
        }
        return logs_[topic].get();
    }
}

FlushWorker* StorageManager::get_or_create_flush_worker(SegmentLog* log)
{
    const auto& topic = log->topic();

    std::unique_lock lock(workers_mutex_);
    auto it = flush_workers_.find(topic);
    if (it != flush_workers_.end())
    {
        return it->second.get();
    }

    // Create new worker
    auto worker = std::make_unique<FlushWorker>(log, config_.durability_mode);
    worker->set_failure_callback([this](const std::string& t, int err, uint64_t seg) {
        on_storage_failure(t, err, seg);
    });
    worker->start();

    auto* ptr = worker.get();
    flush_workers_[topic] = std::move(worker);
    return ptr;
}

void StorageManager::retention_cleanup_loop()
{
    while (retention_running_.load(std::memory_order_acquire))
    {
        std::unique_lock lock(retention_mutex_);
        retention_cv_.wait_for(lock, std::chrono::minutes(10),
                              [this] { return !retention_running_.load(std::memory_order_acquire); });

        cleanup_old_segments();
    }
}

void StorageManager::cleanup_old_segments()
{
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - config_.retention_period;

    std::shared_lock lock(logs_mutex_);
    for (auto& [topic, log] : logs_)
    {
        auto segment_ids = log->list_segment_ids();
        for (auto seg_id : segment_ids)
        {
            auto seg_opt = log->get_segment(seg_id);
            if (seg_opt && seg_opt->created_at < cutoff && !seg_opt->is_active)
            {
                log->delete_segment(seg_id);
            }
        }
    }
}

} // namespace highway
