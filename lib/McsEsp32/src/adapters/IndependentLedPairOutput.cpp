#include "IndependentLedPairOutput.h"

IndependentLedPairOutput::IndependentLedPairOutput(IndependentLedPairDriver& driver, const LedPairColor color)
    : driver_(driver), color_(color)
{
}

void IndependentLedPairOutput::set(const bool active)
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

bool IndependentLedPairOutput::isSet() const
{
    return color_ == LedPairColor::Green ? driver_.isGreenRequested() : driver_.isRedRequested();
}
