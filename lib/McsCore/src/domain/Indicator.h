#pragma once

#include "../ports/DigitalOutput.h"

class Indicator
{
public:
    explicit Indicator(DigitalOutput& output);

    void on();
    void off();
    void set(bool active);

    [[nodiscard]] bool isOn() const;

private:
    DigitalOutput& output_;
};