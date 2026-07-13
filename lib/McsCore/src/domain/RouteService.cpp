#include "RouteService.h"

RouteService::RouteService(TurnoutService& service)
    : turnoutService(service) {
}

bool RouteService::addRoute(const Route& route) {
    auto result = routes.insert({route.id(), route});
    return result.second;
}

const Route* RouteService::getRoute(int id) const {
    auto it = routes.find(id);
    if (it != routes.end()) {
        return &it->second;
    }
    return nullptr;
}

RouteActivationResult RouteService::activateRoute(int routeId) {
    const Route* route = getRoute(routeId);

    if (route == nullptr) {
        return RouteActivationResult::RouteNotFound;
    }

    const auto& turnouts = route->getTurnouts();

    if (turnouts.empty()) {
        return RouteActivationResult::Success;
    }

    bool hadMissingTurnout = false;

    for (const auto& [address, position] : turnouts) {
        TurnoutServiceResult result;

        if (position == TurnoutPosition::Closed) {
            result = turnoutService.throwStraight(address);
        } else {
            result = turnoutService.throwDiverging(address);
        }

        if (result == TurnoutServiceResult::NotFound) {
            hadMissingTurnout = true;
            continue;
        }

        if (result == TurnoutServiceResult::Locked) {
            return RouteActivationResult::TurnoutLocked;
        }

        if (result == TurnoutServiceResult::Disabled) {
            return RouteActivationResult::TurnoutDisabled;
        }
    }

    if (hadMissingTurnout) {
        return RouteActivationResult::PartialSuccess;
    }

    return RouteActivationResult::Success;
}
