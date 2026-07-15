#ifdef ARDUINO

#include "ArduinoClock.h"

#include <Arduino.h>

unsigned long ArduinoClock::nowMilliseconds() const
{
    return millis();
}

#endif
