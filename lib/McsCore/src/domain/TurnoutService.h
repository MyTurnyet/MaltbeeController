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
    TurnoutCollection& collection_;

    // Validation helper to eliminate duplication
    TurnoutServiceResult validateTurnout(Turnout* turnout) const;

public:
    TurnoutService(TurnoutCollection& collection);

    void addTurnout(const Turnout& turnout);
    const Turnout* getTurnout(int address) const;

    TurnoutServiceResult throwStraight(int address);
    TurnoutServiceResult throwDiverging(int address);
    TurnoutServiceResult toggle(int address);
};

#endif // TURNOUTSERVICE_H
