#include "core//node.hpp"
#include "core/peer_connection.hpp"
#include "helpers/load_to_server.hpp"

Node::Node(uint64_t id, uint16_t port)
    : io_(),
      work_(boost::asio::make_work_guard(io_)),
      acceptor_(io_, tcp::endpoint(tcp::v4(), port)),
      router_(id, peers_, *this),
      id_(id),
      receive_handler_(default_receive_handler)
{
    std::cout << this->id_ << " is initialised" << std::endl;
}

Node::~Node()
{
    io_.stop();
    if (thread_.joinable())
        thread_.join();
}

void Node::set_receive_handler(ReceiveHandler handler)
{
    receive_handler_ = std::move(handler);
}

void Node::default_receive_handler(uint64_t node_id, uint64_t from_id, const std::string &msg)
{
    if (msg.size() < sizeof(uint64_t) * 2)
    {
        return;
    }

    uint64_t ts;
    uint64_t seq;

    std::memcpy(&ts, msg.data() + sizeof(uint64_t), sizeof(uint64_t));
    std::memcpy(&seq, msg.data(), sizeof(uint64_t));

    uint64_t latency = now_ns() - ts;

    std::cout << "[NODE " << node_id << "] "
              << "from=" << from_id << " seq=" << seq;

    size_t latencyUniform;
    std::string unit;

    if (latency < 1'000) // < 1 µs
    {
        latencyUniform = 1;
        unit = "ns";
    }
    else if (latency < 1'000'000) // < 1 ms
    {
        latencyUniform = 1'000;
        unit = "µs";
    }
    else if (latency < 1'000'000'000) // < 1 s
    {
        latencyUniform = 1'000'000;
        unit = "ms";
    }
    else
    {
        latencyUniform = 1'000'000'000;
        unit = "s";
    }

    std::cout
        << " latency(" << unit << ")=" << (float)latency / latencyUniform << std::endl;
    return;
}

void Node::run()
{
    accept_loop();
    thread_ = std::thread([this]
                          { io_.run(); });
}

void Node::accept_loop()
{
    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket)
                           {
        if (!ec) {
            std::cout << "\n[NODE " << this->id_ << "] Accepted connection from " 
                      << socket.remote_endpoint() << std::endl;
            auto peer = std::make_shared<PeerConnection>(std::move(socket), router_);
            peers_.add(peer);
            peer->start();
        }
        accept_loop(); });
}

void Node::connect(const tcp::endpoint &ep)
{
    std::cout << "trying to connect 0" << std::endl;

    auto socket_ptr = std::make_shared<tcp::socket>(io_);
    std::cout << "trying to connect 1" << std::endl;

    socket_ptr->async_connect(ep, [this, socket_ptr, ep](boost::system::error_code ec) mutable
                              {
            std::cout << "io stopped? " << io_.stopped() << std::endl;
        if (!ec) {
            auto peer = std::make_shared<PeerConnection>(std::move(*socket_ptr), router_);
            peers_.add(peer);
            peer->start();
            std::cout << "\n[NODE " << this->id_ << "] Connected to " << ep << std::endl;
        } else {
            std::cerr << "\n[NODE " << this->id_ << "] Connection failed: " << ec.message() << std::endl;
        } });
}

void Node::connect(const Node &n)
{
    connect(n.get_endpoint());
}

tcp::endpoint Node::get_endpoint() const
{
    return acceptor_.local_endpoint();
}

void Node::send(uint64_t dst, std::string_view data)
{
    Message msg;
    msg.header.type = static_cast<uint16_t>(MessageType::Data);
    msg.header.src_node_id = id_;
    msg.header.dst_node_id = dst;
    msg.header.ttl = 2;
    msg.header.size = data.size();

    msg.payload.assign(data.begin(), data.end());

    // std::cout << "\n[NODE " << id_ << "] Sending message to " << dst
    //   << " via peers" << std::endl;
    peers_.for_each([&](auto &peer)
                    { peer->async_send(msg); });
}
