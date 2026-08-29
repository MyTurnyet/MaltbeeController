#pragma once

#include "ports/Clock.h"
#include "../ports/LocoNetSwitchDriver.h"
#include "../ports/LocoNetTransport.h"

class PulsingLocoNetTransport final : public LocoNetTransport
{
public:
    PulsingLocoNetTransport(LocoNetSwitchDriver& driver, Clock& clock, unsigned long pulseDurationMs);

    void sendPacket(const LocoNetPacket& packet) override;
    void update();

private:
    void releasePendingSwitch();

    LocoNetSwitchDriver& driver_;
    Clock& clock_;
    unsigned long pulseDurationMs_;

    bool releasePending_ = false;
    int pendingAddress_ = 0;
    TurnoutPosition pendingPosition_ = TurnoutPosition::Closed;
    unsigned long releaseAtMs_ = 0;
};
