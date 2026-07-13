#include "Route.h"
#include <utility>

Route::Route(int id, std::string name)
    : routeId(id), routeName(std::move(name)) {
}

int Route::id() const {
    return routeId;
}

std::string Route::name() const {
    return routeName;
}

bool Route::addTurnout(int address, TurnoutPosition position) {
    auto result = turnouts.insert({address, position});
    return result.second; // true if inserted, false if already exists
}

int Route::getTurnoutCount() const {
    return static_cast<int>(turnouts.size());
}

bool Route::containsTurnout(int address) const {
    return turnouts.find(address) != turnouts.end();
}

std::optional<TurnoutPosition> Route::getTurnoutPosition(int address) const {
    auto it = turnouts.find(address);
    if (it != turnouts.end()) {
        return it->second;
    }
    return std::nullopt;
}

const std::map<int, TurnoutPosition>& Route::getTurnouts() const {
    return turnouts;
}
