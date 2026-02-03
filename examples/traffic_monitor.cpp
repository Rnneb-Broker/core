/**
 * Example: Traffic Monitor (Consumer)
 * 
 * Subscribes to sensor telemetry and monitors for traffic slowdowns
 */

#include "client/client.hpp"
#include "data/sensor_event.hpp"
#include "common/time_utils.hpp"
#include <iostream>
#include <unordered_map>
#include <deque>
#include <mutex>

using namespace highway;

// Alert condition: speed 0-5 km/h for more than 3 minutes
constexpr float SLOW_SPEED_THRESHOLD = 5.0f;
constexpr int64_t SLOW_DURATION_MS = 3 * 60 * 1000;  // 3 minutes

struct SensorState {
    std::deque<std::pair<int64_t, float>> recent_speeds;  // timestamp, speed
    bool alert_active = false;
};

class TrafficMonitor {
public:
    TrafficMonitor(const std::string& broker_host, uint16_t broker_port) {
        Client::Config config;
        config.host = broker_host;
        config.port = broker_port;
        config.client_id = "traffic_monitor";
        
        client_ = std::make_shared<Client>(config);
        
        client_->set_message_handler([this](const std::string& topic, 
                                            const std::vector<uint8_t>& payload) {
            on_message(topic, payload);
        });
        
        client_->set_error_handler([](const std::string& error) {
            std::cerr << "[MONITOR] Error: " << error << std::endl;
        });
    }
    
    void start() {
        client_->connect([this](bool success) {
            if (success) {
                std::cout << "[MONITOR] Connected to broker" << std::endl;
                
                // Subscribe to all sensor telemetry using wildcard
                client_->subscribe("highway/+/telemetry");
                std::cout << "[MONITOR] Subscribed to highway/+/telemetry" << std::endl;
            } else {
                std::cerr << "[MONITOR] Failed to connect" << std::endl;
            }
        });
        
        client_->run();  // Blocking
    }
    
    void stop() {
        client_->stop();
    }
    
private:
    void on_message(const std::string& topic, const std::vector<uint8_t>& payload) {
        try {
            SensorEvent event = SensorEvent::deserialize(payload);
            process_event(event);
        } catch (const std::exception& e) {
            std::cerr << "[MONITOR] Failed to parse event: " << e.what() << std::endl;
        }
    }
    
    void process_event(const SensorEvent& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& state = sensor_states_[event.sensor_id];
        
        // Add to recent history
        state.recent_speeds.emplace_back(event.timestamp, event.speed_kmh);
        
        // Remove old entries (older than 5 minutes for some buffer)
        int64_t cutoff = event.timestamp - (5 * 60 * 1000);
        while (!state.recent_speeds.empty() && state.recent_speeds.front().first < cutoff) {
            state.recent_speeds.pop_front();
        }
        
        // Check for slow traffic condition
        check_slow_traffic(event.sensor_id, state, event.timestamp);
        
        // Print periodic updates
        if (++event_count_ % 1000 == 0) {
            std::cout << "[MONITOR] Processed " << event_count_ << " events, "
                      << "monitoring " << sensor_states_.size() << " sensors" << std::endl;
        }
    }
    
    void check_slow_traffic(uint64_t sensor_id, SensorState& state, int64_t now) {
        if (state.recent_speeds.empty()) {
            return;
        }
        
        // Find the oldest timestamp where speed was continuously <= threshold
        int64_t slow_start = now;
        
        for (auto it = state.recent_speeds.rbegin(); it != state.recent_speeds.rend(); ++it) {
            if (it->second > SLOW_SPEED_THRESHOLD) {
                break;  // Speed went above threshold
            }
            slow_start = it->first;
        }
        
        int64_t slow_duration = now - slow_start;
        
        if (slow_duration >= SLOW_DURATION_MS) {
            if (!state.alert_active) {
                state.alert_active = true;
                
                // Calculate average speed during slow period
                float total_speed = 0;
                int count = 0;
                for (const auto& [ts, speed] : state.recent_speeds) {
                    if (ts >= slow_start) {
                        total_speed += speed;
                        ++count;
                    }
                }
                float avg_speed = count > 0 ? total_speed / count : 0;
                
                std::cout << "\n*** TRAFFIC ALERT ***" << std::endl;
                std::cout << "  Sensor ID: " << sensor_id << std::endl;
                std::cout << "  Duration: " << (slow_duration / 1000) << " seconds" << std::endl;
                std::cout << "  Avg Speed: " << avg_speed << " km/h" << std::endl;
                std::cout << "  Possible traffic blockage!" << std::endl;
                std::cout << "*********************\n" << std::endl;
                
                // TODO: Publish alert to highway/{sensor_id}/alerts topic
            }
        } else if (state.alert_active && slow_duration < SLOW_DURATION_MS / 2) {
            // Reset alert if traffic starts moving again
            state.alert_active = false;
            std::cout << "[MONITOR] Sensor " << sensor_id << ": Traffic flowing again" << std::endl;
        }
    }
    
    std::shared_ptr<Client> client_;
    std::unordered_map<uint64_t, SensorState> sensor_states_;
    std::mutex mutex_;
    uint64_t event_count_ = 0;
};

int main(int argc, char* argv[]) {
    std::string broker_host = "127.0.0.1";
    uint16_t broker_port = 1883;
    
    if (argc > 1) broker_host = argv[1];
    if (argc > 2) broker_port = static_cast<uint16_t>(std::stoi(argv[2]));
    
    std::cout << "Starting Traffic Monitor..." << std::endl;
    std::cout << "Connecting to " << broker_host << ":" << broker_port << std::endl;
    
    TrafficMonitor monitor(broker_host, broker_port);
    monitor.start();
    
    return 0;
}
