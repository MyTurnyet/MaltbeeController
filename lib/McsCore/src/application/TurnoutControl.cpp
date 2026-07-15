#include "TurnoutControl.h"

TurnoutControl::TurnoutControl(Button& throwButton,
                                Button& closeButton,
                                Turnout& turnout,
                                TurnoutIndicator& indicator,
                                TurnoutCommandPort& turnoutCommandPort)
    : throwButton_(throwButton)
    , closeButton_(closeButton)
    , turnout_(turnout)
    , indicator_(indicator)
    , turnoutCommandPort_(turnoutCommandPort)
{
}

void TurnoutControl::update()
{
    if (throwButton_.wasPressed())
    {
        turnoutCommandPort_.send(turnout_.address(), TurnoutPosition::Thrown);
    }

    if (closeButton_.wasPressed())
    {
        turnoutCommandPort_.send(turnout_.address(), TurnoutPosition::Closed);
    }
}

void TurnoutControl::applyFeedback(const TurnoutFeedback feedback)
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
