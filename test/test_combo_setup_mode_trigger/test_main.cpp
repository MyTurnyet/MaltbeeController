#include <catch2/catch_test_macros.hpp>

#include "adapters/ComboSetupModeTrigger.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"

namespace
{
    constexpr unsigned long MIN_HOLD_MS = 3000;
}

TEST_CASE("isHolding is false initially and while only one button is active")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    trigger.update();
    REQUIRE_FALSE(trigger.isHolding());

    buttonA.active = true;
    trigger.update();
    REQUIRE_FALSE(trigger.isHolding());
}

TEST_CASE("isHolding becomes true once both buttons are simultaneously active")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();

    REQUIRE(trigger.isHolding());
}

TEST_CASE("requested stays false if released before minHoldMs elapses")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS - 1);
    buttonA.active = false;
    trigger.update();

    REQUIRE_FALSE(trigger.requested());
    REQUIRE_FALSE(trigger.isHolding());
}

TEST_CASE("requested fires exactly once on the release tick after meeting minHoldMs, then resets")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS);
    buttonA.active = false;
    buttonB.active = false;
    trigger.update();

    REQUIRE(trigger.requested());
    REQUIRE_FALSE(trigger.isHolding());

    trigger.update();
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("releasing only one of the two buttons still ends the joint hold and can fire requested")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS);
    buttonA.active = false;
    // buttonB remains active - only one released
    trigger.update();

    REQUIRE(trigger.requested());
    REQUIRE_FALSE(trigger.isHolding());
}

TEST_CASE("a fresh press-hold-release cycle after a full release can trigger again")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();
    clock.advanceBy(MIN_HOLD_MS);
    buttonA.active = false;
    buttonB.active = false;
    trigger.update();
    REQUIRE(trigger.requested());

    trigger.update();
    REQUIRE_FALSE(trigger.requested());

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();
    clock.advanceBy(MIN_HOLD_MS);
    buttonA.active = false;
    buttonB.active = false;
    trigger.update();

    REQUIRE(trigger.requested());
}
