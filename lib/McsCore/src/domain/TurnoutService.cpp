#include "TurnoutService.h"

TurnoutService::TurnoutService(TurnoutCollection& collection)
    : collection_(collection) {
}

void TurnoutService::addTurnout(const Turnout& turnout) {
    collection_.add(turnout);
}

const Turnout* TurnoutService::getTurnout(int address) const {
    return collection_.getByAddress(address);
}

TurnoutServiceResult TurnoutService::validateTurnout(Turnout* turnout) const {
    if (turnout == nullptr) {
        return TurnoutServiceResult::NotFound;
    }

    if (turnout->isLocked()) {
        return TurnoutServiceResult::Locked;
    }

    if (turnout->isDisabled()) {
        return TurnoutServiceResult::Disabled;
    }

    return TurnoutServiceResult::Success;
}

TurnoutServiceResult TurnoutService::throwStraight(int address) {
    Turnout* turnout = collection_.getByAddress(address);
    TurnoutServiceResult validation = validateTurnout(turnout);
    if (validation != TurnoutServiceResult::Success) {
        return validation;
    }

    turnout->throwStraight();
    return TurnoutServiceResult::Success;
}

TurnoutServiceResult TurnoutService::throwDiverging(int address) {
    Turnout* turnout = collection_.getByAddress(address);
    TurnoutServiceResult validation = validateTurnout(turnout);
    if (validation != TurnoutServiceResult::Success) {
        return validation;
    }

    turnout->throwDiverging();
    return TurnoutServiceResult::Success;
}

TurnoutServiceResult TurnoutService::toggle(int address) {
    Turnout* turnout = collection_.getByAddress(address);
    TurnoutServiceResult validation = validateTurnout(turnout);
    if (validation != TurnoutServiceResult::Success) {
        return validation;
    }

    turnout->toggle();
    return TurnoutServiceResult::Success;
}
