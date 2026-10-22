#include "IndependentLedPairDriver.h"

IndependentLedPairDriver::IndependentLedPairDriver(DigitalOutput& greenGpio, DigitalOutput& redGpio,
                                                     Clock& clock, const unsigned long blinkIntervalMs,
                                                     const LedPairColor defaultColor)
    : greenGpio_(greenGpio), redGpio_(redGpio), clock_(clock), blinkIntervalMs_(blinkIntervalMs),
      lastDisplayedColor_(defaultColor)
{
    lastToggleTime_ = clock_.nowMilliseconds();
    writeColor(lastDisplayedColor_);
}

void IndependentLedPairDriver::begin()
{
    lastToggleTime_ = clock_.nowMilliseconds();
    writeColor(currentColorToShow());
}

void IndependentLedPairDriver::setGreen(const bool active)
{
    greenRequested_ = active;
    applyState();
}

void IndependentLedPairDriver::setRed(const bool active)
{
    redRequested_ = active;
    applyState();
}

bool IndependentLedPairDriver::isGreenRequested() const
{
    return greenRequested_;
}

bool IndependentLedPairDriver::isRedRequested() const
{
    return redRequested_;
}

void IndependentLedPairDriver::setIdentifying(const bool active)
{
    if (active == identifying_)
    {
        return;
    }
    identifying_ = active;
    if (identifying_)
    {
        identifyShowingGreen_ = true;
        identifyLastToggleMs_ = clock_.nowMilliseconds();
        writeColor(LedPairColor::Green);
    }
    else
    {
        writeColor(currentColorToShow());
    }
}

void IndependentLedPairDriver::applyState()
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
        return; // transient both-requested state: leave the GPIOs exactly as they were
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

void IndependentLedPairDriver::writeColor(const LedPairColor color)
{
    greenGpio_.set(color == LedPairColor::Green);
    redGpio_.set(color == LedPairColor::Red);
}

LedPairColor IndependentLedPairDriver::currentColorToShow() const
{
    if (currentMode_ == Mode::Green)
    {
        return LedPairColor::Green;
    }
    if (currentMode_ == Mode::Red)
    {
        return LedPairColor::Red;
    }

    const LedPairColor opposite =
        lastDisplayedColor_ == LedPairColor::Green ? LedPairColor::Red : LedPairColor::Green;
    return blinkShowingLastColor_ ? lastDisplayedColor_ : opposite;
}

void IndependentLedPairDriver::update()
{
    if (identifying_)
    {
        const unsigned long now = clock_.nowMilliseconds();
        if (now - identifyLastToggleMs_ >= kIdentifyIntervalMs)
        {
            identifyShowingGreen_ = !identifyShowingGreen_;
            identifyLastToggleMs_ = now;
            writeColor(identifyShowingGreen_ ? LedPairColor::Green : LedPairColor::Red);
        }
        return;
    }

    if (currentMode_ != Mode::Blink)
    {
        return;
    }

    const unsigned long now = clock_.nowMilliseconds();
    if (now - lastToggleTime_ < blinkIntervalMs_)
    {
        return;
    }

    blinkShowingLastColor_ = !blinkShowingLastColor_;
    lastToggleTime_ = now;

    const LedPairColor opposite =
        lastDisplayedColor_ == LedPairColor::Green ? LedPairColor::Red : LedPairColor::Green;
    writeColor(blinkShowingLastColor_ ? lastDisplayedColor_ : opposite);
}
