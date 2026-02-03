#include "core/node.hpp"
#include "helpers/connect_server_client_arch.hpp"
#include "helpers/fill_nodes_vetor.hpp"
#include "helpers/load_to_server.hpp"
#include <chrono>
#include <thread>
#include <iostream>

#define NODES_COLLECTION_SIZE 2

int main()
{
  try
  {
    std::vector<std::shared_ptr<Node>> nodes = fill_nodes_vector(NODES_COLLECTION_SIZE);
    connect_server_client_arch(nodes);

    std::vector<std::thread> threads;
    threads.reserve(nodes.size());

    for (auto &node : nodes)
    {
      threads.emplace_back(load_to_server, node, 0UL);
    }

    for (auto &t : threads)
      t.join();
  }
  catch (const std::exception &e)
  {
    std::cerr << "Fatal error: " << e.what() << "\n";
  }

  return 0;
}
