#ifdef ARDUINO

#include "MrrwaLocoNetFeedbackSource.h"

#include <LocoNet.h>

namespace
{
    bool reportPending = false;
    SwitchOutputsReport pendingReport;
}

void notifySwitchOutputsReport(uint16_t address, uint8_t closedOutput, uint8_t thrownOutput)
{
    reportPending = true;
    pendingReport = {static_cast<int>(address), closedOutput != 0, thrownOutput != 0};
}

bool MrrwaLocoNetFeedbackSource::poll(SwitchOutputsReport& outReport)
{
    reportPending = false;

    lnMsg* receivedPacket = LocoNet.receive();
    if (receivedPacket != nullptr)
    {
        LocoNet.processSwitchSensorMessage(receivedPacket);
    }

    if (!reportPending)
    {
        return false;
    }

    outReport = pendingReport;
    return true;
}

#endif
