#include "helpers/connect_server_client_arch.hpp"

void connect_server_client_arch(std::vector<std::shared_ptr<Node>> &nodes)
{
    for (size_t i = 1; i < nodes.size(); i++)
        nodes[i]->connect(*nodes[0]);
}
