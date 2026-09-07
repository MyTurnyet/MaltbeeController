#include <catch2/catch_test_macros.hpp>

#include "application/MqttPresenceAnnouncer.h"
#include "support/FakeClock.h"
#include "support/FakeMqttTransport.h"

TEST_CASE("update does not publish anything while never connected")
{
    FakeMqttTransport transport;
    FakeClock clock;
    MqttPresenceAnnouncer announcer(transport, clock, 5, "AAAA");

    announcer.update(false);

    REQUIRE(transport.published.empty());
}

TEST_CASE("update publishes online status and mac on the connect edge, not retained")
{
    FakeMqttTransport transport;
    FakeClock clock;
    MqttPresenceAnnouncer announcer(transport, clock, 5, "AAAA");

    announcer.update(true);

    REQUIRE(transport.published.size() == 2);
    REQUIRE(transport.published[0].topic == "panel/5/status");
    REQUIRE(transport.published[0].payload == "online");
    REQUIRE_FALSE(transport.published[0].retained);
    REQUIRE(transport.published[1].topic == "panel/5/mac");
    REQUIRE(transport.published[1].payload == "AAAA");
    REQUIRE_FALSE(transport.published[1].retained);
}

TEST_CASE("update does not re-publish before the heartbeat interval elapses")
{
    FakeMqttTransport transport;
    FakeClock clock;
    MqttPresenceAnnouncer announcer(transport, clock, 5, "AAAA");

    announcer.update(true);
    clock.advanceBy(MqttPresenceAnnouncer::kHeartbeatIntervalMs - 1);
    announcer.update(true);
    announcer.update(true);

    REQUIRE(transport.published.size() == 2);
}

TEST_CASE("update re-publishes once the heartbeat interval elapses while still connected")
{
    FakeMqttTransport transport;
    FakeClock clock;
    MqttPresenceAnnouncer announcer(transport, clock, 5, "AAAA");

    announcer.update(true);
    clock.advanceBy(MqttPresenceAnnouncer::kHeartbeatIntervalMs);
    announcer.update(true);

    REQUIRE(transport.published.size() == 4);
    REQUIRE(transport.published[2].topic == "panel/5/status");
    REQUIRE(transport.published[3].topic == "panel/5/mac");
}

TEST_CASE("update re-announces immediately after a disconnect and reconnect cycle")
{
    FakeMqttTransport transport;
    FakeClock clock;
    MqttPresenceAnnouncer announcer(transport, clock, 5, "AAAA");

    announcer.update(true);
    announcer.update(false);
    announcer.update(true);

    REQUIRE(transport.published.size() == 4);
    REQUIRE(transport.published[2].topic == "panel/5/status");
    REQUIRE(transport.published[3].topic == "panel/5/mac");
}

TEST_CASE("a heartbeat publish rearms the interval rather than firing every tick")
{
    FakeMqttTransport transport;
    FakeClock clock;
    MqttPresenceAnnouncer announcer(transport, clock, 5, "AAAA");

    announcer.update(true);
    clock.advanceBy(MqttPresenceAnnouncer::kHeartbeatIntervalMs);
    announcer.update(true);
    clock.advanceBy(1);
    announcer.update(true);
    announcer.update(true);

    REQUIRE(transport.published.size() == 4);
}
