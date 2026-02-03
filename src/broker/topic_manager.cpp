#include "broker/topic_manager.hpp"
#include <algorithm>
#include <sstream>
#include <mutex>

namespace highway
{

    std::vector<std::string> TopicManager::split_topic(const std::string &topic)
    {
        std::vector<std::string> levels;
        std::istringstream stream(topic);
        std::string level;

        while (std::getline(stream, level, '/'))
        {
            levels.push_back(level);
        }

        return levels;
    }

    bool TopicManager::matches(const std::string &pattern, const std::string &topic)
    {
        auto pattern_levels = split_topic(pattern);
        auto topic_levels = split_topic(topic);

        size_t pi = 0, ti = 0;

        while (pi < pattern_levels.size() && ti < topic_levels.size())
        {
            const auto &p = pattern_levels[pi];
            const auto &t = topic_levels[ti];

            if (p == "#")
            {
                // # matches everything from here on
                return true;
            }
            else if (p == "+")
            {
                // + matches exactly one level
                ++pi;
                ++ti;
            }
            else if (p == t)
            {
                // Exact match
                ++pi;
                ++ti;
            }
            else
            {
                // No match
                return false;
            }
        }

        // Check if we consumed everything
        if (pi == pattern_levels.size() && ti == topic_levels.size())
        {
            return true;
        }

        // Pattern ends with # and we have more topic levels
        if (pi < pattern_levels.size() && pattern_levels[pi] == "#")
        {
            return true;
        }

        return false;
    }

    bool TopicManager::is_valid_topic(const std::string &topic)
    {
        if (topic.empty())
        {
            return false;
        }

        // Topics for publishing cannot contain wildcards
        if (topic.find('+') != std::string::npos ||
            topic.find('#') != std::string::npos)
        {
            return false;
        }

        // Cannot start or end with /
        if (topic.front() == '/' || topic.back() == '/')
        {
            return false;
        }

        // No empty levels (consecutive /)
        if (topic.find("//") != std::string::npos)
        {
            return false;
        }

        return true;
    }

    bool TopicManager::is_valid_pattern(const std::string &pattern)
    {
        if (pattern.empty())
        {
            return false;
        }

        auto levels = split_topic(pattern);

        for (size_t i = 0; i < levels.size(); ++i)
        {
            const auto &level = levels[i];

            if (level.empty())
            {
                return false; // Empty level
            }

            if (level == "#")
            {
                // # must be last level
                if (i != levels.size() - 1)
                {
                    return false;
                }
            }
            else if (level == "+")
            {
                // + is valid anywhere
            }
            else
            {
                // Regular level - no wildcards mixed with text
                if (level.find('+') != std::string::npos ||
                    level.find('#') != std::string::npos)
                {
                    return false;
                }
            }
        }

        return true;
    }

    void TopicManager::register_topic(const std::string &topic)
    {
        if (!is_valid_topic(topic))
        {
            return;
        }

        std::unique_lock lock(mutex_);
        topics_[topic] = true;
    }

    std::vector<std::string> TopicManager::get_matching_topics(const std::string &pattern) const
    {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;

        for (const auto &[topic, _] : topics_)
        {
            if (matches(pattern, topic))
            {
                result.push_back(topic);
            }
        }

        return result;
    }

    size_t TopicManager::topic_count() const
    {
        std::shared_lock lock(mutex_);
        return topics_.size();
    }

    std::vector<std::string> TopicManager::get_all_topics() const
    {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        result.reserve(topics_.size());

        for (const auto &[topic, _] : topics_)
        {
            result.push_back(topic);
        }

        return result;
    }

} // namespace highway
