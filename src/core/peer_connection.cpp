#include "core/peer_connection.hpp"
#include "core/router.hpp"

using boost::asio::buffer;
using boost::system::error_code;

PeerConnection::PeerConnection(tcp::socket socket, Router &router)
    : socket_(std::move(socket)), router_(router) {}

void PeerConnection::start()
{
    read_header_();
}

void PeerConnection::read_header_()
{
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        buffer(&header_, sizeof(header_)),
        [this, self](error_code ec, std::size_t)
        {
            if (!ec)
            {
                body_.resize(header_.size);
                read_body_();
            }
        });
}

void PeerConnection::read_body_()
{
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        buffer(body_),
        [this, self](error_code ec, std::size_t)
        {
            if (!ec)
            {
                Message msg;
                msg.header = header_;
                msg.payload = std::move(body_);
                router_.on_message(msg, this);
                read_header_();
            }
        });
}

void PeerConnection::async_send(const Message &msg)
{
    std::vector<uint8_t> buffer_data(sizeof(MessageHeader) + msg.payload.size());
    std::memcpy(buffer_data.data(), &msg.header, sizeof(MessageHeader));
    std::memcpy(buffer_data.data() + sizeof(MessageHeader),
                msg.payload.data(),
                msg.payload.size());

    bool writing = !write_queue_.empty();
    write_queue_.push_back(std::move(buffer_data));

    if (!writing)
    {
        write_next_();
    }
}

void PeerConnection::write_next_()
{
    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        buffer(write_queue_.front()),
        [this, self](error_code ec, std::size_t)
        {
            if (!ec)
            {
                write_queue_.pop_front();
                if (!write_queue_.empty())
                {
                    write_next_();
                }
            }
        });
}
