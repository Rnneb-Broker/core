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
     * Subscription entry
     */
    struct Subscription
    {
        Session *session;
        std::string pattern; // May contain wildcards
        QoS qos;
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
         */
        void subscribe(Session *session, const std::string &pattern, QoS qos);

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

    private:
        mutable std::shared_mutex mutex_;

        // Pattern -> set of subscriptions
        std::unordered_map<std::string, std::vector<Subscription>> subscriptions_by_pattern_;

        // Session -> set of patterns (for cleanup)
        std::unordered_map<Session *, std::unordered_set<std::string>> patterns_by_session_;
    };

} // namespace highway
