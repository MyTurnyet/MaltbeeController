//
// Created by Paige Watson on 7/9/26.
//

#include "Turnout.h"

#include <utility>


Turnout::Turnout(int address,
                 std::string name,
                 TurnoutPosition turnout_position,
                 bool turnout_locked,
                 bool turnout_disabled) {
    this->currentPosition = turnout_position;
    this->displayName = std::move(name);
    this->layoutAddress = address;\
    this->turnoutLocked = turnout_locked;
    this->turnoutDisabled = turnout_disabled;
}

void Turnout::throwStraight() {
    currentPosition = TurnoutPosition::Closed;
}

void Turnout::throwDiverging() {
    currentPosition = TurnoutPosition::Thrown;
}

TurnoutPosition Turnout::position() const {
    return currentPosition;
}

std::string Turnout::name() const {
    return displayName;
}

int Turnout::address() const {
    return layoutAddress;
}

void Turnout::toggle() {
    if (currentPosition == TurnoutPosition::Closed) {
        throwDiverging();
        return;
    }
    throwStraight();
}

bool Turnout::canThrow() const {
    return !isLocked() && !isDisabled();
}

bool Turnout::isLocked() const {
    return turnoutLocked;
}

void Turnout::lock() {
    turnoutLocked = true;
}

void Turnout::unlock() {
    turnoutLocked = false;
}

bool Turnout::isDisabled() const {
    return turnoutDisabled;
}

void Turnout::disable() {
    turnoutDisabled = true;
}

void Turnout::enable() {
    turnoutDisabled = false;
}
