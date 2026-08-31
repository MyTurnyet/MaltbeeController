#pragma once

#include "ports/Clock.h"
#include "ports/DigitalOutput.h"

enum class LedPairColor
{
    Green,
    Red
};

// Drives a shared-GPIO red/green LED pair: one GPIO level encodes both
// colors, so there is no true "off". `update()` must be called on every
// loop iteration for blinking to work. Exactly two `LedPairOutput`s (one
// green, one red) are expected to share a single `LedPairDriver` instance.
// Green corresponds to the GPIO driven HIGH.
class LedPairDriver
{
public:
    LedPairDriver(DigitalOutput& gpio, Clock& clock, unsigned long blinkIntervalMs,
                  LedPairColor defaultColor);

    void begin();

    void setGreen(bool active);
    void setRed(bool active);

    [[nodiscard]] bool isGreenRequested() const;
    [[nodiscard]] bool isRedRequested() const;

    void setIdentifying(bool active);

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
    [[nodiscard]] LedPairColor currentColorToShow() const;

    DigitalOutput& gpio_;
    Clock& clock_;
    unsigned long blinkIntervalMs_;
    LedPairColor lastDisplayedColor_;

    bool greenRequested_ = false;
    bool redRequested_ = false;

    Mode currentMode_ = Mode::Blink;
    bool blinkShowingLastColor_ = true;
    unsigned long lastToggleTime_ = 0;

    bool identifying_ = false;
    bool identifyShowingGreen_ = true;
    unsigned long identifyLastToggleMs_ = 0;
    static constexpr unsigned long kIdentifyIntervalMs = 150;
};
