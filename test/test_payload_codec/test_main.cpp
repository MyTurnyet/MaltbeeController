#include <catch2/catch_test_macros.hpp>

#include "domain/PayloadCodec.h"

TEST_CASE("encode Closed produces CLOSED")
{
    REQUIRE(PayloadCodec::encode(TurnoutPosition::Closed) == "CLOSED");
}

TEST_CASE("encode Thrown produces THROWN")
{
    REQUIRE(PayloadCodec::encode(TurnoutPosition::Thrown) == "THROWN");
}

TEST_CASE("decode CLOSED produces Closed")
{
    const std::optional<TurnoutPosition> position = PayloadCodec::decode("CLOSED");

    REQUIRE(position.has_value());
    REQUIRE(*position == TurnoutPosition::Closed);
}

TEST_CASE("decode THROWN produces Thrown")
{
    const std::optional<TurnoutPosition> position = PayloadCodec::decode("THROWN");

    REQUIRE(position.has_value());
    REQUIRE(*position == TurnoutPosition::Thrown);
}

TEST_CASE("decode an unrecognized payload produces nothing")
{
    REQUIRE_FALSE(PayloadCodec::decode("GARBAGE").has_value());
}
