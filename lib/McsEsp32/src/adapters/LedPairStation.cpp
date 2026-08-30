#ifdef ARDUINO

#include "LedPairStation.h"

LedPairStation::LedPairStation(const LedPairConfig& config, Clock& clock,
                                const unsigned long blinkIntervalMs, const LedPairColor defaultColor)
    : gpio_(config.gpioPin, false)
    , driver_(gpio_, clock, blinkIntervalMs, defaultColor)
    , green_(driver_, LedPairColor::Green)
    , red_(driver_, LedPairColor::Red)
{
}

void LedPairStation::begin()
{
    gpio_.begin();
    driver_.begin();
}

void LedPairStation::update()
{
    driver_.update();
}

DigitalOutput& LedPairStation::green()
{
    return green_;
}

DigitalOutput& LedPairStation::red()
{
    return red_;
}

#endif
