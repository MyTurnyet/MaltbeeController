#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Route.h"
#include "domain/Turnout.h"

TEST_CASE("Route can be created", "[Route]") {
    Route route(1, "Main Line");

    REQUIRE(route.id() == 1);
    REQUIRE(route.name() == "Main Line");
}

TEST_CASE("Route has ID", "[Route]") {
    Route route(42, "Test Route");

    REQUIRE(route.id() == 42);
}

TEST_CASE("Route has name", "[Route]") {
    Route route(1, "East Yard Entry");

    REQUIRE(route.name() == "East Yard Entry");
}

TEST_CASE("Route can add turnout", "[Route]") {
    Route route(1, "Main Line");

    route.addTurnout(101, TurnoutPosition::Closed);

    REQUIRE(route.getTurnoutCount() == 1);
}

TEST_CASE("Route can add multiple turnouts", "[Route]") {
    Route route(1, "Main Line");

    route.addTurnout(101, TurnoutPosition::Closed);
    route.addTurnout(102, TurnoutPosition::Thrown);
    route.addTurnout(103, TurnoutPosition::Closed);

    REQUIRE(route.getTurnoutCount() == 3);
}

TEST_CASE("Route duplicate turnout rejected", "[Route]") {
    Route route(1, "Main Line");

    route.addTurnout(101, TurnoutPosition::Closed);
    bool added = route.addTurnout(101, TurnoutPosition::Thrown);

    REQUIRE_FALSE(added);
    REQUIRE(route.getTurnoutCount() == 1);
}

TEST_CASE("Route contains turnout", "[Route]") {
    Route route(1, "Main Line");
    route.addTurnout(101, TurnoutPosition::Closed);

    REQUIRE(route.containsTurnout(101));
}

TEST_CASE("Route does not contain unknown turnout", "[Route]") {
    Route route(1, "Main Line");
    route.addTurnout(101, TurnoutPosition::Closed);

    REQUIRE_FALSE(route.containsTurnout(999));
}

TEST_CASE("Route reports turnout position", "[Route]") {
    Route route(1, "Main Line");
    route.addTurnout(101, TurnoutPosition::Closed);
    route.addTurnout(102, TurnoutPosition::Thrown);

    auto position1 = route.getTurnoutPosition(101);
    auto position2 = route.getTurnoutPosition(102);

    REQUIRE(position1.has_value());
    REQUIRE(position1.value() == TurnoutPosition::Closed);
    REQUIRE(position2.has_value());
    REQUIRE(position2.value() == TurnoutPosition::Thrown);
}

TEST_CASE("Route unknown turnout position returns nullopt", "[Route]") {
    Route route(1, "Main Line");
    route.addTurnout(101, TurnoutPosition::Closed);

    auto position = route.getTurnoutPosition(999);

    REQUIRE_FALSE(position.has_value());
}

TEST_CASE("Route reports turnout count", "[Route]") {
    Route route(1, "Main Line");

    REQUIRE(route.getTurnoutCount() == 0);

    route.addTurnout(101, TurnoutPosition::Closed);
    REQUIRE(route.getTurnoutCount() == 1);

    route.addTurnout(102, TurnoutPosition::Thrown);
    REQUIRE(route.getTurnoutCount() == 2);
}

TEST_CASE("Empty route is valid", "[Route]") {
    Route route(1, "Empty Route");

    REQUIRE(route.getTurnoutCount() == 0);
}

TEST_CASE("Route with multiple turnouts", "[Route]") {
    Route route(1, "Complex Route");

    route.addTurnout(101, TurnoutPosition::Closed);
    route.addTurnout(102, TurnoutPosition::Thrown);
    route.addTurnout(103, TurnoutPosition::Closed);
    route.addTurnout(104, TurnoutPosition::Thrown);

    REQUIRE(route.getTurnoutCount() == 4);
    REQUIRE(route.containsTurnout(101));
    REQUIRE(route.containsTurnout(102));
    REQUIRE(route.containsTurnout(103));
    REQUIRE(route.containsTurnout(104));
}
