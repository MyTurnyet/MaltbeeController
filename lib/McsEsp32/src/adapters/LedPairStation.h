#pragma once

#include "adapters/ArduinoDigitalOutput.h"
#include "ports/Clock.h"
#include "ports/DigitalOutput.h"
#include "../domain/LedPairDriver.h"
#include "LedPairOutput.h"

struct LedPairConfig
{
    int gpioPin;
};

class LedPairStation
{
public:
    LedPairStation(const LedPairConfig& config, Clock& clock, unsigned long blinkIntervalMs,
                   LedPairColor defaultColor);

    void begin();
    void update();
    void setIdentifying(bool active);

    DigitalOutput& green();
    DigitalOutput& red();

private:
    ArduinoDigitalOutput gpio_;
    LedPairDriver driver_;
    LedPairOutput green_;
    LedPairOutput red_;
};
