#include "client/client.hpp"
#include "protocol/serialization.hpp"
#include <iostream>

namespace highway
{

    Client::Client(Config config)
        : config_(std::move(config)),
          io_context_(),
          socket_(io_context_),
          keepalive_timer_(io_context_),
          work_guard_(boost::asio::make_work_guard(io_context_))
    {

        if (config_.client_id.empty())
        {
            config_.client_id = "client_" + std::to_string(std::rand());
        }
    }

    Client::~Client()
    {
        stop();
    }

    void Client::connect(ConnectHandler on_connect)
    {
        connect_handler_ = std::move(on_connect);

        tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(config_.host, std::to_string(config_.port));

        auto self = shared_from_this();
        boost::asio::async_connect(socket_, endpoints,
                                   [this, self](boost::system::error_code ec, const tcp::endpoint &)
                                   {
                                       if (!ec)
                                       {
                                           socket_.set_option(tcp::no_delay(true));
                                           send_connect_packet();
                                           read_header();
                                       }
                                       else
                                       {
                                           if (error_handler_)
                                           {
                                               error_handler_("Connection failed: " + ec.message());
                                           }
                                           if (connect_handler_)
                                           {
                                               connect_handler_(false);
                                           }
                                       }
                                   });
    }

    void Client::disconnect()
    {
        if (!connected_.load())
        {
            return;
        }

        // Send DISCONNECT packet
        Packet packet = Packet::create(PacketType::DISCONNECT, 0, {});
        send(packet);

        connected_ = false;

        boost::system::error_code ec;
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }

    void Client::send_connect_packet()
    {
        ConnectPayload payload;
        payload.client_id = config_.client_id;
        payload.username = config_.username;
        payload.password = config_.password;
        payload.keepalive = config_.keepalive;

        Packet packet = Packet::create(PacketType::CONNECT, 0, payload.serialize());
        send(packet);
    }

    void Client::read_header()
    {
        auto self = shared_from_this();
        boost::asio::async_read(socket_,
                                boost::asio::buffer(&header_, sizeof(PacketHeader)),
                                [this, self](boost::system::error_code ec, std::size_t)
                                {
                                    if (!ec)
                                    {
                                        if (header_.remaining_len > 0)
                                        {
                                            read_payload(header_.remaining_len);
                                        }
                                        else
                                        {
                                            payload_buffer_.clear();
                                            process_packet();
                                            read_header();
                                        }
                                    }
                                    else if (ec != boost::asio::error::operation_aborted)
                                    {
                                        connected_ = false;
                                        if (error_handler_)
                                        {
                                            error_handler_("Read error: " + ec.message());
                                        }
                                    }
                                });
    }

    void Client::read_payload(uint16_t length)
    {
        payload_buffer_.resize(length);
        auto self = shared_from_this();
        boost::asio::async_read(socket_,
                                boost::asio::buffer(payload_buffer_),
                                [this, self](boost::system::error_code ec, std::size_t)
                                {
                                    if (!ec)
                                    {
                                        process_packet();
                                        read_header();
                                    }
                                    else if (ec != boost::asio::error::operation_aborted)
                                    {
                                        connected_ = false;
                                        if (error_handler_)
                                        {
                                            error_handler_("Read error: " + ec.message());
                                        }
                                    }
                                });
    }

    void Client::process_packet()
    {
        PacketType type = static_cast<PacketType>(header_.type);

        switch (type)
        {
        case PacketType::CONNACK:
            handle_connack();
            break;
        case PacketType::PUBLISH:
            handle_publish();
            break;
        case PacketType::SUBACK:
            handle_suback();
            break;
        case PacketType::PINGRESP:
            handle_pingresp();
            break;
        default:
            break;
        }
    }

    void Client::handle_connack()
    {
        if (payload_buffer_.size() >= 2)
        {
            ConnectResult result = static_cast<ConnectResult>(payload_buffer_[1]);

            if (result == ConnectResult::Accepted)
            {
                connected_ = true;
                start_keepalive();

                if (connect_handler_)
                {
                    connect_handler_(true);
                }
            }
            else
            {
                if (error_handler_)
                {
                    error_handler_("Connection rejected: " + std::to_string(static_cast<int>(result)));
                }
                if (connect_handler_)
                {
                    connect_handler_(false);
                }
            }
        }
    }

