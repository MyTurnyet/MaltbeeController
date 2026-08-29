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

TEST_CASE("stateTopicFor builds the expected state-suffixed topic")
{
    REQUIRE(TopicScheme::stateTopicFor("LT5") == "track/turnout/LT5/state");
}

TEST_CASE("stateTopicFor differs from topicFor for the same name")
{
    REQUIRE(TopicScheme::stateTopicFor("LT1") != TopicScheme::topicFor("LT1"));
}

TEST_CASE("stateTopicFor handles an empty name")
{
    REQUIRE(TopicScheme::stateTopicFor("") == "track/turnout//state");
}
