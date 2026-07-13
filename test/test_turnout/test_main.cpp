#include <catch2/catch_test_macros.hpp>

#include "domain/Turnout.h"

Turnout createTurnout(TurnoutPosition defaultPosition = TurnoutPosition::Closed,
                      bool isLocked = false,
                      bool isDisabled = false) {
    int address = 9999;
    std::string name = "Turnout";

    return Turnout(address, name, defaultPosition, isLocked, isDisabled);
}

TEST_CASE("Turnout takes all needed parameters") {
    Turnout turnout = createTurnout();

    REQUIRE(turnout.position() == TurnoutPosition::Closed);
    REQUIRE(turnout.name() == "Turnout");
    REQUIRE(turnout.address() == 9999);
    REQUIRE(turnout.isDisabled() == false);
    REQUIRE(turnout.isLocked() == false);
    REQUIRE(turnout.canThrow() == true);
}

TEST_CASE("New turnout starts straight") {
    Turnout turnout = createTurnout();

    REQUIRE(
        turnout.position() == TurnoutPosition::Closed
    );
}

TEST_CASE("Turnout can throw straight") {
    Turnout turnout = createTurnout();

    turnout.throwStraight();

    REQUIRE(
        turnout.position() == TurnoutPosition::Closed
    );
}

TEST_CASE("Turnout can throw diverging") {
    Turnout turnout = createTurnout();

    turnout.throwDiverging();

    REQUIRE(
        turnout.position() == TurnoutPosition::Thrown
    );
}

TEST_CASE("Turnout toggles to diverging") {
    Turnout turnout = createTurnout();
    REQUIRE(
        turnout.position() == TurnoutPosition::Closed
    );
    turnout.toggle();

    REQUIRE(
        turnout.position() == TurnoutPosition::Thrown
    );
}

TEST_CASE("Turnout toggles to straight") {
    Turnout turnout = createTurnout(TurnoutPosition::Thrown);
    REQUIRE(
        turnout.position() == TurnoutPosition::Thrown
    );
    turnout.toggle();

    REQUIRE(
        turnout.position() == TurnoutPosition::Closed
    );
}

TEST_CASE("Turnout locks") {
    Turnout turnout = createTurnout();
    turnout.lock();
    REQUIRE(
        turnout.isLocked() == true
    );
}

TEST_CASE("Turnout unlocks") {
    Turnout turnout = createTurnout(TurnoutPosition::Closed, true);
    REQUIRE(
        turnout.isLocked() == true
    );
    turnout.unlock();
    REQUIRE(
        turnout.isLocked() == false
    );
}

TEST_CASE("Turnout can be disabled") {
    Turnout turnout = createTurnout();
    turnout.disable();
    REQUIRE(
        turnout.isDisabled() == true
    );
}

TEST_CASE("Turnout can be enabled") {
    Turnout turnout = createTurnout(TurnoutPosition::Closed, true, true);
    REQUIRE(
        turnout.isDisabled()==true
    );
    turnout.enable();
    REQUIRE(
        turnout.isDisabled() == false
    );
}

TEST_CASE("Turnout cannot throw when disabled") {
    Turnout turnout = createTurnout();
    REQUIRE(turnout.canThrow() == true);

    turnout.disable();
    REQUIRE(
        turnout.canThrow() == false
    );
}

TEST_CASE("Turnout cannot throw when locked") {
    Turnout turnout = createTurnout();
    REQUIRE(turnout.canThrow() == true);

    turnout.lock();
    REQUIRE(
        turnout.canThrow() == false
    );
}

TEST_CASE("Locked turnout cannot be thrown") {
    Turnout turnout = createTurnout(TurnoutPosition::Closed, true);
    REQUIRE(turnout.isLocked() == true);

    turnout.throwDiverging();

    REQUIRE(
        turnout.position() == TurnoutPosition::Closed
    );
}

TEST_CASE("Disabled turnout cannot be thrown") {
    Turnout turnout = createTurnout(TurnoutPosition::Closed, false, true);
    REQUIRE(turnout.isDisabled() == true);

    turnout.throwDiverging();

    REQUIRE(
        turnout.position() == TurnoutPosition::Closed
    );
}

TEST_CASE("Locked turnout cannot be toggled") {
    Turnout turnout = createTurnout(TurnoutPosition::Closed, true);
    REQUIRE(turnout.isLocked() == true);

    turnout.toggle();

    REQUIRE(
        turnout.position() == TurnoutPosition::Closed
    );
}
