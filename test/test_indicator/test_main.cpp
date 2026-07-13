#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/Indicator.h"
#include "support/FakeDigitalOutput.h"

TEST_CASE("Indicator begins off")
{
    FakeDigitalOutput output;
    Indicator indicator(output);

    REQUIRE_FALSE(indicator.isOn());
    REQUIRE_FALSE(output.isSet());
}

TEST_CASE("Calling on activates the output")
{
    FakeDigitalOutput output;
    Indicator indicator(output);

    indicator.on();

    REQUIRE(indicator.isOn());
    REQUIRE(output.isSet());
}

TEST_CASE("Calling off deactivates the output")
{
    FakeDigitalOutput output;
    Indicator indicator(output);

    indicator.on();
    indicator.off();

    REQUIRE_FALSE(indicator.isOn());
    REQUIRE_FALSE(output.isSet());
}

TEST_CASE("Calling set(true) activates the output")
{
    FakeDigitalOutput output;
    Indicator indicator(output);

    indicator.set(true);

    REQUIRE(indicator.isOn());
    REQUIRE(output.isSet());
}

TEST_CASE("Calling set(false) deactivates the output")
{
    FakeDigitalOutput output;
    Indicator indicator(output);

    indicator.set(true);
    indicator.set(false);

    REQUIRE_FALSE(indicator.isOn());
    REQUIRE_FALSE(output.isSet());
}

TEST_CASE("Repeated on calls are idempotent")
{
    FakeDigitalOutput output;
    Indicator indicator(output);

    indicator.on();
    indicator.on();
    indicator.on();

    REQUIRE(indicator.isOn());
    REQUIRE(output.isSet());
}

TEST_CASE("Repeated off calls are idempotent")
{
    FakeDigitalOutput output;
    Indicator indicator(output);

    indicator.off();
    indicator.off();

    REQUIRE_FALSE(indicator.isOn());
    REQUIRE_FALSE(output.isSet());
}
