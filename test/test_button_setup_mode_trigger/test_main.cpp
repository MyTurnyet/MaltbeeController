#include <catch2/catch_test_macros.hpp>

#include "adapters/ButtonSetupModeTrigger.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"

namespace
{
    constexpr unsigned long MIN_HOLD_MS = 3000;
}

TEST_CASE("requested is false initially and while the button is merely held, not yet released")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    trigger.update();
    REQUIRE_FALSE(trigger.requested());

    button.active = true;
    trigger.update();
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("requested stays false if released before minHoldMs elapses")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS - 1);
    button.active = false;
    trigger.update();

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("requested fires exactly once on the release tick after meeting minHoldMs, then resets")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS);
    button.active = false;
    trigger.update();

    REQUIRE(trigger.requested());

    trigger.update();
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("the hold survives intermediate update ticks without restarting the timer")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();

    for (int i = 0; i < 10; ++i)
    {
        clock.advanceBy(MIN_HOLD_MS / 10);
        trigger.update();
        REQUIRE_FALSE(trigger.requested());
    }

    button.active = false;
    trigger.update();

    REQUIRE(trigger.requested());
}

TEST_CASE("a fresh press-hold-release cycle after a full release can trigger again")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();
    clock.advanceBy(MIN_HOLD_MS);
    button.active = false;
    trigger.update();
    REQUIRE(trigger.requested());

    trigger.update();
    REQUIRE_FALSE(trigger.requested());

    button.active = true;
    trigger.update();
    clock.advanceBy(MIN_HOLD_MS);
    button.active = false;
    trigger.update();

    REQUIRE(trigger.requested());
}
