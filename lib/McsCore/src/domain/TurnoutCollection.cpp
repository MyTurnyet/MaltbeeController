#include "TurnoutCollection.h"

void TurnoutCollection::add(const Turnout& turnout) {
    for (int i = 0; i < count_; ++i) {
        if (turnouts_[i].address() == turnout.address()) {
            turnouts_[i] = turnout;
            return;
        }
    }

    if (count_ >= MAX_TURNOUTS) {
        return;
    }

    turnouts_[count_] = turnout;
    ++count_;
}

int TurnoutCollection::count() const {
    return count_;
}

const Turnout* TurnoutCollection::getByAddress(int address) const {
    for (int i = 0; i < count_; ++i) {
        if (turnouts_[i].address() == address) {
            return &turnouts_[i];
        }
    }
    return nullptr;
}

Turnout* TurnoutCollection::getByAddress(int address) {
    for (int i = 0; i < count_; ++i) {
        if (turnouts_[i].address() == address) {
            return &turnouts_[i];
        }
    }
    return nullptr;
}
