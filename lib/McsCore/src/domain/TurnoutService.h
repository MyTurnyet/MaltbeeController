#ifndef TURNOUTSERVICE_H
#define TURNOUTSERVICE_H

#include "TurnoutCollection.h"
#include "Turnout.h"

enum class TurnoutServiceResult {
    Success,
    NotFound,
    Locked,
    Disabled
};

class TurnoutService {
private:
    TurnoutCollection collection;

public:
    TurnoutService() = default;

    void addTurnout(const Turnout& turnout);
    const Turnout* getTurnout(int address) const;

    TurnoutServiceResult throwStraight(int address);
    TurnoutServiceResult throwDiverging(int address);
    TurnoutServiceResult toggle(int address);
};

#endif // TURNOUTSERVICE_H
