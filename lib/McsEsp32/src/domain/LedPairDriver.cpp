#include "LedPairDriver.h"

LedPairDriver::LedPairDriver(DigitalOutput& gpio, Clock& clock, const unsigned long blinkIntervalMs,
                              const LedPairColor defaultColor)
    : gpio_(gpio), clock_(clock), blinkIntervalMs_(blinkIntervalMs), lastDisplayedColor_(defaultColor)
{
    lastToggleTime_ = clock_.nowMilliseconds();
    writeColor(lastDisplayedColor_);
}

void LedPairDriver::setGreen(const bool active)
{
    greenRequested_ = active;
    applyState();
}

void LedPairDriver::setRed(const bool active)
{
    redRequested_ = active;
    applyState();
}

bool LedPairDriver::isGreenRequested() const
{
    return greenRequested_;
}

bool LedPairDriver::isRedRequested() const
{
    return redRequested_;
}

void LedPairDriver::applyState()
{
    Mode nextMode;
    if (greenRequested_ && !redRequested_)
    {
        nextMode = Mode::Green;
    }
    else if (redRequested_ && !greenRequested_)
    {
        nextMode = Mode::Red;
    }
    else if (!greenRequested_ && !redRequested_)
    {
        nextMode = Mode::Blink;
    }
    else
    {
        return; // transient both-requested state: leave the GPIO exactly as it was
    }

    if (nextMode == currentMode_)
    {
        return;
    }

    currentMode_ = nextMode;

    if (nextMode == Mode::Green)
    {
        lastDisplayedColor_ = LedPairColor::Green;
        writeColor(LedPairColor::Green);
    }
    else if (nextMode == Mode::Red)
    {
        lastDisplayedColor_ = LedPairColor::Red;
        writeColor(LedPairColor::Red);
    }
    else
    {
        blinkShowingLastColor_ = true;
        lastToggleTime_ = clock_.nowMilliseconds();
        writeColor(lastDisplayedColor_);
    }
}

void LedPairDriver::writeColor(const LedPairColor color)
{
    gpio_.set(color == LedPairColor::Green);
}

void LedPairDriver::update()
{
    if (currentMode_ != Mode::Blink)
    {
        return;
    }

    if (clock_.nowMilliseconds() - lastToggleTime_ < blinkIntervalMs_)
    {
        return;
    }

    blinkShowingLastColor_ = !blinkShowingLastColor_;
    lastToggleTime_ = clock_.nowMilliseconds();

    const LedPairColor opposite =
        lastDisplayedColor_ == LedPairColor::Green ? LedPairColor::Red : LedPairColor::Green;
    writeColor(blinkShowingLastColor_ ? lastDisplayedColor_ : opposite);
}
