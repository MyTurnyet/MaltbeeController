#pragma once

#include "adapters/ArduinoDigitalOutput.h"
#include "ports/Clock.h"
#include "ports/DigitalOutput.h"
#include "../domain/IndependentLedPairDriver.h"
#include "IndependentLedPairOutput.h"

struct IndependentLedPairConfig
{
    int redGpioPin;
    int greenGpioPin;
};

class IndependentLedPairStation
{
public:
    IndependentLedPairStation(const IndependentLedPairConfig& config, Clock& clock,
                               unsigned long blinkIntervalMs, LedPairColor defaultColor);

    IndependentLedPairStation(const IndependentLedPairStation&) = delete;
    IndependentLedPairStation& operator=(const IndependentLedPairStation&) = delete;

    void begin();
    void update();
    void setIdentifying(bool active);

    DigitalOutput& green();
    DigitalOutput& red();
    IndependentLedPairDriver& driver();

private:
    ArduinoDigitalOutput redGpio_;
    ArduinoDigitalOutput greenGpio_;
    IndependentLedPairDriver driver_;
    IndependentLedPairOutput green_;
    IndependentLedPairOutput red_;
};
