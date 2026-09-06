#include <catch2/catch_test_macros.hpp>

#include "application/BrokerAddressResolver.h"
#include "support/FakeMdnsResolver.h"

TEST_CASE("resolve returns the mDNS result when the resolver finds one")
{
    FakeMdnsResolver resolver;
    resolver.result = "192.168.1.77";
    BrokerAddressResolver brokerAddressResolver(resolver);

    const std::string host = brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(host == "192.168.1.77");
}

TEST_CASE("resolve falls back to the given host when the resolver finds nothing")
{
    FakeMdnsResolver resolver;
    resolver.result = std::nullopt;
    BrokerAddressResolver brokerAddressResolver(resolver);

    const std::string host = brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(host == "192.168.1.50");
}

TEST_CASE("resolve queries the exact hostname it was given")
{
    FakeMdnsResolver resolver;
    resolver.result = "192.168.1.77";
    BrokerAddressResolver brokerAddressResolver(resolver);

    brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(resolver.lastRequestedHostname == "loco2mqtt");
}
