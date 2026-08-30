#pragma once

#include "ports/Clock.h"
#include "ports/DigitalOutput.h"

enum class LedPairColor
{
    Green,
    Red
};

class LedPairDriver
{
public:
    LedPairDriver(DigitalOutput& gpio, Clock& clock, unsigned long blinkIntervalMs,
                  LedPairColor defaultColor);

    void setGreen(bool active);
    void setRed(bool active);

    [[nodiscard]] bool isGreenRequested() const;
    [[nodiscard]] bool isRedRequested() const;

    void update();

private:
    enum class Mode
    {
        Green,
        Red,
        Blink
    };

    void applyState();
    void writeColor(LedPairColor color);

    DigitalOutput& gpio_;
    Clock& clock_;
    unsigned long blinkIntervalMs_;
    LedPairColor lastDisplayedColor_;

    bool greenRequested_ = false;
    bool redRequested_ = false;

    Mode currentMode_ = Mode::Blink;
    bool blinkShowingLastColor_ = true;
    unsigned long lastToggleTime_ = 0;
};
