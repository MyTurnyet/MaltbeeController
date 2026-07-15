#ifdef ARDUINO

#include "ArduinoDigitalInput.h"

#include <Arduino.h>

ArduinoDigitalInput::ArduinoDigitalInput(const int pin, const bool activeLow)
    : pin_(pin), activeLow_(activeLow)
{
}

void ArduinoDigitalInput::begin()
{
    pinMode(pin_, activeLow_ ? INPUT_PULLUP : INPUT);
}

bool ArduinoDigitalInput::isActive() const
{
    const bool raw = digitalRead(pin_) == HIGH;
    return activeLow_ ? !raw : raw;
}

#endif
