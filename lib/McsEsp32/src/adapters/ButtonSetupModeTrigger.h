#pragma once

#include "ports/Clock.h"
#include "ports/DigitalInput.h"

class ButtonSetupModeTrigger
{
public:
    ButtonSetupModeTrigger(DigitalInput& button, Clock& clock, unsigned long minHoldMs);

    void update();
    [[nodiscard]] bool requested() const;

private:
    static constexpr unsigned long kReleaseSettleMs = 50;

    DigitalInput& button_;
    Clock& clock_;
    unsigned long minHoldMs_;
    bool holding_ = false;
    unsigned long holdStartMs_ = 0;
    bool releasing_ = false;
    unsigned long releaseStartMs_ = 0;
    bool requestedThisTick_ = false;
};
