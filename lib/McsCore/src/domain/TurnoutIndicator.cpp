#include "TurnoutIndicator.h"

TurnoutIndicator::TurnoutIndicator(Indicator& thrownIndicator, Indicator& closedIndicator)
    : thrownIndicator_(thrownIndicator), closedIndicator_(closedIndicator)
{
}

void TurnoutIndicator::display(const TurnoutPosition position)
{
    if (position == TurnoutPosition::Thrown)
    {
        thrownIndicator_.on();
        closedIndicator_.off();
    }
    else
    {
        closedIndicator_.on();
        thrownIndicator_.off();
    }
}

void TurnoutIndicator::clear()
{
    thrownIndicator_.off();
    closedIndicator_.off();
}
