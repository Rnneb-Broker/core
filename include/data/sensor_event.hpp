#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

#include "protocol/serialization.hpp"

namespace highway {

/**
 * Telemetry event from highway sensor
 * 
 * Packed for efficient network transmission
 */
#pragma pack(push, 1)
struct SensorEventRaw {
    uint64_t car_id;       // Vehicle identifier
    uint64_t sensor_id;    // Sensor identifier
    int64_t  timestamp;    // Unix timestamp in milliseconds
    float    speed_kmh;    // Vehicle speed in km/h
    float    latitude;     // GPS latitude (optional, 0 if not available)
    float    longitude;    // GPS longitude (optional, 0 if not available)
};
#pragma pack(pop)

static_assert(sizeof(SensorEventRaw) == 36, "SensorEventRaw must be 36 bytes");

/**
 * High-level sensor event with serialization support
 */
struct SensorEvent {
    uint64_t car_id;
    uint64_t sensor_id;
    int64_t  timestamp;
    float    speed_kmh;
    float    latitude{0.0f};
    float    longitude{0.0f};
    
    // Additional fields (not in raw format)
    std::string lane;      // Lane identifier (optional)
    
    /**
     * Create event with current timestamp
     */
    static SensorEvent create(uint64_t car_id, uint64_t sensor_id, float speed_kmh) {
        SensorEvent event;
        event.car_id = car_id;
        event.sensor_id = sensor_id;
        event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        event.speed_kmh = speed_kmh;
        return event;
    }
    
    /**
     * Serialize to binary format
     */
    std::vector<uint8_t> serialize() const {
        BinaryWriter writer;
        writer.write_u64(car_id);
        writer.write_u64(sensor_id);
        writer.write_u64(static_cast<uint64_t>(timestamp));
        
        // Write float as raw bytes
        uint32_t speed_bits;
        std::memcpy(&speed_bits, &speed_kmh, sizeof(float));
        writer.write_u32(speed_bits);
        
        uint32_t lat_bits, lon_bits;
        std::memcpy(&lat_bits, &latitude, sizeof(float));
        std::memcpy(&lon_bits, &longitude, sizeof(float));
        writer.write_u32(lat_bits);
        writer.write_u32(lon_bits);
        
        return writer.release();
    }
    
    /**
     * Deserialize from binary format
     */
    static SensorEvent deserialize(const uint8_t* data, size_t len) {
        BinaryReader reader(data, len);
        
        SensorEvent event;
        event.car_id = reader.read_u64();
        event.sensor_id = reader.read_u64();
        event.timestamp = static_cast<int64_t>(reader.read_u64());
        
        uint32_t speed_bits = reader.read_u32();
        std::memcpy(&event.speed_kmh, &speed_bits, sizeof(float));
        
        if (reader.remaining() >= 8) {
            uint32_t lat_bits = reader.read_u32();
            uint32_t lon_bits = reader.read_u32();
            std::memcpy(&event.latitude, &lat_bits, sizeof(float));
            std::memcpy(&event.longitude, &lon_bits, sizeof(float));
        }
        
        return event;
    }
    
    static SensorEvent deserialize(const std::vector<uint8_t>& data) {
        return deserialize(data.data(), data.size());
    }
    
    /**
     * Generate topic for this event
     * Format: highway/{sensor_id}/telemetry
     */
    std::string topic() const {
        return "highway/" + std::to_string(sensor_id) + "/telemetry";
    }
};

/**
 * Traffic alert event
 */
struct TrafficAlert {
    enum class Type : uint8_t {
        SlowTraffic = 1,    // Speed 0-5 km/h for >3 min
        Congestion  = 2,    // Multiple slow vehicles
        Incident    = 3     // Potential accident
    };
    
    Type     type;
    uint64_t sensor_id;
    int64_t  timestamp;
    int64_t  duration_ms;   // How long condition persisted
    float    avg_speed;     // Average speed during alert period
    uint32_t vehicle_count; // Number of affected vehicles
    
    std::vector<uint8_t> serialize() const;
    static TrafficAlert deserialize(const uint8_t* data, size_t len);
    
    std::string topic() const {
        return "highway/" + std::to_string(sensor_id) + "/alerts";
    }
};

} // namespace highway
