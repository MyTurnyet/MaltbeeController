#pragma once

#include "ports/Clock.h"
#include "ports/DigitalOutput.h"

// Drives one single-color status LED (one real GPIO) through four states.
// update() must be called on every loop iteration for blinking to work.
class StatusLedDriver
{
public:
    enum class State
    {
        Off,
        On,
        SlowBlink,
        FastBlink
    };

    StatusLedDriver(DigitalOutput& gpio, Clock& clock, unsigned long slowBlinkIntervalMs,
                     unsigned long fastBlinkIntervalMs);

    void begin();
    void setState(State state);
    void update();

private:
    void applyGpio();

    DigitalOutput& gpio_;
    Clock& clock_;
    unsigned long slowBlinkIntervalMs_;
    unsigned long fastBlinkIntervalMs_;

    State state_ = State::Off;
    bool blinkShowingOn_ = true;
    unsigned long lastToggleTime_ = 0;
};
