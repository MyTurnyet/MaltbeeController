#include "TurnoutCollection.h"

void TurnoutCollection::add(const Turnout& turnout) {
    turnouts.insert_or_assign(turnout.address(), turnout);
}

int TurnoutCollection::count() const {
    return static_cast<int>(turnouts.size());
}

const Turnout* TurnoutCollection::getByAddress(int address) const {
    auto it = turnouts.find(address);
    if (it != turnouts.end()) {
        return &it->second;
    }
    return nullptr;
}

Turnout* TurnoutCollection::getByAddress(int address) {
    auto it = turnouts.find(address);
    if (it != turnouts.end()) {
        return &it->second;
    }
    return nullptr;
}
