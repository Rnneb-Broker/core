#include "broker/subscription_manager.hpp"
#include "broker/topic_manager.hpp"
#include "broker/session.hpp"

namespace highway
{

    void SubscriptionManager::subscribe(Session *session, const std::string &pattern, QoS qos,
                                        SubscriptionMode mode, uint64_t start_offset)
    {
        std::unique_lock lock(mutex_);

        Subscription sub{session, pattern, qos, mode, start_offset};

        // Add to pattern -> subscriptions map
        auto &subs = subscriptions_by_pattern_[pattern];

        // Check if already subscribed, update QoS and mode if so
        for (auto &existing : subs)
        {
            if (existing.session == session)
            {
                existing.qos = qos;
                existing.mode = mode;
                existing.current_offset = start_offset;
                return;
            }
        }

        subs.push_back(sub);

        // Track for cleanup
        patterns_by_session_[session].insert(pattern);
    }

    void SubscriptionManager::unsubscribe(Session *session, const std::string &pattern)
    {
        std::unique_lock lock(mutex_);

        // Remove from pattern -> subscriptions map
        auto it = subscriptions_by_pattern_.find(pattern);
        if (it != subscriptions_by_pattern_.end())
        {
            auto &subs = it->second;
            subs.erase(
                std::remove_if(subs.begin(), subs.end(),
                               [session](const Subscription &s)
                               { return s.session == session; }),
                subs.end());

            if (subs.empty())
            {
                subscriptions_by_pattern_.erase(it);
            }
        }

        // Remove from session tracking
        auto session_it = patterns_by_session_.find(session);
        if (session_it != patterns_by_session_.end())
        {
            session_it->second.erase(pattern);
            if (session_it->second.empty())
            {
                patterns_by_session_.erase(session_it);
            }
        }
    }

    void SubscriptionManager::remove_session(Session *session)
    {
        std::unique_lock lock(mutex_);

        auto session_it = patterns_by_session_.find(session);
        if (session_it == patterns_by_session_.end())
        {
            return;
        }

        // Remove from all subscribed patterns
        for (const auto &pattern : session_it->second)
        {
            auto it = subscriptions_by_pattern_.find(pattern);
            if (it != subscriptions_by_pattern_.end())
            {
                auto &subs = it->second;
                subs.erase(
                    std::remove_if(subs.begin(), subs.end(),
                                   [session](const Subscription &s)
                                   { return s.session == session; }),
                    subs.end());

                if (subs.empty())
                {
                    subscriptions_by_pattern_.erase(it);
                }
            }
        }

        patterns_by_session_.erase(session_it);
    }

    std::vector<Subscription> SubscriptionManager::get_subscribers(const std::string &topic) const
    {
        std::shared_lock lock(mutex_);
        std::vector<Subscription> result;

        // Check all patterns to see which match this topic
        for (const auto &[pattern, subs] : subscriptions_by_pattern_)
        {
            if (TopicManager::matches(pattern, topic))
            {
                result.insert(result.end(), subs.begin(), subs.end());
            }
        }

        return result;
    }

    size_t SubscriptionManager::subscription_count() const
    {
        std::shared_lock lock(mutex_);
        size_t count = 0;
        for (const auto &[_, subs] : subscriptions_by_pattern_)
        {
            count += subs.size();
        }
        return count;
    }

    std::vector<std::string> SubscriptionManager::get_session_subscriptions(Session *session) const
    {
        std::shared_lock lock(mutex_);

        auto it = patterns_by_session_.find(session);
        if (it == patterns_by_session_.end())
        {
            return {};
        }

        return std::vector<std::string>(it->second.begin(), it->second.end());
    }

    void SubscriptionManager::update_subscription_mode(Session *session, const std::string &pattern,
                                                       SubscriptionMode new_mode)
    {
        std::unique_lock lock(mutex_);

        auto it = subscriptions_by_pattern_.find(pattern);
        if (it != subscriptions_by_pattern_.end())
        {
            for (auto &sub : it->second)
            {
                if (sub.session == session)
                {
                    sub.mode = new_mode;
                    return;
                }
            }
        }
    }

    void SubscriptionManager::update_subscription_offset(Session *session, const std::string &pattern,
                                                          uint64_t new_offset)
    {
        std::unique_lock lock(mutex_);

        auto it = subscriptions_by_pattern_.find(pattern);
        if (it != subscriptions_by_pattern_.end())
        {
            for (auto &sub : it->second)
            {
                if (sub.session == session)
                {
                    sub.current_offset = new_offset;
                    return;
                }
            }
        }
    }

} // namespace highway
