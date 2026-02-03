#include "core/node.hpp"
#include <chrono>
#include <thread>
#include <iostream>

int main()
{
  try
  {
    // Create two nodes
    Node node1(1, 9001);
    Node node2(2, 9002);

    // Start their io_context threads
    node1.run();
    node2.run();

    // Connect node1 -> node2
    node1.connect(node2);

    // Optional: send a message
    node1.send(2, "hello from node 1");

    // Keep process alive
    while (tan)
    {
      std::this_thread::sleep_for(std::chrono::nanoseconds(100));
      node1.send(2, "hello from node 1");
      node2.send(1, "hello from node 2");
    }
  }
  catch (const std::exception &e)
  {
    std::cerr << "Fatal error: " << e.what() << "\n";
  }

  return 0;
}
