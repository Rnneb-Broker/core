#include <helpers/load_to_server.hpp>
#include <helpers/fill_nodes_vetor.hpp>
#include <thread>
#include <chrono>
#define MICROSECOND 10

using clock_t_ = std::chrono::steady_clock;

void load_to_server(std::shared_ptr<Node> node, size_t serverIndex)
{
    node->run();
    static uint64_t seq = 0;

    if (serverIndex == node->get_id() - BASE_PORT)
        return;
    while (tan)
    {
        seq++;
        uint64_t ts = now_ns();
        std::string payload;
        payload.resize(sizeof(uint64_t) * 2);

        std::memcpy(payload.data(), &seq, sizeof(uint64_t));
        std::memcpy(payload.data() + sizeof(uint64_t), &ts, sizeof(uint64_t));

        node->send(serverIndex + BASE_PORT, payload);
        seq++;
    }
}

// #include "benchmark/benchmark.hpp"
// #include "core/node.hpp"

// #include <algorithm>
// #include <thread>
// #include <random>
// #include <chrono>
// #include <iostream>
// #include <cstring>

// #ifndef PORT_BASE
// #define PORT_BASE 10000
// #endif

// using clock_t_ = std::chrono::steady_clock;

// /* ============================
//    STATIC NODE FACTORY
//    ============================ */
// std::vector<std::unique_ptr<Node>>
// Benchmark::generate_nodes(std::size_t count, uint16_t base_port) {
//   std::vector<std::unique_ptr<Node>> nodes;
//   nodes.reserve(count);

//   for (std::size_t i = 0; i < count; ++i) {
//     nodes.emplace_back(std::make_unique<Node>(
//         static_cast<uint64_t>(i), static_cast<uint16_t>(base_port + i))

//     );
//   }

//   return nodes;
// }

// /* ============================
//    EXISTING BENCHMARK CODE
//    ============================ */

// Benchmark::Benchmark(std::vector<std::unique_ptr<Node>> &nodes,
//                      uint64_t duration_seconds)
//     : nodes_(nodes), duration_s_(duration_seconds) {}

// void Benchmark::start() {
//   run_nodes();
//   connect_server_client();

//   std::cout << "Starting The Benchmark" << std::endl;
//   running_.store(true, std::memory_order_release);
//   start_tp_ = clock_t_::now();

//   for (auto &n : nodes_) {
//     n->set_receive_handler(
//         [this](uint64_t node_id, uint64_t from_id, const std::string &msg) {
//           on_receive(node_id, from_id, msg);
//         });
//   }

//   std::thread([this] {
//     constexpr auto interval = std::chrono::microseconds(200); // ~5k msg/s

//     auto next = clock_t_::now();

//     while (running_.load(std::memory_order_acquire)) {
//       send_tick();
//       next += interval;
//       std::this_thread::sleep_until(next);
//     }
//   }).detach();
// }

// void Benchmark::send_tick() {
//   static uint64_t seq = 0;

//   uint64_t ts = now_ns();
//   std::string payload;
//   payload.resize(sizeof(uint64_t) * 2);

//   std::memcpy(payload.data(), &seq, sizeof(uint64_t));
//   std::memcpy(payload.data() + sizeof(uint64_t), &ts, sizeof(uint64_t));

//   static std::size_t index = 1;
//   uint64_t dst = 0;

//   if (!index)
//     index = (index + 1) % nodes_.size();
//   nodes_[index]->send(dst, payload);

//   ++sent_;
//   ++seq;
//   index = (index + 1) % nodes_.size();
// }

// void Benchmark::on_receive(uint64_t node_id, uint64_t from_id,
//                            const std::string &msg) {
//   if (msg.size() < sizeof(uint64_t) * 2) {
//     ++dropped_;
//     return;
//   }

//   uint64_t ts;
//   uint64_t seq;

//   std::memcpy(&ts, msg.data() + sizeof(uint64_t), sizeof(uint64_t));
//   std::memcpy(&seq, msg.data(), sizeof(uint64_t));

//   uint64_t latency = now_ns() - ts;

//   {
//     std::lock_guard lk(latency_mtx_);
//     latencies_ns_.push_back(latency);
//   }

//   ++received_;

//   std::cout << "[NODE " << node_id << "] "
//             << "from=" << from_id << " seq=" << seq
//             << " latency(ns)=" << latency << std::endl;
// }

// Benchmark::Result Benchmark::wait_and_collect() {
//   std::this_thread::sleep_for(std::chrono::seconds(duration_s_));

//   running_.store(false, std::memory_order_release);

//   auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
//                      clock_t_::now() - start_tp_)
//                      .count();

//   std::vector<uint64_t> lats;
//   {
//     std::lock_guard lk(latency_mtx_);
//     lats = latencies_ns_;
//   }

//   std::sort(lats.begin(), lats.end());

//   auto pct = [&](double p) -> uint64_t {
//     if (lats.empty())
//       return 0;
//     std::size_t idx = static_cast<std::size_t>(p * lats.size());
//     return lats[std::min(idx, lats.size() - 1)];
//   };

//   return {
//       sent_.load(), received_.load(), dropped_.load(), sent_.load() / elapsed,
//       pct(0.50),    pct(0.95),        pct(0.99)};
// }

// void Benchmark::connect_server_client(int server_index) {

//   if (server_index >= nodes_.size())
//     throw std::overflow_error("The Server index has a overflow position");

//   tcp::endpoint ep(boost::asio::ip::make_address("127.0.0.1"),
//                    PORT_BASE + server_index);

//   for (size_t i = 0; i < nodes_.size(); i++) {
//     if (i == server_index)
//       continue;

//     nodes_[i]->connect(ep);
//   }
// }

// void Benchmark::run_nodes() {
//   for (auto &n : nodes_)
//     n->run();
// }
