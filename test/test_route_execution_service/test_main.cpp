#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/RouteExecutionService.h"
#include "domain/Route.h"
#include "support/FakeClock.h"
#include "support/FakeTurnoutCommandPort.h"

namespace
{
    constexpr unsigned long COMMAND_SPACING_MS = 300;
}

TEST_CASE("start sends the first command immediately")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Test Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    route.addTurnout(102, TurnoutPosition::Closed);

    service.start(route);

    REQUIRE(port.sentCommands.size() == 1);
    REQUIRE(port.sentCommands[0].address == 101);
    REQUIRE(port.sentCommands[0].position == TurnoutPosition::Thrown);
}

TEST_CASE("update does not send the second command before the spacing interval elapses")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Test Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    route.addTurnout(102, TurnoutPosition::Closed);

    service.start(route);
    clock.advanceBy(COMMAND_SPACING_MS - 1);
    service.update();

    REQUIRE(port.sentCommands.size() == 1);
}

TEST_CASE("update sends the second command once the spacing interval elapses, with the correct address and position")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Test Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    route.addTurnout(102, TurnoutPosition::Closed);

    service.start(route);
    clock.advanceBy(COMMAND_SPACING_MS);
    service.update();

    REQUIRE(port.sentCommands.size() == 2);
    REQUIRE(port.sentCommands[1].address == 102);
    REQUIRE(port.sentCommands[1].position == TurnoutPosition::Closed);
}

TEST_CASE("commands are sent in route order across a 3-command route")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Three Command Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    route.addTurnout(102, TurnoutPosition::Closed);
    route.addTurnout(103, TurnoutPosition::Thrown);

    service.start(route);
    clock.advanceBy(COMMAND_SPACING_MS);
    service.update();
    clock.advanceBy(COMMAND_SPACING_MS);
    service.update();

    REQUIRE(port.sentCommands.size() == 3);
    REQUIRE(port.sentCommands[0].address == 101);
    REQUIRE(port.sentCommands[1].address == 102);
    REQUIRE(port.sentCommands[2].address == 103);
}

TEST_CASE("isRunning is true while commands remain and false once the route completes")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Test Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    route.addTurnout(102, TurnoutPosition::Closed);

    REQUIRE_FALSE(service.isRunning());

    service.start(route);
    REQUIRE(service.isRunning());

    clock.advanceBy(COMMAND_SPACING_MS);
    service.update();
    REQUIRE_FALSE(service.isRunning());
}

TEST_CASE("isRunning is false immediately after a single-command route completes")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Single Command Route");
    route.addTurnout(101, TurnoutPosition::Thrown);

    service.start(route);

    REQUIRE_FALSE(service.isRunning());
    REQUIRE(port.sentCommands.size() == 1);
}

TEST_CASE("starting an empty route does nothing")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route emptyRoute(1, "Empty Route");

    service.start(emptyRoute);

    REQUIRE_FALSE(service.isRunning());
    REQUIRE(port.sentCommands.empty());

    service.update();
    REQUIRE(port.sentCommands.empty());
}

TEST_CASE("starting a new route while one is in progress replaces it")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route routeA(1, "Route A");
    routeA.addTurnout(101, TurnoutPosition::Thrown);
    routeA.addTurnout(102, TurnoutPosition::Thrown);

    Route routeB(2, "Route B");
    routeB.addTurnout(201, TurnoutPosition::Closed);

    service.start(routeA);
    REQUIRE(port.sentCommands.size() == 1);
    REQUIRE(port.sentCommands[0].address == 101);

    // Route A still has one command pending (address 102) - interrupt it.
    service.start(routeB);

    REQUIRE(port.sentCommands.size() == 2);
    REQUIRE(port.sentCommands[1].address == 201);
    REQUIRE_FALSE(service.isRunning());

    // Advancing time must not resurrect route A's un-sent command (102).
    clock.advanceBy(COMMAND_SPACING_MS * 2);
    service.update();
    REQUIRE(port.sentCommands.size() == 2);
}

TEST_CASE("starting an empty route while another is in progress cancels it")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route routeA(1, "Route A");
    routeA.addTurnout(101, TurnoutPosition::Thrown);
    routeA.addTurnout(102, TurnoutPosition::Thrown);

    Route emptyRoute(2, "Empty Route");

    service.start(routeA);
    REQUIRE(service.isRunning());
    REQUIRE(port.sentCommands.size() == 1);

    service.start(emptyRoute);

    REQUIRE_FALSE(service.isRunning());

    // Route A's un-sent second command (102) must never fire, even after
    // the time it would have been due.
    clock.advanceBy(COMMAND_SPACING_MS * 2);
    service.update();
    REQUIRE(port.sentCommands.size() == 1);
}
