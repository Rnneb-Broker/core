#include "broker/storage_manager.hpp"
#include "broker/broker.hpp"
#include <iostream>
#include <csignal>
#include <memory>

std::shared_ptr<highway::Broker> g_broker;
std::shared_ptr<highway::StorageManager> g_storage_manager;

void signal_handler(int signal)
{
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_broker)
    {
        g_broker->stop();
    }
}

int main(int argc, char *argv[])
{
    // Parse command line arguments
    highway::Broker::Config config;
    highway::StorageManager::Config storage_config = highway::StorageManager::Config();

    

    if (argc > 1)
    {
        config.port = static_cast<uint16_t>(std::stoi(argv[1]));
    }
    if (argc > 2)
    {
        config.io_threads = static_cast<size_t>(std::stoi(argv[2]));
    }

    std::cout << "=======================================" << std::endl;
    std::cout << "    Highway Message Broker v1.0" << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << "  Port:       " << config.port << std::endl;
    std::cout << "  IO Threads: " << config.io_threads << std::endl;
    std::cout << "  Max Conn:   " << config.max_connections << std::endl;
    std::cout << "=======================================" << std::endl;

    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try
    {
        g_broker = std::make_shared<highway::Broker>(config);
        g_storage_manager =  std::make_shared<highway::StorageManager>(g_broker.get(), storage_config);
        g_storage_manager->initialize();
        g_broker->start();

        bool tick = true;

        // Keep main thread alive and print stats periodically
        while (g_broker->is_running())
        {
            std::this_thread::sleep_for(std::chrono::seconds(10));

            if (g_broker->is_running())
            {
                auto stats = g_broker->get_stats();
                std::cout << "\n"
                          << (tick
                                  ? "-"
                                  : "|")
                          << "[STATS] Connections : " << stats.active_connections
                          << " | Topics: "
                          << stats.topics_count
                          << " | Subscriptions: "
                          << stats.subscriptions_count
                          << " | Msgs In: "
                          << stats.total_messages_in
                          << " | Msgs Out: "
                          << stats.total_messages_out
                          << std::endl;

                tick = !tick;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
