#pragma once

#include "../ports/Clock.h"
#include "../ports/DigitalInput.h"

class Button
{
public:
    Button(DigitalInput& input, Clock& clock, unsigned long debounceMilliseconds);

    void update();

    [[nodiscard]] bool isPressed() const;
    [[nodiscard]] bool wasPressed() const;
    [[nodiscard]] bool wasReleased() const;

private:
    DigitalInput& input_;
    Clock& clock_;
    unsigned long debounceMilliseconds_;

    bool lastRawReading_ = false;
    unsigned long lastRawTransitionTime_ = 0;

    bool stableReading_ = false;
    bool previousStableReading_ = false;
};
