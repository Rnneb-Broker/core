#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <memory>

namespace highway {

/**
 * Topic structure for hierarchical topic management
 * 
 * Supports MQTT-style wildcards:
 * - '+' matches single level: highway/+/sensor matches highway/A1/sensor
 * - '#' matches multiple levels: highway/# matches highway/A1/sensor/speed
 * 
 * Example topics for highway system:
 * - highway/A1/sensor/123/telemetry
 * - highway/A1/alerts
 * - highway/+/alerts (wildcard subscription)
 */
class TopicManager {
public:
    TopicManager() = default;

    /**
     * Check if a topic pattern matches a concrete topic
     * Pattern can contain + and # wildcards
     */
    static bool matches(const std::string& pattern, const std::string& topic);

    /**
     * Split topic into levels
     */
    static std::vector<std::string> split_topic(const std::string& topic);

    /**
     * Validate topic name (for publishing)
     */
    static bool is_valid_topic(const std::string& topic);

    /**
     * Validate topic pattern (for subscribing, allows wildcards)
     */
    static bool is_valid_pattern(const std::string& pattern);

    /**
     * Get all registered topics that match a pattern
     */
    std::vector<std::string> get_matching_topics(const std::string& pattern) const;

    /**
     * Register a topic (called on first publish)
     */
    void register_topic(const std::string& topic);

    /**
     * Get topic count
     */
    size_t topic_count() const;

    /**
     * Get all topics
     */
    std::vector<std::string> get_all_topics() const;

private:
    // Simple flat storage for now, can optimize with trie later
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, bool> topics_;  // topic -> exists
};

} // namespace highway
