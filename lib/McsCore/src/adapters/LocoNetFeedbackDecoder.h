#pragma once

#include "../ports/LocoNetFeedbackSource.h"
#include "../ports/TurnoutCommandPort.h"

struct TurnoutFeedbackLookup
{
    bool found;
    TurnoutFeedback feedback;
};

class LocoNetFeedbackDecoder
{
public:
    TurnoutFeedbackLookup decode(const SwitchOutputsReport& report) const;
};
