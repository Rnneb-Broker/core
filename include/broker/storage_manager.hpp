#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>
#include <map>
#include <optional>
#include <chrono>
#include <functional>

namespace highway
{

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Binary message layout in stream:
 *
 * | uint64 offset | uint32 size | uint32 crc | payload (var) |
 * | 8 bytes       | 4 bytes     | 4 bytes    | size bytes    |
 *
 * CRC32 computed over payload only.
 * Total header: 16 bytes, fixed.
 */
struct MessageHeader
{
    uint64_t offset;   // Monotonically increasing message ID
    uint32_t size;     // Payload size in bytes
    uint32_t crc32;    // CRC32 over payload
};

static_assert(sizeof(MessageHeader) == 16, "MessageHeader must be 16 bytes (packed)");

/**
 * Sparse index entry: one per 1024 messages
 * Used for O(log N) lookup in binary search.
 */
struct SparseIndexEntry
{
    uint64_t offset;        // Message offset
    uint64_t file_position; // Byte position in segment file
};

/**
 * Segment metadata: tracks segment lifecycle and boundaries
 */
struct SegmentMetadata
{
    uint64_t id;                  // Segment ID (usually base_offset)
    uint64_t base_offset;         // First offset in this segment
    uint64_t last_offset;         // Last offset written (initially 0)
    uint64_t byte_position;       // Current write position in file
    uint64_t max_size_bytes;      // Rotation threshold (512MB default)
    std::string file_path;        // Full path to segment file
    std::chrono::system_clock::time_point created_at;
    bool is_active;               // True if still accepting writes
};

// ============================================================================
// Buffer: Pre-allocated, circular write buffer
// ============================================================================

/**
 * Fixed-size message buffer with packed binary format.
 *
 * Used in double-buffering: one buffer is being written to (active),
 * while the other is being flushed (flush).
 *
 * Hot path constraints:
 * - No dynamic allocation during append
 * - No locks in append
 * - Cache-friendly layout
 */
class Buffer
{
public:
    explicit Buffer(size_t capacity = 16 * 1024 * 1024); // 16MB default
    ~Buffer() = default;

    // Non-copyable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Movable (for std::vector<Buffer>)
    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;

    /**
     * Append a message to buffer (hot path, O(1), no locks).
     * If buffer is full, returns false.
     */
    bool append_message(uint64_t offset, const std::vector<uint8_t>& payload);

    /**
     * Check if buffer should trigger rotation
     * (uses 95% threshold to avoid exact-size edge cases)
     */
    bool is_full() const;

    /**
     * Get remaining capacity
     */
    size_t remaining_capacity() const { return capacity_ - position_; }

    /**
     * Reset buffer for reuse
     */
    void clear();

    /**
     * Access underlying data
     */
    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return position_; }

    /**
     * Byte position within segment (populated by flush thread)
     */
    void set_segment_start(uint64_t pos) { segment_start_ = pos; }
    uint64_t segment_start() const { return segment_start_; }

private:
    std::vector<uint8_t> data_;
    size_t position_;           // Current write position
    size_t capacity_;           // Total allocated capacity
    uint64_t segment_start_;    // Base file position for this buffer
};

// ============================================================================
// Sparse Index: In-memory index for fast offset lookup
// ============================================================================

/**
 * Sparse index: one entry per INDEX_INTERVAL messages (1024 default).
 * Enables O(log N) binary search to find approx file position,
 * then bounded sequential scan to exact offset.
 */
class SparseIndex
{
public:
    SparseIndex() = default;

    /**
     * Add index entry for offset -> file_position mapping
     */
    void add(uint64_t offset, uint64_t file_position);

    /**
     * Binary search for file position corresponding to offset ≤ target
     * Returns std::nullopt if no entries exist.
     */
    std::optional<uint64_t> find_file_position(uint64_t target) const;

    /**
     * Clear all entries (used during initialization)
     */
    void clear();

