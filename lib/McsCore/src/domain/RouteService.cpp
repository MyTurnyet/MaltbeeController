#include "RouteService.h"

RouteService::RouteService(TurnoutService& service)
    : turnoutService(service) {
}

bool RouteService::addRoute(const Route& route) {
    if (getRoute(route.id()) != nullptr) {
        return false;
    }
    if (count_ >= MAX_ROUTES) {
        return false;
    }
    routes_[count_] = route;
    ++count_;
    return true;
}

const Route* RouteService::getRoute(int id) const {
    for (int i = 0; i < count_; ++i) {
        if (routes_[i].id() == id) {
            return &routes_[i];
        }
    }
    return nullptr;
}

RouteActivationResult RouteService::activateRoute(int routeId) {
    const Route* route = getRoute(routeId);

    if (route == nullptr) {
        return RouteActivationResult::RouteNotFound;
    }

    if (route->getTurnoutCount() == 0) {
        return RouteActivationResult::Success;
    }

    bool hadMissingTurnout = false;

    for (int i = 0; i < route->getTurnoutCount(); ++i) {
        TurnoutCommand cmd = route->commandAt(i);
        TurnoutServiceResult result;

        if (cmd.position == TurnoutPosition::Closed) {
            result = turnoutService.throwStraight(cmd.address);
        } else {
            result = turnoutService.throwDiverging(cmd.address);
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
