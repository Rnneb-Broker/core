#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace highway
{

    /**
     * Binary serialization utilities for network protocol
     */
    class BinaryWriter
    {
    public:
        void write_u8(uint8_t val)
        {
            buffer_.push_back(val);
        }

        void write_u16(uint16_t val)
        {
            buffer_.push_back(static_cast<uint8_t>(val >> 8));
            buffer_.push_back(static_cast<uint8_t>(val & 0xFF));
        }

        void write_u32(uint32_t val)
        {
            buffer_.push_back(static_cast<uint8_t>(val >> 24));
            buffer_.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
            buffer_.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
            buffer_.push_back(static_cast<uint8_t>(val & 0xFF));
        }

        void write_u64(uint64_t val)
        {
            for (int i = 7; i >= 0; --i)
            {
                buffer_.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
            }
        }

        void write_string(const std::string &str)
        {
            write_u16(static_cast<uint16_t>(str.size()));
            buffer_.insert(buffer_.end(), str.begin(), str.end());
        }

        void write_bytes(const uint8_t *data, size_t len)
        {
            buffer_.insert(buffer_.end(), data, data + len);
        }

        void write_bytes(const std::vector<uint8_t> &data)
        {
            buffer_.insert(buffer_.end(), data.begin(), data.end());
        }

        const std::vector<uint8_t> &data() const { return buffer_; }
        std::vector<uint8_t> release() { return std::move(buffer_); }

    private:
        std::vector<uint8_t> buffer_;
    };

    /**
     * Binary deserialization utilities
     */
    class BinaryReader
    {
    public:
        BinaryReader(const uint8_t *data, size_t len)
            : data_(data), len_(len), pos_(0) {}

        uint8_t read_u8()
        {
            check_remaining(1);
            return data_[pos_++];
        }

        uint16_t read_u16()
        {
            check_remaining(2);
            uint16_t val = (static_cast<uint16_t>(data_[pos_]) << 8) |
                           static_cast<uint16_t>(data_[pos_ + 1]);
            pos_ += 2;
            return val;
        }

        uint32_t read_u32()
        {
            check_remaining(4);
            uint32_t val = (static_cast<uint32_t>(data_[pos_]) << 24) |
                           (static_cast<uint32_t>(data_[pos_ + 1]) << 16) |
                           (static_cast<uint32_t>(data_[pos_ + 2]) << 8) |
                           static_cast<uint32_t>(data_[pos_ + 3]);
            pos_ += 4;
            return val;
        }

        uint64_t read_u64()
        {
            check_remaining(8);
            uint64_t val = 0;
            for (int i = 0; i < 8; ++i)
            {
                val = (val << 8) | data_[pos_ + i];
            }
            pos_ += 8;
            return val;
        }

        std::string read_string()
        {
            uint16_t len = read_u16();
            check_remaining(len);
            std::string str(reinterpret_cast<const char *>(data_ + pos_), len);
            pos_ += len;
            return str;
        }

        std::vector<uint8_t> read_bytes(size_t len)
        {
            check_remaining(len);
            std::vector<uint8_t> result(data_ + pos_, data_ + pos_ + len);
            pos_ += len;
            return result;
        }

        std::vector<uint8_t> read_remaining()
        {
            return read_bytes(remaining());
        }

        size_t remaining() const { return len_ - pos_; }
        size_t position() const { return pos_; }
        bool empty() const { return pos_ >= len_; }

    private:
        void check_remaining(size_t needed)
        {
            if (pos_ + needed > len_)
            {
                throw std::runtime_error("Buffer underflow");
            }
        }

        const uint8_t *data_;
        size_t len_;
        size_t pos_;
    };

} // namespace highway
