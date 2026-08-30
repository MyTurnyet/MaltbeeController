#pragma once

#include "domain/Turnout.h"
#include "domain/Button.h"
#include "domain/Indicator.h"
#include "domain/TurnoutIndicator.h"
#include "ports/Clock.h"
#include "ports/DigitalInput.h"
#include "ports/DigitalOutput.h"
#include "ports/TurnoutCommandPort.h"
#include "../application/ToggleTurnoutControl.h"

class ToggleTurnoutStation
{
public:
    ToggleTurnoutStation(int address, const char* name, DigitalInput& button,
                          DigitalOutput& closedOutput, DigitalOutput& thrownOutput,
                          Clock& clock, TurnoutCommandPort& commandPort);

    void begin();
    void update();
    void applyFeedback(TurnoutFeedback feedback);
    void clearIndicator();

private:
    static constexpr unsigned long DEBOUNCE_MS = 30;

    Turnout turnout_;
    Button button_;
    Indicator closedIndicator_;
    Indicator thrownIndicator_;
    TurnoutIndicator turnoutIndicator_;
    ToggleTurnoutControl control_;
};
