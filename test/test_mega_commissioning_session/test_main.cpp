#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <cstring>

#include "application/MegaCommissioningSession.h"
#include "support/FakeConfigurationRepository.h"
#include "adapters/InMemoryConfigurationRepository.h"

namespace
{
    MegaConfig makeDefaults()
    {
        MegaConfig config;
        config.stations[0] = {101, FixedString32("Turnout 1")};
        config.stations[1] = {102, FixedString32("Turnout 2")};
        config.stations[2] = {103, FixedString32("Turnout 3")};
        config.stations[3] = {104, FixedString32("Turnout 4")};
        return config;
    }
}

TEST_CASE("a fresh session with nothing saved falls back to the compile-time defaults")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    REQUIRE(session.draft().stations[0].address == 101);
    REQUIRE(strcmp(session.draft().stations[0].name.c_str(), "Turnout 1") == 0);
}

TEST_CASE("a fresh session with a valid saved config uses it instead of the defaults")
{
    InMemoryConfigurationRepository repository;
    MegaConfig saved = makeDefaults();
    saved.stations[0].address = 999;
    repository.save(saved);

    MegaCommissioningSession session(repository, makeDefaults());

    REQUIRE(session.draft().stations[0].address == 999);
}

TEST_CASE("address command stages a valid address without touching the repository")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("address 1 500");

    REQUIRE(strcmp(response.c_str(), "OK") == 0);
    REQUIRE(session.draft().stations[0].address == 500);
    REQUIRE_FALSE(repository.load().validate());
}

TEST_CASE("address command rejects an out-of-range station number")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("address 5 500");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
    REQUIRE(session.draft().stations[0].address == 101);
}

TEST_CASE("address command rejects an out-of-range address value")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("address 1 9999");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
    REQUIRE(session.draft().stations[0].address == 101);
}

TEST_CASE("name command stages a new name")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("name 2 MainYard");

    REQUIRE(strcmp(response.c_str(), "OK") == 0);
    REQUIRE(strcmp(session.draft().stations[1].name.c_str(), "MainYard") == 0);
}

TEST_CASE("name command rejects an out-of-range station number")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("name 0 MainYard");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
    REQUIRE(strcmp(session.draft().stations[0].name.c_str(), "Turnout 1") == 0);
}

TEST_CASE("save command persists the draft to the repository")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());
    session.handleLine("address 1 500");

    const FixedString32 response = session.handleLine("save");

    REQUIRE(strcmp(response.c_str(), "OK") == 0);
    REQUIRE(repository.load().stations[0].address == 500);
}

TEST_CASE("save command reports an error when the repository fails to save")
{
    FakeConfigurationRepository repository;
    repository.failNextSave = true;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("save");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
}

TEST_CASE("an unrecognized command is rejected and leaves the draft unchanged")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("frobnicate 1 2");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
    REQUIRE(session.draft().stations[0].address == 101);
}

TEST_CASE("a malformed address command (non-numeric value) is rejected and leaves the draft unchanged")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("address 1 abc");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
    REQUIRE(session.draft().stations[0].address == 101);
}
