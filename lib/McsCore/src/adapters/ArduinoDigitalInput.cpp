#ifdef ARDUINO

#include "ArduinoDigitalInput.h"

#include <Arduino.h>

ArduinoDigitalInput::ArduinoDigitalInput(const int pin, const bool activeLow, const bool useInternalPullup)
    : pin_(pin), activeLow_(activeLow), useInternalPullup_(useInternalPullup)
{
}

void ArduinoDigitalInput::begin()
{
    pinMode(pin_, useInternalPullup_ ? INPUT_PULLUP : INPUT);
}

bool ArduinoDigitalInput::isActive() const
{
    const bool raw = digitalRead(pin_) == HIGH;
    return activeLow_ ? !raw : raw;
}

#endif
