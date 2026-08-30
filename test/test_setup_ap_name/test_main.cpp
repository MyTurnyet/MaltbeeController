#include <catch2/catch_test_macros.hpp>

#include "domain/SetupApName.h"

TEST_CASE("from builds the expected AP name from a MAC address")
{
    const MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF});

    REQUIRE(SetupApName::from(mac) == "MaltBee-Setup-CDEF");
}
