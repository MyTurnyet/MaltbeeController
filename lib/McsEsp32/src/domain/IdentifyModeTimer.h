#pragma once

#include "ports/Clock.h"

class IdentifyModeTimer
{
public:
    IdentifyModeTimer(Clock& clock, unsigned long durationMs);

    void trigger();
    [[nodiscard]] bool isActive() const;

private:
    Clock& clock_;
    unsigned long durationMs_;
    bool triggered_ = false;
    unsigned long triggeredAtMs_ = 0;
};
