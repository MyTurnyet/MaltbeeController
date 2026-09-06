#include <catch2/catch_test_macros.hpp>

#include "domain/TopicScheme.h"

TEST_CASE("topicFor builds the expected command topic")
{
    REQUIRE(TopicScheme::topicFor(5) == "loconet/turnout/5/set");
}

TEST_CASE("topicFor handles a different address")
{
    REQUIRE(TopicScheme::topicFor(2048) == "loconet/turnout/2048/set");
}

TEST_CASE("stateTopicFor builds the expected state topic")
{
    REQUIRE(TopicScheme::stateTopicFor(5) == "loconet/turnout/5/state");
}

TEST_CASE("stateTopicFor differs from topicFor for the same address")
{
    REQUIRE(TopicScheme::stateTopicFor(17) != TopicScheme::topicFor(17));
}
