#include "IdentifyModeTimer.h"

IdentifyModeTimer::IdentifyModeTimer(Clock& clock, const unsigned long durationMs)
    : clock_(clock), durationMs_(durationMs)
{
}

void IdentifyModeTimer::trigger()
{
    triggered_ = true;
    triggeredAtMs_ = clock_.nowMilliseconds();
}

bool IdentifyModeTimer::isActive() const
{
    return triggered_ && (clock_.nowMilliseconds() - triggeredAtMs_ < durationMs_);
}
