#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/InMemoryConfigurationRepository.h"
#include <cstring>

TEST_CASE("load before any save returns a default-constructed, invalid config")
{
    InMemoryConfigurationRepository repository;

    const MegaConfig config = repository.load();

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("save then load round-trips the exact config")
{
    InMemoryConfigurationRepository repository;
    MegaConfig original;
    original.stations[0] = {101, FixedString32("Turnout 1")};
    original.stations[1] = {102, FixedString32("Turnout 2")};
    original.stations[2] = {103, FixedString32("Turnout 3")};
    original.stations[3] = {104, FixedString32("Turnout 4")};

    REQUIRE(repository.save(original));
    const MegaConfig loaded = repository.load();

    REQUIRE(loaded.stations[0].address == 101);
    REQUIRE(strcmp(loaded.stations[0].name.c_str(), "Turnout 1") == 0);
    REQUIRE(loaded.stations[3].address == 104);
    REQUIRE(strcmp(loaded.stations[3].name.c_str(), "Turnout 4") == 0);
}

TEST_CASE("a second save overwrites the first")
{
    InMemoryConfigurationRepository repository;
    MegaConfig first;
    first.stations[0] = {101, FixedString32("First")};
    MegaConfig second;
    second.stations[0] = {201, FixedString32("Second")};

    repository.save(first);
    repository.save(second);
    const MegaConfig loaded = repository.load();

    REQUIRE(loaded.stations[0].address == 201);
}
