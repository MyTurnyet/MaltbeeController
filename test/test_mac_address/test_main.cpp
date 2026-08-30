#include <array>

#include <catch2/catch_test_macros.hpp>

#include "domain/MacAddress.h"

TEST_CASE("lastFourHexDigits formats the last two bytes as uppercase hex")
{
    const MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF});

    REQUIRE(mac.lastFourHexDigits() == "CDEF");
}

TEST_CASE("lastFourHexDigits zero-pads a byte with a high nibble of zero")
{
    const MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0x01, 0x02});

    REQUIRE(mac.lastFourHexDigits() == "0102");
}

TEST_CASE("bytes returns the exact constructed byte array")
{
    const std::array<uint8_t, 6> bytes{0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    const MacAddress mac(bytes);

    REQUIRE(mac.bytes() == bytes);
}

TEST_CASE("equal byte arrays compare equal")
{
    const MacAddress a({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    const MacAddress b({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});

    REQUIRE(a == b);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("different byte arrays compare not equal")
{
    const MacAddress a({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    const MacAddress b({0x01, 0x02, 0x03, 0x04, 0x05, 0x07});

    REQUIRE(a != b);
    REQUIRE_FALSE(a == b);
}
