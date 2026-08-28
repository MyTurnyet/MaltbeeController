#if defined(ARDUINO) && !defined(ESP32)

#include "MrrwaLocoNetSwitchDriver.h"

#include <LocoNet.h>

void MrrwaLocoNetSwitchDriver::requestSwitch(const int address, const TurnoutPosition position, const bool outputOn)
{
    const uint8_t direction = position == TurnoutPosition::Closed ? 0 : 1;
    LocoNet.requestSwitch(static_cast<uint16_t>(address), outputOn ? 1 : 0, direction);
}

#endif
