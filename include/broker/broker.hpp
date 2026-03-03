#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>

#include "broker/session.hpp"
#include "broker/topic_manager.hpp"
#include "broker/subscription_manager.hpp"
#include "broker/storage_manager.hpp"
namespace highway
{

    using boost::asio::ip::tcp;

    /**
     * Central message broker server
     *
     * Handles:
     * - Client connections (sensors and consumers)
     * - Message routing via topics
     * - Subscription management
     * - Message buffering
     */
    class Broker : public std::enable_shared_from_this<Broker>
    {
    public:
        struct Config
        {
            uint16_t port = 1883; // Default MQTT port
            size_t max_connections = 10000;
            size_t io_threads = 4;
            size_t buffer_size = 65536; // Message buffer size
        };

        Broker();
        explicit Broker(Config config);
        ~Broker();

        // Non-copyable, non-movable
        Broker(const Broker &) = delete;
        Broker &operator=(const Broker &) = delete;

        /**
         * Start the broker server
         */
        void start();

        /**
         * Stop the broker server gracefully
         */
        void stop();

        /**
         * Check if broker is running
         */
        bool is_running() const { return running_.load(); }

        /**
         * Get broker statistics
         */
        struct Stats
        {
            size_t active_connections;
            size_t total_messages_in;
            size_t total_messages_out;
            size_t topics_count;
            size_t subscriptions_count;
        };
        Stats get_stats() const;

        // Internal: called by Session
        void on_session_closed(Session *session);
        void on_publish(const std::string &topic, const std::vector<uint8_t> &payload,
                        QoS qos, Session *from);
        void on_subscribe(Session *session, const std::string &topic, QoS qos,
                         SubscriptionMode mode = SubscriptionMode::PUSH_LIVE,
                         uint64_t start_offset = 0);
        void on_unsubscribe(Session *session, const std::string &topic);

        TopicManager &topics() { return topic_manager_; }
        SubscriptionManager &subscriptions() { return subscription_manager_; }
        StorageManager &storage() { return storage_manager_; }

    private:
        void accept_loop();
        void run_io_threads();

        Config config_;

        boost::asio::io_context io_context_;
        tcp::acceptor acceptor_;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;

        TopicManager topic_manager_;
        SubscriptionManager subscription_manager_;
        StorageManager storage_manager_;

        std::unordered_map<Session *, std::shared_ptr<Session>> sessions_;
        mutable std::mutex sessions_mutex_;

        std::vector<std::thread> io_threads_;
        std::atomic<bool> running_{false};

        // Statistics
        std::atomic<size_t> total_messages_in_{0};
        std::atomic<size_t> total_messages_out_{0};
    };

} // namespace highway
