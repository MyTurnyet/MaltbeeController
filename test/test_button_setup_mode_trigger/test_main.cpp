#include <catch2/catch_test_macros.hpp>

#include "adapters/ButtonSetupModeTrigger.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"

namespace
{
    constexpr unsigned long MIN_HOLD_MS = 3000;
    constexpr unsigned long RELEASE_SETTLE_MS = 50;
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

TEST_CASE("requested stays false if released before minHoldMs elapses, even once the release settles")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS - 1);
    button.active = false;
    trigger.update();

    clock.advanceBy(RELEASE_SETTLE_MS);
    trigger.update();

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("requested does not fire on the immediate release tick - it waits for the release to settle")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS);
    button.active = false;
    trigger.update();

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("requested fires once the release has settled for RELEASE_SETTLE_MS, then resets")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS);
    button.active = false;
    trigger.update();

    clock.advanceBy(RELEASE_SETTLE_MS);
    trigger.update();

    REQUIRE(trigger.requested());

    trigger.update();
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("a bounce back to active during the settle window cancels the pending release and the hold continues")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS);
    button.active = false;
    trigger.update();

    clock.advanceBy(RELEASE_SETTLE_MS - 1);
    button.active = true;
    trigger.update();

    REQUIRE_FALSE(trigger.requested());

    // A clean release now still counts from the ORIGINAL press - the bounce
    // must not have reset holdStartMs_, since the finger never actually left
    // the button for any meaningful duration.
    button.active = false;
    trigger.update();
    clock.advanceBy(RELEASE_SETTLE_MS);
    trigger.update();

    REQUIRE(trigger.requested());
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
    clock.advanceBy(RELEASE_SETTLE_MS);
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
    clock.advanceBy(RELEASE_SETTLE_MS);
    trigger.update();
    REQUIRE(trigger.requested());

    trigger.update();
    REQUIRE_FALSE(trigger.requested());

    button.active = true;
    trigger.update();
    clock.advanceBy(MIN_HOLD_MS);
    button.active = false;
    trigger.update();
    clock.advanceBy(RELEASE_SETTLE_MS);
    trigger.update();

    REQUIRE(trigger.requested());
}
