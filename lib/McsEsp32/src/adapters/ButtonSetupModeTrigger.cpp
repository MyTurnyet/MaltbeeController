#include "ButtonSetupModeTrigger.h"

ButtonSetupModeTrigger::ButtonSetupModeTrigger(DigitalInput& button, Clock& clock,
                                                const unsigned long minHoldMs)
    : button_(button), clock_(clock), minHoldMs_(minHoldMs)
{
}

void ButtonSetupModeTrigger::update()
{
    requestedThisTick_ = false;
    const bool active = button_.isActive();

    if (active && !holding_)
    {
        holding_ = true;
        holdStartMs_ = clock_.nowMilliseconds();
    }
    else if (!active && holding_)
    {
        holding_ = false;
        const unsigned long heldFor = clock_.nowMilliseconds() - holdStartMs_;
        if (heldFor >= minHoldMs_)
        {
            requestedThisTick_ = true;
        }
    }
}

bool ButtonSetupModeTrigger::requested() const
{
    return requestedThisTick_;
}
