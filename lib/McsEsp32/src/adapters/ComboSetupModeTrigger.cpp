#include "ComboSetupModeTrigger.h"

ComboSetupModeTrigger::ComboSetupModeTrigger(DigitalInput& buttonA, DigitalInput& buttonB, Clock& clock,
                                              const unsigned long minHoldMs)
    : buttonA_(buttonA), buttonB_(buttonB), clock_(clock), minHoldMs_(minHoldMs)
{
}

void ComboSetupModeTrigger::update()
{
    requestedThisTick_ = false;
    const bool bothActive = buttonA_.isActive() && buttonB_.isActive();

    if (bothActive && !holding_)
    {
        holding_ = true;
        holdStartMs_ = clock_.nowMilliseconds();
    }
    else if (!bothActive && holding_)
    {
        holding_ = false;
        const unsigned long heldFor = clock_.nowMilliseconds() - holdStartMs_;
        if (heldFor >= minHoldMs_)
        {
            requestedThisTick_ = true;
        }
    }
}

bool ComboSetupModeTrigger::isHolding() const
{
    return holding_;
}

bool ComboSetupModeTrigger::requested() const
{
    return requestedThisTick_;
}
