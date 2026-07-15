#pragma once

#include "Indicator.h"
#include "Turnout.h"

class TurnoutIndicator
{
public:
    TurnoutIndicator(Indicator& thrownIndicator, Indicator& closedIndicator);

    void display(TurnoutPosition position);
    void clear();

private:
    Indicator& thrownIndicator_;
    Indicator& closedIndicator_;
};
