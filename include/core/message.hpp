#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

enum class MessageType : uint16_t
{
    Data = 1
};

#pragma pack(push, 1)
struct MessageHeader
{
    uint8_t version{1};
    uint16_t type{0};
    uint64_t src_node_id{0};
    uint64_t dst_node_id{0};
    uint32_t size{0};
    uint16_t ttl{0};
};
#pragma pack(pop)

struct Message
{
    MessageHeader header;
    std::vector<uint8_t> payload;
};
