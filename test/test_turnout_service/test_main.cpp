#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutService.h"
#include "domain/TurnoutCollection.h"
#include "domain/Turnout.h"

TEST_CASE("TurnoutService can construct", "[TurnoutService]") {
    TurnoutService service;

    // If we get here, construction succeeded
    REQUIRE(true);
}

TEST_CASE("TurnoutService can add turnout", "[TurnoutService]") {
    TurnoutService service;
    Turnout turnout(101, "Main Yard", TurnoutPosition::Closed, false, false);

    service.addTurnout(turnout);

    // Service should now contain the turnout
    REQUIRE(true);
}

TEST_CASE("TurnoutService can throw turnout straight by address", "[TurnoutService]") {
    TurnoutService service;
    Turnout turnout(101, "Main Yard", TurnoutPosition::Thrown, false, false);
    service.addTurnout(turnout);

    TurnoutServiceResult result = service.throwStraight(101);

    REQUIRE(result == TurnoutServiceResult::Success);

    const Turnout* found = service.getTurnout(101);
    REQUIRE(found != nullptr);
    REQUIRE(found->position() == TurnoutPosition::Closed);
}

TEST_CASE("TurnoutService can throw turnout diverging by address", "[TurnoutService]") {
    TurnoutService service;
    Turnout turnout(102, "East Siding", TurnoutPosition::Closed, false, false);
    service.addTurnout(turnout);

    TurnoutServiceResult result = service.throwDiverging(102);

    REQUIRE(result == TurnoutServiceResult::Success);

    const Turnout* found = service.getTurnout(102);
    REQUIRE(found != nullptr);
    REQUIRE(found->position() == TurnoutPosition::Thrown);
}

TEST_CASE("TurnoutService can toggle turnout by address", "[TurnoutService]") {
    TurnoutService service;
    Turnout turnout(103, "West Junction", TurnoutPosition::Closed, false, false);
    service.addTurnout(turnout);

    TurnoutServiceResult result = service.toggle(103);

    REQUIRE(result == TurnoutServiceResult::Success);

    const Turnout* found = service.getTurnout(103);
    REQUIRE(found != nullptr);
    REQUIRE(found->position() == TurnoutPosition::Thrown);
}

TEST_CASE("TurnoutService toggle returns turnout to original position", "[TurnoutService]") {
    TurnoutService service;
    Turnout turnout(104, "North Switch", TurnoutPosition::Closed, false, false);
    service.addTurnout(turnout);

    service.toggle(104);
    TurnoutServiceResult result = service.toggle(104);

    REQUIRE(result == TurnoutServiceResult::Success);

    const Turnout* found = service.getTurnout(104);
    REQUIRE(found != nullptr);
    REQUIRE(found->position() == TurnoutPosition::Closed);
}

TEST_CASE("TurnoutService throw unknown turnout returns NotFound", "[TurnoutService]") {
    TurnoutService service;

    TurnoutServiceResult result = service.throwStraight(999);

    REQUIRE(result == TurnoutServiceResult::NotFound);
}

TEST_CASE("TurnoutService toggle unknown turnout returns NotFound", "[TurnoutService]") {
    TurnoutService service;

    TurnoutServiceResult result = service.toggle(999);

    REQUIRE(result == TurnoutServiceResult::NotFound);
}

TEST_CASE("TurnoutService throw locked turnout returns Locked", "[TurnoutService]") {
    TurnoutService service;
    Turnout turnout(105, "Locked Switch", TurnoutPosition::Closed, true, false);
    service.addTurnout(turnout);

    TurnoutServiceResult result = service.throwDiverging(105);

    REQUIRE(result == TurnoutServiceResult::Locked);

    const Turnout* found = service.getTurnout(105);
    REQUIRE(found != nullptr);
    REQUIRE(found->position() == TurnoutPosition::Closed); // Position unchanged
}

TEST_CASE("TurnoutService throw disabled turnout returns Disabled", "[TurnoutService]") {
    TurnoutService service;
    Turnout turnout(106, "Disabled Switch", TurnoutPosition::Closed, false, true);
    service.addTurnout(turnout);

    TurnoutServiceResult result = service.throwStraight(106);

    REQUIRE(result == TurnoutServiceResult::Disabled);

    const Turnout* found = service.getTurnout(106);
    REQUIRE(found != nullptr);
    REQUIRE(found->position() == TurnoutPosition::Closed); // Position unchanged
}

TEST_CASE("TurnoutService toggle locked turnout returns Locked", "[TurnoutService]") {
    TurnoutService service;
    Turnout turnout(107, "Locked Toggle", TurnoutPosition::Closed, true, false);
    service.addTurnout(turnout);

    TurnoutServiceResult result = service.toggle(107);

    REQUIRE(result == TurnoutServiceResult::Locked);
}

TEST_CASE("TurnoutService toggle disabled turnout returns Disabled", "[TurnoutService]") {
    TurnoutService service;
    Turnout turnout(108, "Disabled Toggle", TurnoutPosition::Closed, false, true);
    service.addTurnout(turnout);

    TurnoutServiceResult result = service.toggle(108);

    REQUIRE(result == TurnoutServiceResult::Disabled);
}
