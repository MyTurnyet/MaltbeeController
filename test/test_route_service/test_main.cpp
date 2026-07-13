#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/RouteService.h"
#include "domain/Route.h"
#include "domain/TurnoutService.h"
#include "domain/TurnoutCollection.h"
#include "domain/Turnout.h"

TEST_CASE("RouteService can construct", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    RouteService routeService(turnoutService);

    REQUIRE(true);
}

TEST_CASE("RouteService can add route", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    RouteService routeService(turnoutService);
    Route route(1, "Main Line");

    routeService.addRoute(route);

    const Route* found = routeService.getRoute(1);
    REQUIRE(found != nullptr);
    REQUIRE(found->id() == 1);
    REQUIRE(found->name() == "Main Line");
}

TEST_CASE("RouteService can find route", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    RouteService routeService(turnoutService);
    Route route1(1, "Route 1");
    Route route2(2, "Route 2");

    routeService.addRoute(route1);
    routeService.addRoute(route2);

    const Route* found = routeService.getRoute(2);
    REQUIRE(found != nullptr);
    REQUIRE(found->id() == 2);
}

TEST_CASE("RouteService returns nullptr for unknown route", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    RouteService routeService(turnoutService);

    const Route* found = routeService.getRoute(999);

    REQUIRE(found == nullptr);
}

TEST_CASE("RouteService duplicate route rejected", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    RouteService routeService(turnoutService);
    Route route1(1, "First");
    Route route2(1, "Second");

    routeService.addRoute(route1);
    bool added = routeService.addRoute(route2);

    REQUIRE_FALSE(added);

    const Route* found = routeService.getRoute(1);
    REQUIRE(found != nullptr);
    REQUIRE(found->name() == "First");
}

TEST_CASE("RouteService activate route changes required turnouts", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    Turnout turnout1(101, "Switch 1", TurnoutPosition::Thrown, false, false);
    Turnout turnout2(102, "Switch 2", TurnoutPosition::Thrown, false, false);
    turnoutService.addTurnout(turnout1);
    turnoutService.addTurnout(turnout2);

    RouteService routeService(turnoutService);
    Route route(1, "Main Line");
    route.addTurnout(101, TurnoutPosition::Closed);
    route.addTurnout(102, TurnoutPosition::Closed);
    routeService.addRoute(route);

    RouteActivationResult result = routeService.activateRoute(1);

    REQUIRE(result == RouteActivationResult::Success);

    const Turnout* t1 = turnoutService.getTurnout(101);
    const Turnout* t2 = turnoutService.getTurnout(102);
    REQUIRE(t1->position() == TurnoutPosition::Closed);
    REQUIRE(t2->position() == TurnoutPosition::Closed);
}

TEST_CASE("RouteService activate unknown route fails", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    RouteService routeService(turnoutService);

    RouteActivationResult result = routeService.activateRoute(999);

    REQUIRE(result == RouteActivationResult::RouteNotFound);
}

TEST_CASE("RouteService activation skips missing turnout", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    Turnout turnout1(101, "Switch 1", TurnoutPosition::Thrown, false, false);
    turnoutService.addTurnout(turnout1);

    RouteService routeService(turnoutService);
    Route route(1, "Partial Route");
    route.addTurnout(101, TurnoutPosition::Closed);
    route.addTurnout(999, TurnoutPosition::Closed); // Missing turnout
    routeService.addRoute(route);

    RouteActivationResult result = routeService.activateRoute(1);

    REQUIRE(result == RouteActivationResult::PartialSuccess);

    const Turnout* t1 = turnoutService.getTurnout(101);
    REQUIRE(t1->position() == TurnoutPosition::Closed);
}

TEST_CASE("RouteService activation reports success", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    Turnout turnout(101, "Switch", TurnoutPosition::Thrown, false, false);
    turnoutService.addTurnout(turnout);

    RouteService routeService(turnoutService);
    Route route(1, "Test Route");
    route.addTurnout(101, TurnoutPosition::Closed);
    routeService.addRoute(route);

    RouteActivationResult result = routeService.activateRoute(1);

    REQUIRE(result == RouteActivationResult::Success);
}

TEST_CASE("RouteService activation fails when turnout is locked", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    Turnout turnout(101, "Locked Switch", TurnoutPosition::Closed, true, false);
    turnoutService.addTurnout(turnout);

    RouteService routeService(turnoutService);
    Route route(1, "Test Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    routeService.addRoute(route);

    RouteActivationResult result = routeService.activateRoute(1);

    REQUIRE(result == RouteActivationResult::TurnoutLocked);
}

TEST_CASE("RouteService activation fails when turnout is disabled", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    Turnout turnout(101, "Disabled Switch", TurnoutPosition::Closed, false, true);
    turnoutService.addTurnout(turnout);

    RouteService routeService(turnoutService);
    Route route(1, "Test Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    routeService.addRoute(route);

    RouteActivationResult result = routeService.activateRoute(1);

    REQUIRE(result == RouteActivationResult::TurnoutDisabled);
}

TEST_CASE("RouteService can activate empty route", "[RouteService]") {
    TurnoutCollection collection;
    TurnoutService turnoutService(collection);
    RouteService routeService(turnoutService);
    Route route(1, "Empty Route");
    routeService.addRoute(route);

    RouteActivationResult result = routeService.activateRoute(1);

    REQUIRE(result == RouteActivationResult::Success);
}
