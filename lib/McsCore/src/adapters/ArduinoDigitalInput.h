#pragma once

#include "../ports/DigitalInput.h"

class ArduinoDigitalInput final : public DigitalInput
{
public:
    ArduinoDigitalInput(int pin, bool activeLow);

    void begin();

    [[nodiscard]] bool isActive() const override;

private:
    int pin_;
    bool activeLow_;
};
