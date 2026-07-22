#include <Arduino.h>
#include <LocoNet.h>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ArduinoDigitalOutput.h"
#include "adapters/MrrwaLocoNetSwitchDriver.h"
#include "adapters/MrrwaLocoNetTurnoutAdapter.h"
#include "adapters/PulsingLocoNetTransport.h"
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
    constexpr unsigned long LOCONET_PULSE_DURATION_MS = 250;
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

MrrwaLocoNetSwitchDriver locoNetSwitchDriver;
PulsingLocoNetTransport locoNetTransport(locoNetSwitchDriver, clock, LOCONET_PULSE_DURATION_MS);
MrrwaLocoNetTurnoutAdapter turnoutCommandPort(locoNetTransport);

TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, turnoutCommandPort);

void setup()
{
    throwInput.begin();
    closeInput.begin();

    thrownOutput.begin();
    closedOutput.begin();

    LocoNet.init();
}

void loop()
{
    throwButton.update();
    closeButton.update();

    control.update();
    locoNetTransport.update();
}
