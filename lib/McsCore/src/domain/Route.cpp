#include "Route.h"

Route::Route(int id, const char* name)
    : routeId(id), routeName(name) {
}

int Route::id() const {
    return routeId;
}

FixedString32 Route::name() const {
    return routeName;
}

bool Route::addTurnout(int address, TurnoutPosition position) {
    if (containsTurnout(address)) {
        return false;
    }
    if (commandCount_ >= MAX_TURNOUT_COMMANDS_PER_ROUTE) {
        return false;
    }
    commands_[commandCount_] = TurnoutCommand{address, position};
    ++commandCount_;
    return true;
}

int Route::getTurnoutCount() const {
    return commandCount_;
}

bool Route::containsTurnout(int address) const {
    for (int i = 0; i < commandCount_; ++i) {
        if (commands_[i].address == address) {
            return true;
        }
    }
    return false;
}

TurnoutPositionLookup Route::getTurnoutPosition(int address) const {
    for (int i = 0; i < commandCount_; ++i) {
        if (commands_[i].address == address) {
            return TurnoutPositionLookup{true, commands_[i].position};
        }
    }
    return TurnoutPositionLookup{false, TurnoutPosition::Closed};
}

TurnoutCommand Route::commandAt(int index) const {
    return commands_[index];
}
