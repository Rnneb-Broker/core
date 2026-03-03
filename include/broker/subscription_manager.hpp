#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <memory>
#include <functional>

#include "protocol/packet.hpp"

namespace highway
{

    class Session;
    
    /**
     * Subscription mode (per session per topic)
     */
    enum class SubscriptionMode {
        PUSH_LIVE,           // Default: deliver new messages as they arrive
        CATCHUP_THEN_PUSH    // Replay from offset, then switch to PUSH_LIVE
    };
    
    /**
     * Subscription entry
     */
    struct Subscription
    {
        Session *session;
        std::string pattern;        // May contain wildcards
        QoS qos;
        SubscriptionMode mode;      // Subscription mode
        uint64_t current_offset;    // Current replay offset (for CATCHUP_THEN_PUSH)
        
        Subscription(Session *s, const std::string &p, QoS q, 
                    SubscriptionMode m = SubscriptionMode::PUSH_LIVE,
                    uint64_t offset = 0)
            : session(s), pattern(p), qos(q), mode(m), current_offset(offset) {}
    };

    /**
     * Manages all subscriptions across the broker
     *
     * Efficiently routes messages to interested subscribers
     */
    class SubscriptionManager
    {
    public:
        SubscriptionManager() = default;

        /**
         * Add a subscription
         * @param session The subscribing client session
         * @param pattern Topic pattern (may include + and # wildcards)
         * @param qos Requested QoS level
         * @param mode Subscription mode (PUSH_LIVE or CATCHUP_THEN_PUSH)
         * @param start_offset Starting offset for CATCHUP_THEN_PUSH mode
         */
        void subscribe(Session *session, const std::string &pattern, QoS qos,
                      SubscriptionMode mode = SubscriptionMode::PUSH_LIVE,
                      uint64_t start_offset = 0);

        /**
         * Remove a subscription
         */
        void unsubscribe(Session *session, const std::string &pattern);

        /**
         * Remove all subscriptions for a session
         */
        void remove_session(Session *session);

        /**
         * Get all sessions subscribed to a topic
         * @param topic Concrete topic (no wildcards)
         * @return List of matching subscriptions
         */
        std::vector<Subscription> get_subscribers(const std::string &topic) const;

        /**
         * Get subscription count
         */
        size_t subscription_count() const;

        /**
         * Get all patterns a session is subscribed to
         */
        std::vector<std::string> get_session_subscriptions(Session *session) const;

        /**
         * Update subscription mode (e.g., CATCHUP_THEN_PUSH -> PUSH_LIVE)
         * Thread-safe update for replay state transitions
         */
        void update_subscription_mode(Session *session, const std::string &pattern,
                                     SubscriptionMode new_mode);

        /**
         * Update current offset for a catchup subscription
         * Used during replay to track progress
         */
        void update_subscription_offset(Session *session, const std::string &pattern,
                                       uint64_t new_offset);

    private:
        mutable std::shared_mutex mutex_;

        // Pattern -> set of subscriptions
        std::unordered_map<std::string, std::vector<Subscription>> subscriptions_by_pattern_;

        // Session -> set of patterns (for cleanup)
        std::unordered_map<Session *, std::unordered_set<std::string>> patterns_by_session_;
    };

} // namespace highway
