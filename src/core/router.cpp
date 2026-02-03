#include "core/router.hpp"
#include "core/peer_connection.hpp"
#include "core/peer_manager.hpp"
#include "core/node.hpp"
#include <iostream>

Router::Router(uint64_t self_id, PeerManager &peers, Node &node)
    : self_id_(self_id), peers_(peers), node_(node) {}

void Router::on_message(Message msg, PeerConnection *from)
{
    if (msg.header.dst_node_id == self_id_)
    {
        std::string text(msg.payload.begin(), msg.payload.end());
        // Use the receive handler callback
        node_.get_receive_handler()(self_id_, msg.header.src_node_id, text);
        return;
    }
    else
    {
        std::string text(msg.payload.begin(), msg.payload.end());
        // !DEBUG: Use the receive handler callback
        std::cout << "[WASSIT]:" << msg.header.src_node_id << " -> " << self_id_ << " -> " << msg.header.dst_node_id << std::endl;
    }

    if (msg.header.ttl == 0)
        return;

    msg.header.ttl--;
    forward(msg, from);
}

void Router::forward(Message &msg, PeerConnection *from)
{
    peers_.for_each([&](auto &peer)
                    {
        if (peer.get() != from) {
            peer->async_send(msg);
        } });
}
