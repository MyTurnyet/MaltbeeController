#pragma once

#include "../ports/DigitalOutput.h"

class ArduinoDigitalOutput final : public DigitalOutput
{
public:
    ArduinoDigitalOutput(int pin, bool activeLow);

    void begin();

    void set(bool active) override;
    [[nodiscard]] bool isSet() const override;

private:
    int pin_;
    bool activeLow_;
    bool active_ = false;
};
