#include <catch2/catch_test_macros.hpp>

#include "domain/IdentifyModeTimer.h"
#include "support/FakeClock.h"

namespace
{
    constexpr unsigned long DURATION_MS = 10000;
}

TEST_CASE("isActive is false before any trigger")
{
    FakeClock clock;
    IdentifyModeTimer timer(clock, DURATION_MS);

    REQUIRE_FALSE(timer.isActive());
}

TEST_CASE("isActive is true immediately after trigger")
{
    FakeClock clock;
    IdentifyModeTimer timer(clock, DURATION_MS);

    timer.trigger();

    REQUIRE(timer.isActive());
}

TEST_CASE("isActive becomes false once the duration has elapsed")
{
    FakeClock clock;
    IdentifyModeTimer timer(clock, DURATION_MS);

    timer.trigger();
    clock.advanceBy(DURATION_MS);

    REQUIRE_FALSE(timer.isActive());
}

TEST_CASE("isActive is still true just before the duration elapses")
{
    FakeClock clock;
    IdentifyModeTimer timer(clock, DURATION_MS);

    timer.trigger();
    clock.advanceBy(DURATION_MS - 1);

    REQUIRE(timer.isActive());
}

TEST_CASE("a second trigger before expiry extends the active window past the first trigger's own deadline")
{
    FakeClock clock;
    IdentifyModeTimer timer(clock, DURATION_MS);

    timer.trigger();
    clock.advanceBy(DURATION_MS - 1);
    timer.trigger();
    clock.advanceBy(DURATION_MS - 1);

    REQUIRE(timer.isActive());
}
