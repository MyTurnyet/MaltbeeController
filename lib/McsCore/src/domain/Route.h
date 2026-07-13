#ifndef ROUTE_H
#define ROUTE_H

#include <string>
#include <map>
#include <optional>
#include "Turnout.h"

class Route {
private:
    int routeId;
    std::string routeName;
    std::map<int, TurnoutPosition> turnouts;

public:
    Route(int id, std::string name);

    [[nodiscard]] int id() const;
    [[nodiscard]] std::string name() const;

    bool addTurnout(int address, TurnoutPosition position);
    [[nodiscard]] int getTurnoutCount() const;
    [[nodiscard]] bool containsTurnout(int address) const;
    [[nodiscard]] std::optional<TurnoutPosition> getTurnoutPosition(int address) const;
    [[nodiscard]] const std::map<int, TurnoutPosition>& getTurnouts() const;
};

#endif // ROUTE_H
