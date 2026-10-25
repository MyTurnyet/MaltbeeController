#pragma once

#include "../domain/IdentifyModeTimer.h"
#include "ports/TurnoutCommandPort.h"

// Decorates a TurnoutCommandPort, forwarding every send() to the wrapped
// port while also triggering an activity timer - lets a composition root
// flash an "activity" LED on outbound Loco2MQTT traffic without any change
// to the wrapped port or its callers.
class ActivityTrackingTurnoutCommandPort : public TurnoutCommandPort
{
public:
    ActivityTrackingTurnoutCommandPort(TurnoutCommandPort& inner, IdentifyModeTimer& activityTimer)
        : inner_(inner), activityTimer_(activityTimer)
    {
    }

    void send(int address, TurnoutPosition position) override
    {
        inner_.send(address, position);
        activityTimer_.trigger();
    }

private:
    TurnoutCommandPort& inner_;
    IdentifyModeTimer& activityTimer_;
};
