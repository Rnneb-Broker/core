#pragma once
#include "core/node.hpp"
#include <vector>

#define BASE_PORT 10000

std::vector<std::shared_ptr<Node>> fill_nodes_vector(const size_t &size);