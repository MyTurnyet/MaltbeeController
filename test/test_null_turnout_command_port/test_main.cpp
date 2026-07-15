#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/NullTurnoutCommandPort.h"

TEST_CASE("NullTurnoutCommandPort accepts commands without side effects")
{
    NullTurnoutCommandPort port;

    port.send(101, TurnoutPosition::Thrown);
    port.send(101, TurnoutPosition::Closed);

    SUCCEED("send() completed without throwing or crashing");
}
