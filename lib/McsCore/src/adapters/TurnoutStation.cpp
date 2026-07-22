#ifdef ARDUINO

#include "TurnoutStation.h"

TurnoutStation::TurnoutStation(const TurnoutConfig& config, Clock& clock, TurnoutCommandPort& commandPort)
    : throwInput_(config.throwButtonPin, true, true)
    , closeInput_(config.closeButtonPin, true, true)
    , thrownOutput_(config.thrownLedPin, false)
    , closedOutput_(config.closedLedPin, false)
    , turnout_(config.address, config.name, TurnoutPosition::Closed, false, false)
    , throwButton_(throwInput_, clock, DEBOUNCE_MS)
    , closeButton_(closeInput_, clock, DEBOUNCE_MS)
    , thrownIndicator_(thrownOutput_)
    , closedIndicator_(closedOutput_)
    , turnoutIndicator_(thrownIndicator_, closedIndicator_)
    , control_(throwButton_, closeButton_, turnout_, turnoutIndicator_, commandPort)
{
}

void TurnoutStation::begin()
{
    throwInput_.begin();
    closeInput_.begin();
    thrownOutput_.begin();
    closedOutput_.begin();
}

void TurnoutStation::update()
{
    throwButton_.update();
    closeButton_.update();
    control_.update();
}

void TurnoutStation::applyFeedback(const TurnoutFeedback feedback)
{
    control_.applyFeedback(feedback);
}

#endif
