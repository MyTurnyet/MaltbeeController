#pragma once

#include "LedPairDriver.h" // for LedPairColor
#include "ports/Clock.h"
#include "ports/DigitalOutput.h"

// Drives two independent real GPIOs (green, red) with the same mode/blink
// state machine as LedPairDriver, which exists for a single shared pin
// that cannot go dark. These two GPIOs could show "both off" or "both on"
// (amber), but writeColor() deliberately never does either — it always
// drives exactly one high and the other low, reproducing the shared-pin
// driver's exact blink visual on independent hardware. update() must be
// called on every loop iteration for blinking to work.
class IndependentLedPairDriver
{
public:
    IndependentLedPairDriver(DigitalOutput& greenGpio, DigitalOutput& redGpio, Clock& clock,
                              unsigned long blinkIntervalMs, LedPairColor defaultColor);

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

    DigitalOutput& greenGpio_;
    DigitalOutput& redGpio_;
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
