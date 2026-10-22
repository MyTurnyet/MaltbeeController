#ifdef ARDUINO

#include "IndependentLedPairStation.h"

IndependentLedPairStation::IndependentLedPairStation(const IndependentLedPairConfig& config, Clock& clock,
                                                       const unsigned long blinkIntervalMs,
                                                       const LedPairColor defaultColor)
    : redGpio_(config.redGpioPin, false)
    , greenGpio_(config.greenGpioPin, false)
    , driver_(greenGpio_, redGpio_, clock, blinkIntervalMs, defaultColor)
    , green_(driver_, LedPairColor::Green)
    , red_(driver_, LedPairColor::Red)
{
}

void IndependentLedPairStation::begin()
{
    redGpio_.begin();
    greenGpio_.begin();
    driver_.begin();
}

void IndependentLedPairStation::update()
{
    driver_.update();
}

void IndependentLedPairStation::setIdentifying(const bool active)
{
    driver_.setIdentifying(active);
}

DigitalOutput& IndependentLedPairStation::green()
{
    return green_;
}

DigitalOutput& IndependentLedPairStation::red()
{
    return red_;
}

IndependentLedPairDriver& IndependentLedPairStation::driver()
{
    return driver_;
}

#endif
