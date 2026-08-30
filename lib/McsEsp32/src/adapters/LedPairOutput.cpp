#include "LedPairOutput.h"

LedPairOutput::LedPairOutput(LedPairDriver& driver, const LedPairColor color)
    : driver_(driver), color_(color)
{
}

void LedPairOutput::set(const bool active)
{
    if (color_ == LedPairColor::Green)
    {
        driver_.setGreen(active);
    }
    else
    {
        driver_.setRed(active);
    }
}

bool LedPairOutput::isSet() const
{
    return color_ == LedPairColor::Green ? driver_.isGreenRequested() : driver_.isRedRequested();
}
