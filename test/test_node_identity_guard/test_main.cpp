#include <catch2/catch_test_macros.hpp>

#include "domain/NodeIdentityGuard.h"

TEST_CASE("no collision is detected before any mac has been observed")
{
    NodeIdentityGuard guard("AAAA");

    REQUIRE_FALSE(guard.collisionDetected());
}

TEST_CASE("observing the panel's own mac does not trigger a collision")
{
    NodeIdentityGuard guard("AAAA");

    guard.onMacObserved("AAAA");

    REQUIRE_FALSE(guard.collisionDetected());
}

TEST_CASE("observing a different mac triggers a collision")
{
    NodeIdentityGuard guard("AAAA");

    guard.onMacObserved("BBBB");

    REQUIRE(guard.collisionDetected());
}

TEST_CASE("a detected collision does not clear when the own mac is observed again")
{
    NodeIdentityGuard guard("AAAA");

    guard.onMacObserved("BBBB");
    guard.onMacObserved("AAAA");

    REQUIRE(guard.collisionDetected());
}

TEST_CASE("observing the own mac multiple times in a row never trips a false positive")
{
    NodeIdentityGuard guard("AAAA");

    guard.onMacObserved("AAAA");
    guard.onMacObserved("AAAA");
    guard.onMacObserved("AAAA");

    REQUIRE_FALSE(guard.collisionDetected());
}
