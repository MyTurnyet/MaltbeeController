#include "StatusLedDriver.h"

StatusLedDriver::StatusLedDriver(DigitalOutput& gpio, Clock& clock, const unsigned long slowBlinkIntervalMs,
                                   const unsigned long fastBlinkIntervalMs)
    : gpio_(gpio), clock_(clock), slowBlinkIntervalMs_(slowBlinkIntervalMs), fastBlinkIntervalMs_(fastBlinkIntervalMs)
{
    lastToggleTime_ = clock_.nowMilliseconds();
    applyGpio();
}

void StatusLedDriver::begin()
{
    lastToggleTime_ = clock_.nowMilliseconds();
    blinkShowingOn_ = true;
    applyGpio();
}

void StatusLedDriver::setState(const State state)
{
    if (state == state_)
    {
        return;
    }
    state_ = state;
    blinkShowingOn_ = true;
    lastToggleTime_ = clock_.nowMilliseconds();
    applyGpio();
}

void StatusLedDriver::update()
{
    if (state_ != State::SlowBlink && state_ != State::FastBlink)
    {
        return;
    }

    const unsigned long now = clock_.nowMilliseconds();
    const unsigned long intervalMs = state_ == State::FastBlink ? fastBlinkIntervalMs_ : slowBlinkIntervalMs_;
    if (now - lastToggleTime_ < intervalMs)
    {
        return;
    }

    blinkShowingOn_ = !blinkShowingOn_;
    lastToggleTime_ = now;
    applyGpio();
}

void StatusLedDriver::applyGpio()
{
    switch (state_)
    {
        case State::Off:
            gpio_.set(false);
            break;
        case State::On:
            gpio_.set(true);
            break;
        case State::SlowBlink:
        case State::FastBlink:
            gpio_.set(blinkShowingOn_);
            break;
    }
}
