#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/MrrwaLocoNetTurnoutAdapter.h"
#include "support/FakeLocoNetTransport.h"

TEST_CASE("Closed command creates the expected LocoNet message")
{
    FakeLocoNetTransport transport;
    MrrwaLocoNetTurnoutAdapter adapter(transport);

    adapter.send(101, TurnoutPosition::Closed);

    REQUIRE(transport.sentPackets.size() == 1);
    REQUIRE(transport.sentPackets[0].address == 101);
    REQUIRE(transport.sentPackets[0].position == TurnoutPosition::Closed);
}

TEST_CASE("Thrown command creates the expected LocoNet message")
{
    FakeLocoNetTransport transport;
    MrrwaLocoNetTurnoutAdapter adapter(transport);

    adapter.send(202, TurnoutPosition::Thrown);

    REQUIRE(transport.sentPackets.size() == 1);
    REQUIRE(transport.sentPackets[0].address == 202);
    REQUIRE(transport.sentPackets[0].position == TurnoutPosition::Thrown);
}

TEST_CASE("Sending multiple commands records each packet in order")
{
    FakeLocoNetTransport transport;
    MrrwaLocoNetTurnoutAdapter adapter(transport);

    adapter.send(101, TurnoutPosition::Thrown);
    adapter.send(202, TurnoutPosition::Closed);

    REQUIRE(transport.sentPackets.size() == 2);
    REQUIRE(transport.sentPackets[0].address == 101);
    REQUIRE(transport.sentPackets[0].position == TurnoutPosition::Thrown);
    REQUIRE(transport.sentPackets[1].address == 202);
    REQUIRE(transport.sentPackets[1].position == TurnoutPosition::Closed);
}
