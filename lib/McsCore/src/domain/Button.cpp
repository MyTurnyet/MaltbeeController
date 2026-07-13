#include "Button.h"

Button::Button(DigitalInput& input, Clock& clock, const unsigned long debounceMilliseconds)
    : input_(input)
    , clock_(clock)
    , debounceMilliseconds_(debounceMilliseconds)
{
}

void Button::update()
{
    const bool raw = input_.isActive();

    if (raw != lastRawReading_)
    {
        lastRawReading_ = raw;
        lastRawTransitionTime_ = clock_.nowMilliseconds();
    }

    previousStableReading_ = stableReading_;

    if (clock_.nowMilliseconds() - lastRawTransitionTime_ >= debounceMilliseconds_)
    {
        stableReading_ = raw;
    }
}

bool Button::isPressed() const
{
    return stableReading_;
}

bool Button::wasPressed() const
{
    return stableReading_ && !previousStableReading_;
}

bool Button::wasReleased() const
{
    return !stableReading_ && previousStableReading_;
}