    /**
     * Thread-safe read access
     */
    size_t size() const;

private:
    static constexpr uint64_t INDEX_INTERVAL = 1024; // One entry per 1024 messages

    std::vector<SparseIndexEntry> entries_;
    mutable std::shared_mutex mutex_;
};

// ============================================================================
// SegmentLog: Single-topic append-only log with double-buffering
// ============================================================================

class FlushWorker; // Forward declaration

/**
 * SegmentLog: Manages topic-scoped append-only message log.
 *
 * Threading model:
 * - Event loop thread: calls append() (single-writer, lock-free hot path)
 * - Flush thread: drains buffers and writes to disk (owned by FlushWorker)
 * - Recovery: single-threaded on startup
 *
 * Single-writer principle:
 * - Only event loop assigns offsets (next_offset_ is incremented no-lock)
 * - Only event loop appends to active_buffer
 * - Buffer swap between active <-> flush is atomic pointer exchange
 * - Flush thread operates on decoupled buffers
 */
class SegmentLog
{
public:
    explicit SegmentLog(const std::string& topic);
    ~SegmentLog();

    // Non-copyable, non-movable
    SegmentLog(const SegmentLog&) = delete;
    SegmentLog& operator=(const SegmentLog&) = delete;
    SegmentLog(SegmentLog&&) = delete;
    SegmentLog& operator=(SegmentLog&&) = delete;

    // ---- Hot Path (Event Loop) ----

    /**
     * Append message to log (hot path, O(1), lock-free).
     * Assigns offset and appends to active buffer.
     * May trigger buffer swap if full.
     * Returns assigned offset.
     */
    uint64_t append(const std::vector<uint8_t>& payload);

    /**
     * Check if buffer swap was triggered (used by flush thread signal)
     */
    bool should_flush() const;

    /**
     * Signal flush thread (called by append when buffer fills)
     */
    void signal_flush();

    // ---- Flush Thread Path ----

    /**
     * Acquire buffer that needs flushing.
     * Swaps active <-> flush atomically.
     * Returns pointer to buffer containing accumulated messages, or nullptr.
     * Flush thread calls this to drain buffered messages.
     */
    Buffer* acquire_flush_buffer();

    /**
     * Return buffer to pool after flush completes
     */
    void return_buffer(Buffer* buf);

    // ---- Segment Management ----

    /**
     * Get active segment for writing (lazy-created on first append)
     */
    SegmentMetadata* get_or_create_active_segment();

    /**
     * Rotate to new segment when size threshold exceeded
     */
    void rotate_segment();

    /**
     * Get sparse index for active segment
     */
    SparseIndex* get_active_index();

    /**
     * Load segment file (for recovery)
     */
    std::optional<SegmentMetadata> load_segment(const std::string& file_path);

    // ---- Recovery ----

    /**
     * Recover segment log from disk.
     * Called on broker startup.
     * Validates CRC, truncates at corruption, rebuilds sparse index.
     */
    void recover();

    // ---- Reader Access ----

    /**
     * Read message at exact offset from storage.
     * Returns message data or throws if not found/corrupted.
     */
    std::vector<uint8_t> read_at_offset(uint64_t offset);

    /**
     * Get last offset written (for recovery state restoration)
     */
    uint64_t get_last_offset() const { return next_offset_.load(std::memory_order_acquire) - 1; }

    /**
     * Get topic name
     */
    const std::string& topic() const { return topic_; }

    // ---- Info & Cleanup ----

    /**
     * List all segment IDs for this topic
     */
    std::vector<uint64_t> list_segment_ids() const;

    /**
     * Get segment by ID (for deletion, query)
     */
    std::optional<SegmentMetadata> get_segment(uint64_t segment_id) const;

    /**
     * Remove segment file and metadata (used by retention cleanup)
     */
    void delete_segment(uint64_t segment_id);

