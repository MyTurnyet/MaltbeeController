#ifndef ROUTESERVICE_H
#define ROUTESERVICE_H

#include <map>
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
    TurnoutService& turnoutService;
    std::map<int, Route> routes;

public:
    explicit RouteService(TurnoutService& service);

    bool addRoute(const Route& route);
    const Route* getRoute(int id) const;
    RouteActivationResult activateRoute(int routeId);
};

#endif // ROUTESERVICE_H
