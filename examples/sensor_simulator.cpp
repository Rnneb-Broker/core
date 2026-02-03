/**
 * Example: Highway Sensor Simulation
 *
 * Simulates multiple sensors publishing telemetry data to the broker
 */

#include "client/client.hpp"
#include "data/sensor_event.hpp"
#include "common/time_utils.hpp"
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using namespace highway;

class SensorSimulator
{
public:
    SensorSimulator(uint64_t sensor_id, const std::string &broker_host, uint16_t broker_port)
        : sensor_id_(sensor_id), rng_(std::random_device{}())
    {

        Client::Config config;
        config.host = broker_host;
        config.port = broker_port;
        config.client_id = "sensor_" + std::to_string(sensor_id);

        client_ = std::make_shared<Client>(config);

        client_->set_error_handler([this](const std::string &error)
                                   { std::cerr << "[SENSOR " << sensor_id_ << "] Error: " << error << std::endl; });
    }

    void start()
    {
        client_->connect([this](bool success)
                         {
            if (success) {
                std::cout << "[SENSOR " << sensor_id_ << "] Connected to broker" << std::endl;
                start_publishing();
            } else {
                std::cerr << "[SENSOR " << sensor_id_ << "] Failed to connect" << std::endl;
            } });

        client_->run_async();
    }

    void stop()
    {
        running_ = false;
        client_->stop();
    }

private:
    void start_publishing()
    {
        running_ = true;
        publish_thread_ = std::thread([this]()
                                      {
            std::uniform_int_distribution<uint64_t> car_dist(1, 100000);
            std::normal_distribution<float> speed_dist(80.0f, 20.0f);  // Mean 80 km/h
            
            while (running_) {
                // Simulate car passing
                SensorEvent event = SensorEvent::create(
                    car_dist(rng_),
                    sensor_id_,
                    std::max(0.0f, speed_dist(rng_))
                );
                
                client_->publish(event.topic(), event.serialize());
                
                // Simulate sensor rate (100 events/sec)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } });
    }

    uint64_t sensor_id_;
    std::shared_ptr<Client> client_;
    std::mt19937 rng_;
    std::thread publish_thread_;
    std::atomic<bool> running_{false};
};

int main(int argc, char *argv[])
{
    std::string broker_host = "127.0.0.1";
    uint16_t broker_port = 1883;
    size_t num_sensors = 5;

    if (argc > 1)
        broker_host = argv[1];
    if (argc > 2)
        broker_port = static_cast<uint16_t>(std::stoi(argv[2]));
    if (argc > 3)
        num_sensors = static_cast<size_t>(std::stoi(argv[3]));

    std::cout << "Starting " << num_sensors << " sensor simulators..." << std::endl;
    std::cout << "Connecting to " << broker_host << ":" << broker_port << std::endl;

    std::vector<std::unique_ptr<SensorSimulator>> sensors;

    for (size_t i = 0; i < num_sensors; ++i)
    {
        sensors.push_back(std::make_unique<SensorSimulator>(i + 1, broker_host, broker_port));
        sensors.back()->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "All sensors started. Press Enter to stop..." << std::endl;
    std::cin.get();

    for (auto &sensor : sensors)
    {
        sensor->stop();
    }

    return 0;
}
