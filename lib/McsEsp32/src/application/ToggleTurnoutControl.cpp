#include "ToggleTurnoutControl.h"

ToggleTurnoutControl::ToggleTurnoutControl(Button& button, Turnout& turnout, TurnoutIndicator& indicator,
                                            TurnoutCommandPort& turnoutCommandPort)
    : button_(button), turnout_(turnout), indicator_(indicator), turnoutCommandPort_(turnoutCommandPort)
{
}

void ToggleTurnoutControl::update()
{
    if (!button_.wasPressed())
    {
        return;
    }

    const TurnoutPosition opposite =
        turnout_.position() == TurnoutPosition::Closed ? TurnoutPosition::Thrown : TurnoutPosition::Closed;
    turnoutCommandPort_.send(turnout_.address(), opposite);
}

void ToggleTurnoutControl::applyFeedback(const TurnoutFeedback feedback)
{
    if (feedback.address != turnout_.address())
    {
        return;
    }

    if (feedback.position == TurnoutPosition::Thrown)
    {
        turnout_.throwDiverging();
    }
    else
    {
        turnout_.throwStraight();
    }

    indicator_.display(feedback.position);
}
