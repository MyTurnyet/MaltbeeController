#include <catch2/catch_test_macros.hpp>

#include "domain/TopicScheme.h"

TEST_CASE("topicFor builds the expected prefixed topic")
{
    REQUIRE(TopicScheme::topicFor("LT5") == "track/turnout/LT5");
}

TEST_CASE("topicFor handles a different name")
{
    REQUIRE(TopicScheme::topicFor("Yard Ladder 2") == "track/turnout/Yard Ladder 2");
}

TEST_CASE("topicFor handles an empty name")
{
    REQUIRE(TopicScheme::topicFor("") == "track/turnout/");
}
