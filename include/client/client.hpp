#pragma once
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <deque>
#include <atomic>
#include <thread>

#include "protocol/packet.hpp"

namespace highway
{

    using boost::asio::ip::tcp;

    /**
     * Client for connecting to the Highway Broker
     *
     * Can be used as:
     * - Sensor (publisher): publishes telemetry data
     * - Consumer (subscriber): receives messages from subscribed topics
     */
    class Client : public std::enable_shared_from_this<Client>
    {
    public:
        using MessageHandler = std::function<void(const std::string &topic,
                                                  const std::vector<uint8_t> &payload)>;
        using ConnectHandler = std::function<void(bool success)>;
        using ErrorHandler = std::function<void(const std::string &error)>;

        struct Config
        {
            std::string host = "127.0.0.1";
            uint16_t port = 1883;
            std::string client_id;
            std::string username;
            std::string password;
            uint16_t keepalive = 60;
        };

        explicit Client(Config config);
        ~Client();

        // Non-copyable
        Client(const Client &) = delete;
        Client &operator=(const Client &) = delete;

        /**
         * Connect to the broker
         */
        void connect(ConnectHandler on_connect = nullptr);

        /**
         * Disconnect from the broker
         */
        void disconnect();

        /**
         * Check if connected
         */
        bool is_connected() const { return connected_.load(); }

        /**
         * Publish a message to a topic
         */
        void publish(const std::string &topic, const std::vector<uint8_t> &payload,
                     QoS qos = QoS::AtMostOnce);

        /**
         * Publish string data
         */
        void publish(const std::string &topic, const std::string &data,
                     QoS qos = QoS::AtMostOnce);

        /**
         * Subscribe to a topic pattern
         */
        void subscribe(const std::string &topic, QoS qos = QoS::AtMostOnce);

        /**
         * Unsubscribe from a topic
         */
        void unsubscribe(const std::string &topic);

        /**
         * Set message handler (called when receiving published messages)
         */
        void set_message_handler(MessageHandler handler) { message_handler_ = std::move(handler); }

        /**
         * Set error handler
         */
        void set_error_handler(ErrorHandler handler) { error_handler_ = std::move(handler); }

        /**
         * Run the client IO (blocking)
         */
        void run();

        /**
         * Run the client IO in background thread
         */
        void run_async();

        /**
         * Stop the client
         */
        void stop();

    private:
        void do_connect(ConnectHandler on_connect);
        void read_header();
        void read_payload(uint16_t length);
        void process_packet();
        void send(const Packet &packet);
        void send_raw(std::vector<uint8_t> data);
        void write_next();
        void send_connect_packet();
        void handle_connack();
        void handle_publish();
        void handle_suback();
        void handle_pingresp();
        void start_keepalive();

        Config config_;

        boost::asio::io_context io_context_;
        tcp::socket socket_;
        boost::asio::steady_timer keepalive_timer_;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;

        PacketHeader header_;
        std::vector<uint8_t> payload_buffer_;
        std::deque<std::vector<uint8_t>> write_queue_;

        std::atomic<bool> connected_{false};
        std::atomic<bool> running_{false};
        uint16_t next_packet_id_{1};

        MessageHandler message_handler_;
        ErrorHandler error_handler_;
        ConnectHandler connect_handler_;

        std::thread io_thread_;
    };

} // namespace highway
