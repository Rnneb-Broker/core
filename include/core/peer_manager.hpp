#pragma once
#include <unordered_set>
#include <memory>

class PeerConnection;

class PeerManager
{
public:
    void add(std::shared_ptr<PeerConnection> peer);
    void remove(PeerConnection *peer);

    template <typename Fn>
    void for_each(Fn &&fn)
    {
        for (auto &p : peers_)
            fn(p);
    }

private:
    std::unordered_set<std::shared_ptr<PeerConnection>> peers_;
};
