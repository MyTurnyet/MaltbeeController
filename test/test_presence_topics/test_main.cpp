#include <catch2/catch_test_macros.hpp>

#include "domain/PresenceTopics.h"

TEST_CASE("statusTopic builds the panel status topic for a given node id")
{
    REQUIRE(PresenceTopics::statusTopic(5) == "panel/5/status");
}

TEST_CASE("macTopic builds the panel mac topic for a given node id")
{
    REQUIRE(PresenceTopics::macTopic(5) == "panel/5/mac");
}

TEST_CASE("both topics use the node id's decimal string form for a multi-digit id")
{
    REQUIRE(PresenceTopics::statusTopic(42) == "panel/42/status");
    REQUIRE(PresenceTopics::macTopic(42) == "panel/42/mac");
}
