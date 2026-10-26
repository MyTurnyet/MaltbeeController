#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/MegaConfig.h"

TEST_CASE("a config with all addresses in range and no duplicates is valid")
{
    MegaConfig config;
    config.stations[0] = {101, FixedString32("Turnout 1")};
    config.stations[1] = {102, FixedString32("Turnout 2")};
    config.stations[2] = {103, FixedString32("Turnout 3")};
    config.stations[3] = {104, FixedString32("Turnout 4")};

    REQUIRE(config.validate());
}

TEST_CASE("an address below the minimum is invalid")
{
    MegaConfig config;
    config.stations[0] = {101, FixedString32("Turnout 1")};
    config.stations[1] = {0, FixedString32("Turnout 2")};
    config.stations[2] = {103, FixedString32("Turnout 3")};
    config.stations[3] = {104, FixedString32("Turnout 4")};

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("an address above the maximum is invalid")
{
    MegaConfig config;
    config.stations[0] = {101, FixedString32("Turnout 1")};
    config.stations[1] = {102, FixedString32("Turnout 2")};
    config.stations[2] = {103, FixedString32("Turnout 3")};
    config.stations[3] = {2049, FixedString32("Turnout 4")};

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("the minimum and maximum address bounds are themselves valid")
{
    MegaConfig config;
    config.stations[0] = {1, FixedString32("Turnout 1")};
    config.stations[1] = {2048, FixedString32("Turnout 2")};
    config.stations[2] = {103, FixedString32("Turnout 3")};
    config.stations[3] = {104, FixedString32("Turnout 4")};

    REQUIRE(config.validate());
}

TEST_CASE("two stations sharing the same address is invalid")
{
    MegaConfig config;
    config.stations[0] = {101, FixedString32("Turnout 1")};
    config.stations[1] = {101, FixedString32("Turnout 2")};
    config.stations[2] = {103, FixedString32("Turnout 3")};
    config.stations[3] = {104, FixedString32("Turnout 4")};

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("a default-constructed config (all-zero addresses) is invalid")
{
    const MegaConfig config;

    REQUIRE_FALSE(config.validate());
}
