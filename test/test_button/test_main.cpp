#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Button.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"

namespace
{
    constexpr unsigned long DEBOUNCE_MS = 30;
}

TEST_CASE("Button begins released")
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    REQUIRE_FALSE(button.isPressed());
}

TEST_CASE("Button ignores raw input shorter than debounce period")
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    input.active = true;
    button.update();

    clock.advanceBy(DEBOUNCE_MS - 1);
    button.update();

    REQUIRE_FALSE(button.isPressed());
}

TEST_CASE("Button reports pressed after stable active input")
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    input.active = true;
    button.update();

    clock.advanceBy(DEBOUNCE_MS);
    button.update();

    REQUIRE(button.isPressed());
}

TEST_CASE("Button does not repeatedly report wasPressed while held")
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    input.active = true;
    button.update();
    clock.advanceBy(DEBOUNCE_MS);
    button.update();

    REQUIRE(button.wasPressed());
    button.update();
    REQUIRE_FALSE(button.wasPressed());
}

TEST_CASE("Button reports wasReleased once per release")
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    input.active = true;
    button.update();
    clock.advanceBy(DEBOUNCE_MS);
    button.update();
    button.wasPressed();

    input.active = false;
    button.update();
    clock.advanceBy(DEBOUNCE_MS);
    button.update();

    REQUIRE(button.wasReleased());
    button.update();
    REQUIRE_FALSE(button.wasReleased());
}
