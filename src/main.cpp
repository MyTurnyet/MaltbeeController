#include <Arduino.h>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ArduinoDigitalOutput.h"
#include "adapters/NullTurnoutCommandPort.h"
#include "application/TurnoutControl.h"
#include "domain/Button.h"
#include "domain/Indicator.h"
#include "domain/Turnout.h"
#include "domain/TurnoutIndicator.h"

namespace
{
    constexpr int THROW_BUTTON_PIN = 22;
    constexpr int CLOSE_BUTTON_PIN = 23;
    constexpr int THROWN_LED_PIN = 8;
    constexpr int CLOSED_LED_PIN = 9;
    constexpr unsigned long DEBOUNCE_MS = 30;
    constexpr int TURNOUT_ADDRESS = 101;
}

ArduinoClock clock;

ArduinoDigitalInput throwInput(THROW_BUTTON_PIN, true);
ArduinoDigitalInput closeInput(CLOSE_BUTTON_PIN, true);

ArduinoDigitalOutput thrownOutput(THROWN_LED_PIN, false);
ArduinoDigitalOutput closedOutput(CLOSED_LED_PIN, false);

Button throwButton(throwInput, clock, DEBOUNCE_MS);
Button closeButton(closeInput, clock, DEBOUNCE_MS);

Indicator thrownIndicator(thrownOutput);
Indicator closedIndicator(closedOutput);

Turnout turnout(TURNOUT_ADDRESS, "Turnout 1", TurnoutPosition::Closed, false, false);

TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

NullTurnoutCommandPort turnoutCommandPort;

TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, turnoutCommandPort);

void setup()
{
    throwInput.begin();
    closeInput.begin();

    thrownOutput.begin();
    closedOutput.begin();
}

void loop()
{
    throwButton.update();
    closeButton.update();

    control.update();
}
