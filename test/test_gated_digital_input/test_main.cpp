#include <catch2/catch_test_macros.hpp>

#include "adapters/GatedDigitalInput.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("forwards the inner reading when not suppressed")
{
    FakeDigitalInput inner;
    GatedDigitalInput gated(inner);

    inner.active = true;
    REQUIRE(gated.isActive());

    inner.active = false;
    REQUIRE_FALSE(gated.isActive());
}

TEST_CASE("returns false when suppressed regardless of the inner reading")
{
    FakeDigitalInput inner;
    GatedDigitalInput gated(inner);

    inner.active = true;
    gated.setSuppressed(true);

    REQUIRE_FALSE(gated.isActive());
}

TEST_CASE("forwarding resumes after setSuppressed(false)")
{
    FakeDigitalInput inner;
    GatedDigitalInput gated(inner);

    inner.active = true;
    gated.setSuppressed(true);
    REQUIRE_FALSE(gated.isActive());

    gated.setSuppressed(false);
    REQUIRE(gated.isActive());
}
