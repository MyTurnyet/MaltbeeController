#pragma once

#include "domain/Turnout.h"

class LocoNetSwitchDriver
{
public:
    virtual ~LocoNetSwitchDriver() = default;

    virtual void requestSwitch(int address, TurnoutPosition position, bool outputOn) = 0;
};
