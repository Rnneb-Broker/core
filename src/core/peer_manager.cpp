#include "core/peer_manager.hpp"
#include "core/peer_connection.hpp"

void PeerManager::add(std::shared_ptr<PeerConnection> peer)
{
    peers_.insert(peer);
}

void PeerManager::remove(PeerConnection *peer)
{
    for (auto it = peers_.begin(); it != peers_.end(); ++it)
    {
        if (it->get() == peer)
        {
            peers_.erase(it);
            return;
        }
    }
}
