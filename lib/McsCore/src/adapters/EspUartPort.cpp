#ifdef ESP32

#include "EspUartPort.h"

#include <Arduino.h>

EspUartPort::EspUartPort(const unsigned long baudRate) : baudRate_(baudRate)
{
}

void EspUartPort::begin()
{
    Serial.begin(baudRate_);
}

bool EspUartPort::available() const
{
    return Serial.available() > 0;
}

char EspUartPort::read()
{
    return static_cast<char>(Serial.read());
}

void EspUartPort::write(const std::string& text)
{
    Serial.print(text.c_str());
}

#endif
