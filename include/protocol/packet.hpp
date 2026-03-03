#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>

namespace highway
{

    /**
     * MQTT-lite packet types for the broker protocol
     */
    enum class PacketType : uint8_t
    {
        CONNECT = 0x10,     // Client connection request
        CONNACK = 0x20,     // Connection acknowledgment
        PUBLISH = 0x30,     // Publish message to topic
        PUBACK = 0x40,      // Publish acknowledgment
        SUBSCRIBE = 0x80,   // Subscribe to topic(s)
        SUBACK = 0x90,      // Subscribe acknowledgment
        UNSUBSCRIBE = 0xA0, // Unsubscribe from topic(s)
        UNSUBACK = 0xB0,    // Unsubscribe acknowledgment
        PINGREQ = 0xC0,     // Ping request (keepalive)
        PINGRESP = 0xD0,    // Ping response
        DISCONNECT = 0xE0,  // Graceful disconnect
        
        // Offset-based access (v1.1)  
        FETCH_ONE = 0x50,              // Fetch single message by offset
        FETCH_RESPONSE = 0x51,         // Response with message data
        SUBSCRIBE_FROM_OFFSET = 0x81,  // Subscribe starting from offset
        OFFSET_NOT_FOUND = 0x52        // Offset error response
    };

    /**
     * Quality of Service levels
     */
    enum class QoS : uint8_t
    {
        AtMostOnce = 0,  // Fire and forget
        AtLeastOnce = 1, // Acknowledged delivery
        ExactlyOnce = 2  // Guaranteed single delivery (not implemented yet)
    };

    /**
     * Connection result codes
     */
    enum class ConnectResult : uint8_t
    {
        Accepted = 0x00,
        UnacceptableVersion = 0x01,
        IdentifierRejected = 0x02,
        ServerUnavailable = 0x03,
        BadCredentials = 0x04,
        NotAuthorized = 0x05
    };

/**
 * Fixed header for all packets (4 bytes)
 */
#pragma pack(push, 1)
    struct PacketHeader
    {
        uint8_t type;           // PacketType
        uint8_t flags;          // QoS, retain, dup flags
        uint16_t remaining_len; // Length of variable header + payload
    };
#pragma pack(pop)

    static_assert(sizeof(PacketHeader) == 4, "PacketHeader must be 4 bytes");

    /**
     * Connect packet payload
     */
    struct ConnectPayload
    {
        std::string client_id;
        std::string username; // Optional
        std::string password; // Optional
        uint16_t keepalive;   // Keepalive interval in seconds

        std::vector<uint8_t> serialize() const;
        static ConnectPayload deserialize(const uint8_t *data, size_t len);
    };

    /**
     * Publish packet payload
     */
    struct PublishPayload
    {
        std::string topic;
        uint16_t packet_id; // For QoS > 0
        uint64_t offset;    // Message offset (0 if not available/applicable)
        std::vector<uint8_t> data;

        std::vector<uint8_t> serialize() const;
        static PublishPayload deserialize(const uint8_t *data, size_t len);
    };

    /**
     * Subscribe packet payload
     */
    struct SubscribePayload
    {
        uint16_t packet_id;
        std::vector<std::pair<std::string, QoS>> topics; // topic -> requested QoS

        std::vector<uint8_t> serialize() const;
        static SubscribePayload deserialize(const uint8_t *data, size_t len);
    };

    /**
     * FETCH_ONE packet payload (stateless offset read)
     */
    struct FetchOnePayload
    {
        std::string topic;
        uint64_t offset;

        std::vector<uint8_t> serialize() const;
        static FetchOnePayload deserialize(const uint8_t *data, size_t len);
    };

    /**
     * FETCH_RESPONSE packet payload
     */
    struct FetchResponsePayload
    {
        std::string topic;
        uint64_t offset;
        std::vector<uint8_t> data;

        std::vector<uint8_t> serialize() const;
        static FetchResponsePayload deserialize(const uint8_t *data, size_t len);
    };

    /**
     * SUBSCRIBE_FROM_OFFSET packet payload
     */
    struct SubscribeFromOffsetPayload
    {
        uint16_t packet_id;
        std::string topic;
        uint64_t start_offset;
        QoS qos;

        std::vector<uint8_t> serialize() const;
        static SubscribeFromOffsetPayload deserialize(const uint8_t *data, size_t len);
    };

    /**
     * OFFSET_NOT_FOUND error payload
     */
    struct OffsetNotFoundPayload
    {
        std::string topic;
        uint64_t requested_offset;
        uint64_t oldest_available;
        uint64_t newest_available;

        std::vector<uint8_t> serialize() const;
        static OffsetNotFoundPayload deserialize(const uint8_t *data, size_t len);
    };

    /**
     * Generic packet container
     */
    struct Packet
    {
        PacketHeader header;
        std::vector<uint8_t> payload;

        PacketType type() const { return static_cast<PacketType>(header.type); }

        std::vector<uint8_t> serialize() const;
        static Packet create(PacketType type, uint8_t flags, const std::vector<uint8_t> &payload);
    };

} // namespace highway
