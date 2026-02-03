#pragma once
#include <deque>
#include <memory>
#include <vector>
#include <boost/asio.hpp>

#include "message.hpp"

using boost::asio::ip::tcp;

class Router;

class PeerConnection : public std::enable_shared_from_this<PeerConnection>
{
public:
    explicit PeerConnection(tcp::socket socket, Router &router);
    void start();
    void async_send(const Message &msg);

    tcp::socket &socket() { return socket_; }

private:
    void read_header_();
    void read_body_();
    void write_next_();

    tcp::socket socket_;
    Router &router_;

    MessageHeader header_;
    std::vector<uint8_t> body_;

    std::deque<std::vector<uint8_t>> write_queue_;
};
