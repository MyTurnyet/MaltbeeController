#ifndef ROUTESERVICE_H
#define ROUTESERVICE_H

#include "Route.h"
#include "TurnoutService.h"

enum class RouteActivationResult {
    Success,
    RouteNotFound,
    TurnoutLocked,
    TurnoutDisabled,
    PartialSuccess
};

class RouteService {
private:
    static constexpr int MAX_ROUTES = 32;

    TurnoutService& turnoutService;
    Route routes_[MAX_ROUTES] = {};
    int count_ = 0;

public:
    explicit RouteService(TurnoutService& service);

    bool addRoute(const Route& route);
    const Route* getRoute(int id) const;
    RouteActivationResult activateRoute(int routeId);
};

#endif // ROUTESERVICE_H
