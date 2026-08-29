#include "PulsingLocoNetTransport.h"

PulsingLocoNetTransport::PulsingLocoNetTransport(LocoNetSwitchDriver& driver, Clock& clock, const unsigned long pulseDurationMs)
    : driver_(driver), clock_(clock), pulseDurationMs_(pulseDurationMs)
{
}

void PulsingLocoNetTransport::sendPacket(const LocoNetPacket& packet)
{
    if (releasePending_)
    {
        releasePendingSwitch();
    }

    driver_.requestSwitch(packet.address, packet.position, true);

    releasePending_ = true;
    pendingAddress_ = packet.address;
    pendingPosition_ = packet.position;
    releaseAtMs_ = clock_.nowMilliseconds() + pulseDurationMs_;
}

void PulsingLocoNetTransport::update()
{
    if (releasePending_ && clock_.nowMilliseconds() >= releaseAtMs_)
    {
        releasePendingSwitch();
    }
}

void PulsingLocoNetTransport::releasePendingSwitch()
{
    driver_.requestSwitch(pendingAddress_, pendingPosition_, false);
    releasePending_ = false;
}