    void Client::handle_publish()
    {
        try
        {
            auto payload = PublishPayload::deserialize(
                payload_buffer_.data(), payload_buffer_.size());

            if (message_handler_)
            {
                message_handler_(payload.topic, payload.data);
            }
        }
        catch (const std::exception &e)
        {
            if (error_handler_)
            {
                error_handler_("Failed to parse PUBLISH: " + std::string(e.what()));
            }
        }
    }

    void Client::handle_suback()
    {
        // Subscription confirmed
    }

    void Client::handle_pingresp()
    {
        // Keepalive acknowledged
    }

    void Client::start_keepalive()
    {
        if (config_.keepalive == 0)
        {
            return;
        }

        auto self = shared_from_this();
        keepalive_timer_.expires_after(std::chrono::seconds(config_.keepalive / 2));
        keepalive_timer_.async_wait([this, self](boost::system::error_code ec)
                                    {
        if (!ec && connected_) {
            Packet ping = Packet::create(PacketType::PINGREQ, 0, {});
            send(ping);
            start_keepalive();
        } });
    }

    void Client::publish(const std::string &topic, const std::vector<uint8_t> &payload, QoS qos)
    {
        if (!connected_)
        {
            return;
        }

        PublishPayload pub;
        pub.topic = topic;
        pub.packet_id = (qos != QoS::AtMostOnce) ? next_packet_id_++ : 0;
        pub.data = payload;

        uint8_t flags = static_cast<uint8_t>(qos) << 1;
        Packet packet = Packet::create(PacketType::PUBLISH, flags, pub.serialize());
        send(packet);
    }

    void Client::publish(const std::string &topic, const std::string &data, QoS qos)
    {
        std::vector<uint8_t> payload(data.begin(), data.end());
        publish(topic, payload, qos);
    }

    void Client::subscribe(const std::string &topic, QoS qos)
    {
        if (!connected_)
        {
            return;
        }

        SubscribePayload sub;
        sub.packet_id = next_packet_id_++;
        sub.topics.emplace_back(topic, qos);

        Packet packet = Packet::create(PacketType::SUBSCRIBE, 0x02, sub.serialize());
        send(packet);
    }

    void Client::unsubscribe(const std::string &topic)
    {
        if (!connected_)
        {
            return;
        }

        BinaryWriter writer;
        writer.write_u16(next_packet_id_++);
        writer.write_string(topic);

        Packet packet = Packet::create(PacketType::UNSUBSCRIBE, 0x02, writer.release());
        send(packet);
    }

    void Client::send(const Packet &packet)
    {
        send_raw(packet.serialize());
    }

    void Client::send_raw(std::vector<uint8_t> data)
    {
        auto self = shared_from_this();

        bool was_empty = write_queue_.empty();
        write_queue_.push_back(std::move(data));

        if (was_empty)
        {
            write_next();
        }
    }

    void Client::write_next()
    {
        if (write_queue_.empty())
        {
            return;
        }

        auto self = shared_from_this();
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(write_queue_.front()),
                                 [this, self](boost::system::error_code ec, std::size_t)
                                 {
                                     if (!ec)
                                     {
                                         write_queue_.pop_front();
                                         if (!write_queue_.empty())
                                         {
                                             write_next();
                                         }
                                     }
                                     else if (ec != boost::asio::error::operation_aborted)
                                     {
                                         if (error_handler_)
                                         {
                                             error_handler_("Write error: " + ec.message());
                                         }
                                     }
                                 });
    }

    void Client::run()
    {
        running_ = true;
        io_context_.run();
    }

    void Client::run_async()
    {
        running_ = true;
        io_thread_ = std::thread([this]()
                                 { io_context_.run(); });
    }

    void Client::stop()
    {
        if (!running_.exchange(false))
        {
            return;
        }

        keepalive_timer_.cancel();
        disconnect();

        work_guard_.reset();
        io_context_.stop();

        if (io_thread_.joinable())
        {
            io_thread_.join();
        }
    }

} // namespace highway
