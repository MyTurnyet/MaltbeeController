#ifndef ROUTE_H
#define ROUTE_H

#include "FixedString32.h"
#include "Turnout.h"

struct TurnoutCommand
{
    int address;
    TurnoutPosition position;
};

struct TurnoutPositionLookup
{
    bool found;
    TurnoutPosition position;
};

class Route {
private:
    static constexpr int MAX_TURNOUT_COMMANDS_PER_ROUTE = 64;

    int routeId = 0;
    FixedString32 routeName;
    TurnoutCommand commands_[MAX_TURNOUT_COMMANDS_PER_ROUTE] = {};
    int commandCount_ = 0;

public:
    Route() = default;
    Route(int id, const char* name);

    [[nodiscard]] int id() const;
    [[nodiscard]] FixedString32 name() const;

    bool addTurnout(int address, TurnoutPosition position);
    [[nodiscard]] int getTurnoutCount() const;
    [[nodiscard]] bool containsTurnout(int address) const;
    [[nodiscard]] TurnoutPositionLookup getTurnoutPosition(int address) const;
    [[nodiscard]] TurnoutCommand commandAt(int index) const;
};

#endif // ROUTE_H
