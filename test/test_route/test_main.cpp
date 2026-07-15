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

    REQUIRE(position1.found);
    REQUIRE(position1.position == TurnoutPosition::Closed);
    REQUIRE(position2.found);
    REQUIRE(position2.position == TurnoutPosition::Thrown);
}

TEST_CASE("Route unknown turnout position returns not found", "[Route]") {
    Route route(1, "Main Line");
    route.addTurnout(101, TurnoutPosition::Closed);

    auto position = route.getTurnoutPosition(999);

    REQUIRE_FALSE(position.found);
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

TEST_CASE("Default-constructed route has id 0, empty name, and zero commands", "[Route]") {
    Route route;

    REQUIRE(route.id() == 0);
    REQUIRE(route.name() == "");
    REQUIRE(route.getTurnoutCount() == 0);
}

TEST_CASE("Route rejects adding a turnout beyond capacity", "[Route]") {
    Route route(1, "Full Route");

    for (int i = 0; i < 64; ++i) {
        REQUIRE(route.addTurnout(1000 + i, TurnoutPosition::Closed));
    }
    REQUIRE(route.getTurnoutCount() == 64);

    bool added = route.addTurnout(9999, TurnoutPosition::Thrown);

    REQUIRE_FALSE(added);
    REQUIRE(route.getTurnoutCount() == 64);
}

TEST_CASE("Route commandAt returns the turnout command at an index", "[Route]") {
    Route route(1, "Main Line");
    route.addTurnout(101, TurnoutPosition::Closed);
    route.addTurnout(102, TurnoutPosition::Thrown);

    TurnoutCommand first = route.commandAt(0);
    TurnoutCommand second = route.commandAt(1);

    REQUIRE(first.address == 101);
    REQUIRE(first.position == TurnoutPosition::Closed);
    REQUIRE(second.address == 102);
    REQUIRE(second.position == TurnoutPosition::Thrown);
}
