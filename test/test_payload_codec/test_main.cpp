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
    const TurnoutPositionLookup lookup = PayloadCodec::decode("CLOSED");

    REQUIRE(lookup.found);
    REQUIRE(lookup.position == TurnoutPosition::Closed);
}

TEST_CASE("decode THROWN produces Thrown")
{
    const TurnoutPositionLookup lookup = PayloadCodec::decode("THROWN");

    REQUIRE(lookup.found);
    REQUIRE(lookup.position == TurnoutPosition::Thrown);
}

TEST_CASE("decode an unrecognized payload produces nothing")
{
    REQUIRE_FALSE(PayloadCodec::decode("GARBAGE").found);
}
