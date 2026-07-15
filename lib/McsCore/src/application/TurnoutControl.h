#pragma once

#include "../domain/Button.h"
#include "../domain/Turnout.h"
#include "../domain/TurnoutIndicator.h"
#include "../ports/TurnoutCommandPort.h"

class TurnoutControl
{
public:
    TurnoutControl(Button& throwButton,
                   Button& closeButton,
                   Turnout& turnout,
                   TurnoutIndicator& indicator,
                   TurnoutCommandPort& turnoutCommandPort);

    void update();
    void applyFeedback(TurnoutFeedback feedback);

private:
    Button& throwButton_;
    Button& closeButton_;
    Turnout& turnout_;
    TurnoutIndicator& indicator_;
    TurnoutCommandPort& turnoutCommandPort_;
};
