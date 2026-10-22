#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/EitherActiveDigitalInput.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("both inputs inactive is inactive")
{
    FakeDigitalInput first;
    FakeDigitalInput second;
    EitherActiveDigitalInput composite(first, second);

    REQUIRE_FALSE(composite.isActive());
}

TEST_CASE("first input active, second inactive is active")
{
    FakeDigitalInput first;
    FakeDigitalInput second;
    first.active = true;
    EitherActiveDigitalInput composite(first, second);

    REQUIRE(composite.isActive());
}

TEST_CASE("first input inactive, second active is active")
{
    FakeDigitalInput first;
    FakeDigitalInput second;
    second.active = true;
    EitherActiveDigitalInput composite(first, second);

    REQUIRE(composite.isActive());
}

TEST_CASE("both inputs active is active")
{
    FakeDigitalInput first;
    FakeDigitalInput second;
    first.active = true;
    second.active = true;
    EitherActiveDigitalInput composite(first, second);

    REQUIRE(composite.isActive());
}

TEST_CASE("reads the composed inputs live rather than latching at construction")
{
    FakeDigitalInput first;
    FakeDigitalInput second;
    EitherActiveDigitalInput composite(first, second);

    REQUIRE_FALSE(composite.isActive());

    second.active = true;

    REQUIRE(composite.isActive());
}
