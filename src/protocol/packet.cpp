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
        writer.write_bytes(data);
        return writer.release();
    }

    PublishPayload PublishPayload::deserialize(const uint8_t *data, size_t len)
    {
        BinaryReader reader(data, len);
        PublishPayload payload;
        payload.topic = reader.read_string();
        payload.packet_id = reader.read_u16();
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
    // Packet
    // ============================================================================

    std::vector<uint8_t> Packet::serialize() const
    {
        std::vector<uint8_t> result(sizeof(PacketHeader) + payload.size());
        std::memcpy(result.data(), &header, sizeof(PacketHeader));
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
