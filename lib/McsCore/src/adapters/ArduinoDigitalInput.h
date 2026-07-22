#pragma once

#include "../ports/DigitalInput.h"

class ArduinoDigitalInput final : public DigitalInput
{
public:
    ArduinoDigitalInput(int pin, bool activeLow, bool useInternalPullup);

    void begin();

    [[nodiscard]] bool isActive() const override;

private:
    int pin_;
    bool activeLow_;
    bool useInternalPullup_;
};
