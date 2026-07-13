#include "TurnoutService.h"

void TurnoutService::addTurnout(const Turnout& turnout) {
    collection.add(turnout);
}

const Turnout* TurnoutService::getTurnout(int address) const {
    return collection.getByAddress(address);
}

TurnoutServiceResult TurnoutService::throwStraight(int address) {
    Turnout* turnout = collection.getByAddress(address);

    if (turnout == nullptr) {
        return TurnoutServiceResult::NotFound;
    }

    if (turnout->isLocked()) {
        return TurnoutServiceResult::Locked;
    }

    if (turnout->isDisabled()) {
        return TurnoutServiceResult::Disabled;
    }

    turnout->throwStraight();
    return TurnoutServiceResult::Success;
}

TurnoutServiceResult TurnoutService::throwDiverging(int address) {
    Turnout* turnout = collection.getByAddress(address);

    if (turnout == nullptr) {
        return TurnoutServiceResult::NotFound;
    }

    if (turnout->isLocked()) {
        return TurnoutServiceResult::Locked;
    }

    if (turnout->isDisabled()) {
        return TurnoutServiceResult::Disabled;
    }

    turnout->throwDiverging();
    return TurnoutServiceResult::Success;
}

TurnoutServiceResult TurnoutService::toggle(int address) {
    Turnout* turnout = collection.getByAddress(address);

    if (turnout == nullptr) {
        return TurnoutServiceResult::NotFound;
    }

    if (turnout->isLocked()) {
        return TurnoutServiceResult::Locked;
    }

    if (turnout->isDisabled()) {
        return TurnoutServiceResult::Disabled;
    }

    turnout->toggle();
    return TurnoutServiceResult::Success;
}
