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
    else if (!active && holding_ && !releasing_)
    {
        releasing_ = true;
        releaseStartMs_ = clock_.nowMilliseconds();
    }
    else if (releasing_)
    {
        if (active)
        {
            // Contact bounce: the button never actually left the button for
            // any meaningful duration, so the original press time stands.
            releasing_ = false;
        }
        else if (clock_.nowMilliseconds() - releaseStartMs_ >= kReleaseSettleMs)
        {
            holding_ = false;
            releasing_ = false;
            const unsigned long heldFor = releaseStartMs_ - holdStartMs_;
            if (heldFor >= minHoldMs_)
            {
                requestedThisTick_ = true;
            }
        }
    }
}

bool ButtonSetupModeTrigger::requested() const
{
    return requestedThisTick_;
}