    /**
     * Get total size of all segments on disk
     */
    uint64_t get_total_disk_size() const;

    /**
     * Get statistics for monitoring
     */
    struct Stats
    {
        uint64_t message_count;
        uint64_t disk_bytes;
        uint64_t segment_count;
        uint64_t last_offset;
    };
    Stats get_stats() const;

private:
    friend class FlushWorker; // Allow direct buffer access

    std::string topic_;
    std::atomic<uint64_t> next_offset_{0};  // Single-writer offset counter

    // Double-buffered write
    Buffer buffer_pool_[2];                  // Pre-allocated buffers
    std::atomic<Buffer*> active_buffer_;     // Current write target
    std::atomic<Buffer*> flush_buffer_;      // Pending flush
    std::atomic<bool> flush_signaled_{false};

    // Segment management
    std::map<uint64_t, SegmentMetadata> segments_;  // segment_id -> metadata
    mutable std::shared_mutex segments_mutex_;
    std::unique_ptr<SparseIndex> active_index_;

    // Configuration
    static constexpr uint64_t SEGMENT_SIZE_BYTES = 512ULL * 1024 * 1024; // 512MB

    // Helper methods
    void write_message_to_buffer(Buffer* buf, uint64_t offset,
                                  const std::vector<uint8_t>& payload);
    std::string make_segment_filename(uint64_t base_offset) const;
    void truncate_segment_at_position(const std::string& path, uint64_t position);
};

// ============================================================================
// FlushWorker: Background thread for durability
// ============================================================================

/**
 * FlushWorker: Manages per-topic asynchronous buffer flushing.
 *
 * Runs in dedicated thread, writes buffered messages sequentially to disk
 * (no random I/O), calls fsync every 100ms or on demand.
 *
 * Durability modes:
 * FAST: Flush every 100ms (default for telemetry)
 * BALANCED: Flush when buffer fills
 * SAFE: Flush every message (not implemented in v1)
 */
class FlushWorker
{
public:
    enum class DurabilityMode
    {
        FAST,      // ~100ms
        BALANCED,  // On buffer full
        SAFE       // Per-message (reduces throughput)
    };

    // Callback for broker alerting on failure
    using FailureCallback = std::function<void(const std::string& topic, int error_code, uint64_t segment_id)>;

    explicit FlushWorker(SegmentLog* log, DurabilityMode mode = DurabilityMode::FAST);
    ~FlushWorker();

    // Non-copyable, non-movable
    FlushWorker(const FlushWorker&) = delete;
    FlushWorker& operator=(const FlushWorker&) = delete;
    FlushWorker(FlushWorker&&) = delete;
    FlushWorker& operator=(FlushWorker&&) = delete;

    /**
     * Start flush worker thread
     */
    void start();

    /**
     * Stop flush worker thread gracefully
     */
    void stop();

    /**
     * Register failure callback (called by StorageManager)
     */
    void set_failure_callback(FailureCallback cb) { failure_cb_ = cb; }

    /**
     * Signal flush (called by append when buffer fills)
     */
    void signal();

    /**
     * Get flush statistics
     */
    struct Stats
    {
        uint64_t messages_flushed;
        uint64_t bytes_written;
        std::chrono::milliseconds last_flush_latency;
    };
    Stats get_stats() const;

private:
    SegmentLog* log_;
    DurabilityMode mode_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::condition_variable signal_cv_;
    std::mutex signal_mutex_;

    FailureCallback failure_cb_;

    // Worker thread loop
    void worker_loop();

    // Flush one buffer to segment
    void flush_buffer(Buffer* buf);

    // Helper: write to segment file
    void write_to_segment(Buffer* buf, SegmentMetadata& segment);

    // Helper: handle flush errors with retry backoff
    void handle_flush_error(const std::string& topic, int error_code, uint64_t segment_id);
};

// ============================================================================
// StorageManager: Multi-topic storage coordinator
// ============================================================================

