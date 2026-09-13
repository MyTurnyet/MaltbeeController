#include <catch2/catch_test_macros.hpp>

#include "application/BrokerAddressResolver.h"
#include "support/FakeClock.h"
#include "support/FakeMdnsResolver.h"

namespace
{
    constexpr unsigned long kThrottleMs = 60000;
}

TEST_CASE("resolve returns the mDNS result when the resolver finds one")
{
    FakeMdnsResolver resolver;
    resolver.result = "192.168.1.77";
    FakeClock clock;
    BrokerAddressResolver brokerAddressResolver(resolver, clock, kThrottleMs);

    const std::string host = brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(host == "192.168.1.77");
}

TEST_CASE("resolve falls back to the given host when the resolver finds nothing")
{
    FakeMdnsResolver resolver;
    resolver.result = std::nullopt;
    FakeClock clock;
    BrokerAddressResolver brokerAddressResolver(resolver, clock, kThrottleMs);

    const std::string host = brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(host == "192.168.1.50");
}

TEST_CASE("resolve queries the exact hostname it was given")
{
    FakeMdnsResolver resolver;
    resolver.result = "192.168.1.77";
    FakeClock clock;
    BrokerAddressResolver brokerAddressResolver(resolver, clock, kThrottleMs);

    brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(resolver.lastRequestedHostname == "loco2mqtt");
}

TEST_CASE("resolve queries the resolver on the very first call")
{
    FakeMdnsResolver resolver;
    resolver.result = "192.168.1.77";
    FakeClock clock;
    BrokerAddressResolver brokerAddressResolver(resolver, clock, kThrottleMs);

    resolver.lastRequestedHostname.clear();
    brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(resolver.lastRequestedHostname == "loco2mqtt");
}

TEST_CASE("resolve does not re-query the resolver before the throttle interval elapses")
{
    FakeMdnsResolver resolver;
    resolver.result = "192.168.1.77";
    FakeClock clock;
    BrokerAddressResolver brokerAddressResolver(resolver, clock, kThrottleMs);
    brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    resolver.lastRequestedHostname.clear();
    resolver.result = "192.168.1.99";
    clock.advanceBy(kThrottleMs - 1);
    const std::string host = brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(resolver.lastRequestedHostname.empty());
    REQUIRE(host == "192.168.1.77");
}

TEST_CASE("resolve re-queries the resolver once the throttle interval elapses")
{
    FakeMdnsResolver resolver;
    resolver.result = "192.168.1.77";
    FakeClock clock;
    BrokerAddressResolver brokerAddressResolver(resolver, clock, kThrottleMs);
    brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    resolver.result = "192.168.1.99";
    clock.advanceBy(kThrottleMs);
    const std::string host = brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(resolver.lastRequestedHostname == "loco2mqtt");
    REQUIRE(host == "192.168.1.99");
}

TEST_CASE("a throttled call still uses the caller's current fallback host when mDNS has never resolved")
{
    FakeMdnsResolver resolver;
    resolver.result = std::nullopt;
    FakeClock clock;
    BrokerAddressResolver brokerAddressResolver(resolver, clock, kThrottleMs);
    brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    clock.advanceBy(kThrottleMs - 1);
    const std::string host = brokerAddressResolver.resolve("loco2mqtt", "192.168.1.66");

    REQUIRE(host == "192.168.1.66");
}
