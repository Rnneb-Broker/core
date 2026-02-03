#include "data/sensor_event.hpp"

namespace highway {

std::vector<uint8_t> TrafficAlert::serialize() const {
    BinaryWriter writer;
    writer.write_u8(static_cast<uint8_t>(type));
    writer.write_u64(sensor_id);
    writer.write_u64(static_cast<uint64_t>(timestamp));
    writer.write_u64(static_cast<uint64_t>(duration_ms));
    
    uint32_t speed_bits;
    std::memcpy(&speed_bits, &avg_speed, sizeof(float));
    writer.write_u32(speed_bits);
    
    writer.write_u32(vehicle_count);
    
    return writer.release();
}

TrafficAlert TrafficAlert::deserialize(const uint8_t* data, size_t len) {
    BinaryReader reader(data, len);
    
    TrafficAlert alert;
    alert.type = static_cast<Type>(reader.read_u8());
    alert.sensor_id = reader.read_u64();
    alert.timestamp = static_cast<int64_t>(reader.read_u64());
    alert.duration_ms = static_cast<int64_t>(reader.read_u64());
    
    uint32_t speed_bits = reader.read_u32();
    std::memcpy(&alert.avg_speed, &speed_bits, sizeof(float));
    
    alert.vehicle_count = reader.read_u32();
    
    return alert;
}

} // namespace highway
