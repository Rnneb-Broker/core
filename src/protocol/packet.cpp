#include "protocol/packet.hpp"
#include "protocol/serialization.hpp"

namespace highway
{

    // ============================================================================
    // ConnectPayload
    // ============================================================================

    std::vector<uint8_t> ConnectPayload::serialize() const
    {
        BinaryWriter writer;
        writer.write_string(client_id);
        writer.write_string(username);
        writer.write_string(password);
        writer.write_u16(keepalive);
        return writer.release();
    }

    ConnectPayload ConnectPayload::deserialize(const uint8_t *data, size_t len)
    {
        BinaryReader reader(data, len);
        ConnectPayload payload;
        payload.client_id = reader.read_string();
        if (!reader.empty())
            payload.username = reader.read_string();
        if (!reader.empty())
            payload.password = reader.read_string();
        if (reader.remaining() >= 2)
            payload.keepalive = reader.read_u16();
        return payload;
    }

    // ============================================================================
    // PublishPayload
    // ============================================================================

    std::vector<uint8_t> PublishPayload::serialize() const
    {
        BinaryWriter writer;
        writer.write_string(topic);
        writer.write_u16(packet_id);
        writer.write_u64(offset);
        writer.write_bytes(data);
        return writer.release();
    }

    PublishPayload PublishPayload::deserialize(const uint8_t *data, size_t len)
    {
        BinaryReader reader(data, len);
        PublishPayload payload;
        payload.topic = reader.read_string();
        payload.packet_id = reader.read_u16();
        payload.offset = reader.read_u64();
        payload.data = reader.read_remaining();
        return payload;
    }

    // ============================================================================
    // SubscribePayload
    // ============================================================================

    std::vector<uint8_t> SubscribePayload::serialize() const
    {
        BinaryWriter writer;
        writer.write_u16(packet_id);
        for (const auto &[topic, qos] : topics)
        {
            writer.write_string(topic);
            writer.write_u8(static_cast<uint8_t>(qos));
        }
        return writer.release();
    }

    SubscribePayload SubscribePayload::deserialize(const uint8_t *data, size_t len)
    {
        BinaryReader reader(data, len);
        SubscribePayload payload;
        payload.packet_id = reader.read_u16();
        while (!reader.empty())
        {
            std::string topic = reader.read_string();
            QoS qos = static_cast<QoS>(reader.read_u8());
            payload.topics.emplace_back(topic, qos);
        }
        return payload;
    }

    // ============================================================================
    // FetchOnePayload
    // ============================================================================

    std::vector<uint8_t> FetchOnePayload::serialize() const
    {
        BinaryWriter writer;
        writer.write_string(topic);
        writer.write_u64(offset);
        return writer.release();
    }

    FetchOnePayload FetchOnePayload::deserialize(const uint8_t *data, size_t len)
    {
        BinaryReader reader(data, len);
        FetchOnePayload payload;
        payload.topic = reader.read_string();
        payload.offset = reader.read_u64();
        return payload;
    }

    // ============================================================================
    // FetchResponsePayload
    // ============================================================================

    std::vector<uint8_t> FetchResponsePayload::serialize() const
    {
        BinaryWriter writer;
        writer.write_string(topic);
        writer.write_u64(offset);
        writer.write_bytes(data);
        return writer.release();
    }

    FetchResponsePayload FetchResponsePayload::deserialize(const uint8_t *data, size_t len)
    {
        BinaryReader reader(data, len);
        FetchResponsePayload payload;
        payload.topic = reader.read_string();
        payload.offset = reader.read_u64();
        payload.data = reader.read_remaining();
        return payload;
    }

    // ============================================================================
    // SubscribeFromOffsetPayload
    // ============================================================================

    std::vector<uint8_t> SubscribeFromOffsetPayload::serialize() const
    {
        BinaryWriter writer;
        writer.write_u16(packet_id);
        writer.write_string(topic);
        writer.write_u64(start_offset);
        writer.write_u8(static_cast<uint8_t>(qos));
        return writer.release();
    }

    SubscribeFromOffsetPayload SubscribeFromOffsetPayload::deserialize(const uint8_t *data, size_t len)
    {
        BinaryReader reader(data, len);
        SubscribeFromOffsetPayload payload;
        payload.packet_id = reader.read_u16();
        payload.topic = reader.read_string();
        payload.start_offset = reader.read_u64();
        payload.qos = static_cast<QoS>(reader.read_u8());
        return payload;
    }

    // ============================================================================
    // OffsetNotFoundPayload
    // ============================================================================

    std::vector<uint8_t> OffsetNotFoundPayload::serialize() const
    {
        BinaryWriter writer;
        writer.write_string(topic);
        writer.write_u64(requested_offset);
        writer.write_u64(oldest_available);
        writer.write_u64(newest_available);
        return writer.release();
    }

    OffsetNotFoundPayload OffsetNotFoundPayload::deserialize(const uint8_t *data, size_t len)
    {
        BinaryReader reader(data, len);
        OffsetNotFoundPayload payload;
        payload.topic = reader.read_string();
        payload.requested_offset = reader.read_u64();
        payload.oldest_available = reader.read_u64();
        payload.newest_available = reader.read_u64();
        return payload;
    }

    // ============================================================================
    // Packet
    // ============================================================================

    std::vector<uint8_t> Packet::serialize() const
    {
        std::vector<uint8_t> result(sizeof(PacketHeader) + payload.size());
        
        // Serialize header with network byte order (big-endian)
        result[0] = header.type;
        result[1] = header.flags;
        // Convert remaining_len to network byte order (big-endian)
        result[2] = static_cast<uint8_t>((header.remaining_len >> 8) & 0xFF);
        result[3] = static_cast<uint8_t>(header.remaining_len & 0xFF);
        
        if (!payload.empty())
        {
            std::memcpy(result.data() + sizeof(PacketHeader), payload.data(), payload.size());
        }
        return result;
    }

    Packet Packet::create(PacketType type, uint8_t flags, const std::vector<uint8_t> &payload)
    {
        Packet packet;
        packet.header.type = static_cast<uint8_t>(type);
        packet.header.flags = flags;
        packet.header.remaining_len = static_cast<uint16_t>(payload.size());
        packet.payload = payload;
        return packet;
    }

} // namespace highway
