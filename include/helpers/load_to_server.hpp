#pragma once
#include "core/node.hpp"
#include <vector>
#include <chrono>

static inline uint64_t now_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void load_to_server(std::shared_ptr<Node> node, size_t serverIndex = 0);