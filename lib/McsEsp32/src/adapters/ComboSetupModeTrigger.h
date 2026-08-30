#pragma once

#include "ports/Clock.h"
#include "ports/DigitalInput.h"

class ComboSetupModeTrigger
{
public:
    ComboSetupModeTrigger(DigitalInput& buttonA, DigitalInput& buttonB, Clock& clock,
                           unsigned long minHoldMs);

    void update();
    [[nodiscard]] bool isHolding() const;
    [[nodiscard]] bool requested() const;

private:
    DigitalInput& buttonA_;
    DigitalInput& buttonB_;
    Clock& clock_;
    unsigned long minHoldMs_;
    bool holding_ = false;
    unsigned long holdStartMs_ = 0;
    bool requestedThisTick_ = false;
};
