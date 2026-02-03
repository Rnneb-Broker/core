#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <deque>
#include <unordered_set>
#include <atomic>
#include <mutex>

#include "protocol/packet.hpp"

namespace highway
{

    using boost::asio::ip::tcp;

    class Broker;

    /**
     * Represents a client session (sensor or consumer)
     *
     * Handles:
     * - Protocol parsing
     * - Connection state
     * - Send queue management
     */
    class Session : public std::enable_shared_from_this<Session>
    {
    public:
        enum class State
        {
            Connected,     // TCP connected, awaiting CONNECT packet
            Authenticated, // CONNECT received and accepted
            Disconnecting, // Graceful shutdown in progress
            Disconnected   // Session ended
        };

        Session(tcp::socket socket, Broker &broker);
        ~Session();

        /**
         * Start the session (begin reading)
         */
        void start();

        /**
         * Close the session
         */
        void close();

        /**
         * Send a packet to this client
         */
        void send(const Packet &packet);

        /**
         * Send raw data to this client
         */
        void send_raw(std::vector<uint8_t> data);

        /**
         * Publish a message to this client (for subscribers)
         */
        void deliver(const std::string &topic, const std::vector<uint8_t> &payload, QoS qos);

        // Accessors
        const std::string &client_id() const { return client_id_; }
        State state() const { return state_; }
        bool is_authenticated() const { return state_ == State::Authenticated; }

        // Subscriptions tracked locally for cleanup
        void add_subscription(const std::string &topic);
        void remove_subscription(const std::string &topic);
        const std::unordered_set<std::string> &subscriptions() const { return subscriptions_; }

    private:
        void read_header();
        void read_payload(uint16_t length);
        void process_packet();
        void write_next();
        void do_write();

        // Packet handlers
        void handle_connect();
        void handle_publish();
        void handle_subscribe();
        void handle_unsubscribe();
        void handle_pingreq();
        void handle_disconnect();

        // Send helpers
        void send_connack(ConnectResult result);
        void send_suback(uint16_t packet_id, const std::vector<QoS> &granted_qos);
        void send_unsuback(uint16_t packet_id);
        void send_pingresp();

        tcp::socket socket_;
        Broker &broker_;

        State state_{State::Connected};
        std::string client_id_;
        std::string username_;
        uint16_t keepalive_{60};

        // Read buffers
        PacketHeader header_;
        std::vector<uint8_t> payload_buffer_;

        // Write queue (protected by write_mutex_)
        std::mutex write_mutex_;
        std::deque<std::vector<uint8_t>> write_queue_;
        std::atomic<bool> writing_{false};

        // Local subscription tracking
        std::unordered_set<std::string> subscriptions_;

        // Statistics
        std::atomic<uint64_t> messages_received_{0};
        std::atomic<uint64_t> messages_sent_{0};
    };

} // namespace highway
