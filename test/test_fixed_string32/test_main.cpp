#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/FixedString32.h"

TEST_CASE("FixedString32 stores a short value and compares equal to a matching const char*")
{
    const FixedString32 value("Main Yard");

    REQUIRE(value == "Main Yard");
}

TEST_CASE("FixedString32 does not compare equal to a different value")
{
    const FixedString32 value("Main Yard");

    REQUIRE_FALSE(value == "East Siding");
}

TEST_CASE("FixedString32 default-constructs to an empty string")
{
    const FixedString32 value;

    REQUIRE(value == "");
}

TEST_CASE("FixedString32 truncates values longer than capacity")
{
    const FixedString32 value("1234567890123456789012345678901234567890");

    REQUIRE(value == "1234567890123456789012345678901");
}

