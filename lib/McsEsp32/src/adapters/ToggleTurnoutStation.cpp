#include "ToggleTurnoutStation.h"

ToggleTurnoutStation::ToggleTurnoutStation(const int address, const char* name, DigitalInput& button,
                                            DigitalOutput& closedOutput, DigitalOutput& thrownOutput,
                                            Clock& clock, TurnoutCommandPort& commandPort)
    : turnout_(address, name, TurnoutPosition::Closed, false, false)
    , button_(button, clock, DEBOUNCE_MS)
    , closedIndicator_(closedOutput)
    , thrownIndicator_(thrownOutput)
    , turnoutIndicator_(thrownIndicator_, closedIndicator_)
    , control_(button_, turnout_, turnoutIndicator_, commandPort)
{
}

void ToggleTurnoutStation::begin()
{
    turnoutIndicator_.clear();
}

void ToggleTurnoutStation::update()
{
    button_.update();
    control_.update();
}

void ToggleTurnoutStation::applyFeedback(const TurnoutFeedback feedback)
{
    control_.applyFeedback(feedback);
}

void ToggleTurnoutStation::clearIndicator()
{
    turnoutIndicator_.clear();
}
