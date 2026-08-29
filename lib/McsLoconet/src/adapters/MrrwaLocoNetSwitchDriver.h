#pragma once

#include "../ports/LocoNetSwitchDriver.h"

class MrrwaLocoNetSwitchDriver final : public LocoNetSwitchDriver
{
public:
    void requestSwitch(int address, TurnoutPosition position, bool outputOn) override;
};
