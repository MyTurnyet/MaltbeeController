#ifdef ARDUINO

#include "ArduinoDigitalOutput.h"

#include <Arduino.h>

ArduinoDigitalOutput::ArduinoDigitalOutput(const int pin, const bool activeLow)
    : pin_(pin), activeLow_(activeLow)
{
}

void ArduinoDigitalOutput::begin()
{
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, activeLow_ ? HIGH : LOW);
}

void ArduinoDigitalOutput::set(const bool active)
{
    active_ = active;
    const bool driveHigh = activeLow_ ? !active : active;
    digitalWrite(pin_, driveHigh ? HIGH : LOW);
}

bool ArduinoDigitalOutput::isSet() const
{
    return active_;
}

#endif
