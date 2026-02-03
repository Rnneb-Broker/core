#pragma once
#include <cstdint>
#include <chrono>

namespace highway {

/**
 * High-resolution timestamp in nanoseconds
 */
inline uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count()
    );
}

/**
 * Timestamp in milliseconds (for general use)
 */
inline int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

/**
 * Timestamp in microseconds (for latency measurements)
 */
inline uint64_t now_us() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count()
    );
}

/**
 * Format latency with appropriate unit
 */
struct FormattedLatency {
    double value;
    const char* unit;
    
    static FormattedLatency from_ns(uint64_t ns) {
        if (ns < 1'000) {
            return {static_cast<double>(ns), "ns"};
        } else if (ns < 1'000'000) {
            return {static_cast<double>(ns) / 1'000.0, "µs"};
        } else if (ns < 1'000'000'000) {
            return {static_cast<double>(ns) / 1'000'000.0, "ms"};
        } else {
            return {static_cast<double>(ns) / 1'000'000'000.0, "s"};
        }
    }
};

} // namespace highway
