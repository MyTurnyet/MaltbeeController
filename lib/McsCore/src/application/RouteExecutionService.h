#pragma once

#include "../domain/Route.h"
#include "../ports/Clock.h"
#include "../ports/TurnoutCommandPort.h"

// Executes a Route's turnout commands one at a time, directly through the
// given TurnoutCommandPort - the same path a real button press already
// uses. Sends are spaced out over time (update() must be called on every
// loop iteration) rather than fired back-to-back, because
// PulsingLocoNetTransport can only track one pending pulse at a time;
// firing commands faster than that would truncate every earlier pulse.
class RouteExecutionService
{
public:
    RouteExecutionService(TurnoutCommandPort& commandPort, Clock& clock, unsigned long commandSpacingMs);

    // route must outlive the entire execution (every update() call until the
    // route completes or a new start() replaces it) - held by reference, not
    // copied. Pass a long-lived object, not a temporary or stack-local.
    void start(const Route& route);
    void update();
    [[nodiscard]] bool isRunning() const;

private:
    TurnoutCommandPort& commandPort_;
    Clock& clock_;
    unsigned long commandSpacingMs_;

    const Route* activeRoute_ = nullptr;
    int nextCommandIndex_ = 0;
    unsigned long lastSendAtMs_ = 0;
};
