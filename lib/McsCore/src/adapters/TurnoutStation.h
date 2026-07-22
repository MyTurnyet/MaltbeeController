#pragma once

#include "ArduinoDigitalInput.h"
#include "ArduinoDigitalOutput.h"
#include "../application/TurnoutControl.h"
#include "../domain/Button.h"
#include "../domain/Indicator.h"
#include "../domain/Turnout.h"
#include "../domain/TurnoutIndicator.h"
#include "../ports/Clock.h"
#include "../ports/TurnoutCommandPort.h"

struct TurnoutConfig
{
    int address;
    const char* name;
    int throwButtonPin;
    int closeButtonPin;
    int thrownLedPin;
    int closedLedPin;
};

class TurnoutStation
{
public:
    TurnoutStation(const TurnoutConfig& config, Clock& clock, TurnoutCommandPort& commandPort);

    void begin();
    void update();
    void applyFeedback(TurnoutFeedback feedback);

private:
    static constexpr unsigned long DEBOUNCE_MS = 30;

    ArduinoDigitalInput throwInput_;
    ArduinoDigitalInput closeInput_;
    ArduinoDigitalOutput thrownOutput_;
    ArduinoDigitalOutput closedOutput_;
    Turnout turnout_;
    Button throwButton_;
    Button closeButton_;
    Indicator thrownIndicator_;
    Indicator closedIndicator_;
    TurnoutIndicator turnoutIndicator_;
    TurnoutControl control_;
};
