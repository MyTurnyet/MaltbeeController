#pragma once

#include <array>
#include <cstdint>
#include <string>

class MacAddress
{
public:
    explicit MacAddress(std::array<uint8_t, 6> bytes) : bytes_(bytes)
    {
    }

    [[nodiscard]] const std::array<uint8_t, 6>& bytes() const
    {
        return bytes_;
    }

    [[nodiscard]] std::string lastFourHexDigits() const
    {
        static const char* kHexDigits = "0123456789ABCDEF";
        std::string result;
        result += kHexDigits[(bytes_[4] >> 4) & 0x0F];
        result += kHexDigits[bytes_[4] & 0x0F];
        result += kHexDigits[(bytes_[5] >> 4) & 0x0F];
        result += kHexDigits[bytes_[5] & 0x0F];
        return result;
    }

    bool operator==(const MacAddress& other) const
    {
        return bytes_ == other.bytes_;
    }

    bool operator!=(const MacAddress& other) const
    {
        return !(*this == other);
    }

private:
    std::array<uint8_t, 6> bytes_;
};
