#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeBootModeIndicator.h"

TEST_CASE("FakeBootModeIndicator defaults to Normal with no updates")
{
    FakeBootModeIndicator indicator;

    REQUIRE(indicator.lastShown() == BootMode::Normal);
    REQUIRE(indicator.updateCalls() == 0);
}

TEST_CASE("FakeBootModeIndicator records the last mode shown")
{
    FakeBootModeIndicator indicator;

    indicator.show(BootMode::WirelessSetup);

    REQUIRE(indicator.lastShown() == BootMode::WirelessSetup);
}

TEST_CASE("FakeBootModeIndicator counts update calls")
{
    FakeBootModeIndicator indicator;

    indicator.update();
    indicator.update();

    REQUIRE(indicator.updateCalls() == 2);
}