class Broker; // Forward declaration

/**
 * StorageManager: Central storage engine for all message topics in broker.
 *
 * Responsibilities:
 * - Registry of topic -> SegmentLog (lazy creation)
 * - Dispatch on_publish to appropriate SegmentLog
 * - Coordinate recovery on startup
 * - Manage retention cleanup tasks
 * - Failure alerting to broker
 *
 * Thread-safety:
 * - Reader-writer lock for topic registry (shared_mutex)
 * - Each SegmentLog is internally thread-safe (single-writer append)
 */
class StorageManager
{
public:
    struct Config
    {
        uint64_t segment_size_bytes = 512ULL * 1024 * 1024; // 512MB
        std::chrono::hours retention_period{24 * 7};         // 7 days
        std::string storage_root = "./storage";
        FlushWorker::DurabilityMode durability_mode = FlushWorker::DurabilityMode::FAST;

        Config() = default;
    };

    explicit StorageManager(Broker* broker, Config config = Config());
    ~StorageManager();

    // Non-copyable, non-movable
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;
    StorageManager(StorageManager&&) = delete;
    StorageManager& operator=(StorageManager&&) = delete;

    /**
     * Initialize storage (create directories, recover from disk)
     */
    void initialize();

    /**
     * Shutdown storage (flush all buffers, stop workers, close files)
     */
    void shutdown();

    // ---- Hot Path (Event Loop) ----

    /**
     * Process published message (hot path via Broker::on_publish).
     * Dispatches to appropriate SegmentLog.
     * Non-blocking: returns immediately.
     */
    void on_publish(const std::string& topic, const std::vector<uint8_t>& payload);

    // ---- Read Path ----

    /**
     * Read message at offset (post-publish, from storage or in-memory buffer).
     * Used for subscriber replay functionality.
     */
    std::vector<uint8_t> read(const std::string& topic, uint64_t offset);

    /**
     * Query if offset is available in this topic
     */
    bool has_offset(const std::string& topic, uint64_t offset) const;

    // ---- Storage Callbacks ----

    /**
     * Called by FlushWorker on flush failure.
     * Emits broker-level alert for ops.
     */
    void on_storage_failure(const std::string& topic, int error_code, uint64_t segment_id);

    // ---- Lifecycle ----

    /**
     * Recover all topics from persistent storage (called on startup)
     */
    void recover_all();

    /**
     * Start background retention cleanup task
     */
    void start_retention_cleanup();

    /**
     * Stop retention cleanup task
     */
    void stop_retention_cleanup();

    // ---- Query & Monitoring ----

    /**
     * Get all known topics
     */
    std::vector<std::string> list_topics() const;

    /**
     * Get statistics for a topic
     */
    std::optional<SegmentLog::Stats> get_topic_stats(const std::string& topic) const;

    /**
     * Get total storage size across all topics
     */
    uint64_t get_total_storage_size() const;

    /**
     * Get configuration
     */
    const Config& config() const { return config_; }

private:
    Broker* broker_;
    Config config_;

    // Topic registry: topic_name -> SegmentLog
    std::unordered_map<std::string, std::unique_ptr<SegmentLog>> logs_;
    mutable std::shared_mutex logs_mutex_;

    // Flush workers per topic
    std::unordered_map<std::string, std::unique_ptr<FlushWorker>> flush_workers_;
    std::mutex workers_mutex_;

    // Retention cleanup
    std::thread retention_thread_;
    std::atomic<bool> retention_running_{false};
    std::condition_variable retention_cv_;
    std::mutex retention_mutex_;

    // Helper methods
    SegmentLog* get_or_create_log(const std::string& topic);
    FlushWorker* get_or_create_flush_worker(SegmentLog* log);

    // Retention cleanup loop (runs in background thread)
    void retention_cleanup_loop();
    void cleanup_old_segments();
};

} // namespace highway
