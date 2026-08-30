#pragma once

#include "domain/Button.h"
#include "domain/Turnout.h"
#include "domain/TurnoutIndicator.h"
#include "ports/TurnoutCommandPort.h"

class ToggleTurnoutControl
{
public:
    ToggleTurnoutControl(Button& button, Turnout& turnout, TurnoutIndicator& indicator,
                          TurnoutCommandPort& turnoutCommandPort);

    void update();
    void applyFeedback(TurnoutFeedback feedback);

private:
    Button& button_;
    Turnout& turnout_;
    TurnoutIndicator& indicator_;
    TurnoutCommandPort& turnoutCommandPort_;
};
