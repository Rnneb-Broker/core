#include <gtest/gtest.h>
#include "broker/storage_manager.hpp"
#include <random>

using namespace highway;

class BufferTest : public ::testing::Test
{
protected:
    Buffer buffer{16 * 1024 * 1024}; // 16MB for tests
};

TEST_F(BufferTest, AppendMessageBasic)
{
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};

    bool success = buffer.append_message(0, payload);
    EXPECT_TRUE(success);
    EXPECT_EQ(buffer.size(), sizeof(MessageHeader) + payload.size());
}

TEST_F(BufferTest, AppendMultipleMessages)
{
    std::vector<uint8_t> payload1 = {0x01, 0x02};
    std::vector<uint8_t> payload2 = {0x03, 0x04, 0x05};
    std::vector<uint8_t> payload3 = {0x06};

    EXPECT_TRUE(buffer.append_message(0, payload1));
    size_t size_after_1 = buffer.size();

    EXPECT_TRUE(buffer.append_message(1, payload2));
    size_t size_after_2 = buffer.size();

    EXPECT_TRUE(buffer.append_message(2, payload3));
    size_t size_after_3 = buffer.size();

    EXPECT_EQ(size_after_1, sizeof(MessageHeader) + 2);
    EXPECT_EQ(size_after_2, size_after_1 + sizeof(MessageHeader) + 3);
    EXPECT_EQ(size_after_3, size_after_2 + sizeof(MessageHeader) + 1);
}

TEST_F(BufferTest, BufferFillsAndRejects)
{
    Buffer small_buffer(100); // Very small buffer
    std::vector<uint8_t> payload(32);

    // Should succeed
    EXPECT_TRUE(small_buffer.append_message(0, payload));

    // Should fail (almost full)
    EXPECT_FALSE(small_buffer.append_message(1, payload));

    // is_full should return true
    EXPECT_TRUE(small_buffer.is_full());
}

TEST_F(BufferTest, ClearBuffer)
{
    std::vector<uint8_t> payload = {0x01, 0x02};
    EXPECT_TRUE(buffer.append_message(0, payload));
    EXPECT_GT(buffer.size(), 0);

    buffer.clear();
    EXPECT_EQ(buffer.size(), 0);

    // Should be able to append again
    EXPECT_TRUE(buffer.append_message(1, payload));
    EXPECT_GT(buffer.size(), 0);
}

TEST_F(BufferTest, LargePayload)
{
    std::vector<uint8_t> large_payload(1024 * 1024); // 1MB
    std::fill(large_payload.begin(), large_payload.end(), 0xAB);

    EXPECT_TRUE(buffer.append_message(0, large_payload));
    EXPECT_EQ(buffer.size(), sizeof(MessageHeader) + large_payload.size());
}

TEST_F(BufferTest, OffsetMonotonicity)
{
    std::vector<uint8_t> payload = {0x01};

    for (uint64_t i = 0; i < 100; ++i)
    {
        EXPECT_TRUE(buffer.append_message(i, payload));
    }

    // Verify all messages are in buffer
    EXPECT_EQ(buffer.size(), 100 * (sizeof(MessageHeader) + 1));
}

TEST_F(BufferTest, RemainingCapacity)
{
    size_t initial_capacity = buffer.remaining_capacity();
    std::vector<uint8_t> payload(1024);

    buffer.append_message(0, payload);
    size_t remaining_after = buffer.remaining_capacity();

    EXPECT_LT(remaining_after, initial_capacity);
    EXPECT_EQ(remaining_after, initial_capacity - sizeof(MessageHeader) - 1024);
}

// ============================================================================
// Sparse Index Tests
// ============================================================================

class SparseIndexTest : public ::testing::Test
{
protected:
    SparseIndex index;
};

TEST_F(SparseIndexTest, AddAndFind)
{
    index.add(0, 0);
    index.add(1024, 1024);
    index.add(2048, 2048);

    EXPECT_EQ(index.size(), 3);

    // Should find position for existing offset
    auto pos = index.find_file_position(1024);
    EXPECT_TRUE(pos.has_value());
    EXPECT_EQ(pos.value(), 1024);
}

TEST_F(SparseIndexTest, BinarySearchLowerBound)
{
    index.add(0, 0);
    index.add(1024, 1024);
    index.add(2048, 2048);
    index.add(4096, 4096);

    // Should find lower bound
    auto pos = index.find_file_position(1500);
    EXPECT_TRUE(pos.has_value());
    EXPECT_EQ(pos.value(), 1024); // Should point to offset 1024

    auto pos2 = index.find_file_position(3000);
    EXPECT_TRUE(pos2.has_value());
    EXPECT_EQ(pos2.value(), 2048); // Should point to offset 2048
}

TEST_F(SparseIndexTest, EmptyIndex)
{
    auto pos = index.find_file_position(100);
    EXPECT_TRUE(pos.has_value());
    EXPECT_EQ(pos.value(), 0); // Empty index returns 0
}

TEST_F(SparseIndexTest, Clear)
{
    index.add(0, 0);
    index.add(1024, 1024);
    EXPECT_EQ(index.size(), 2);

    index.clear();
    EXPECT_EQ(index.size(), 0);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(BufferTest, StressAppend)
{
    std::vector<uint8_t> payload(256);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    // Fill buffer with random data
    int count = 0;
    for (uint64_t i = 0; i < 100000; ++i)
    {
        std::generate(payload.begin(), payload.end(), [&] { return dist(rng); });

        if (buffer.append_message(i, payload))
        {
            count++;
        }
        else
        {
            break; // Buffer full
        }
    }

    EXPECT_GT(count, 1000); // Should fit at least 1000 messages of 256 bytes
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
