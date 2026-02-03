#include "helpers/fill_nodes_vetor.hpp"

std::vector<std::shared_ptr<Node>> fill_nodes_vector(const size_t &size)
{
    std::vector<std::shared_ptr<Node>> nodes;
    nodes.reserve(size);

    for (size_t i = 0; i < size; i++)
    {
        nodes.emplace_back(std::make_shared<Node>(BASE_PORT + i, BASE_PORT + i));
    }

    return nodes;
}
