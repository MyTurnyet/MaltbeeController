#include "LocoNetFeedbackDecoder.h"

TurnoutFeedbackLookup LocoNetFeedbackDecoder::decode(const SwitchOutputsReport& report) const
{
    if (report.closedOutputOn == report.thrownOutputOn)
    {
        return {false, {0, TurnoutPosition::Closed}};
    }

    const TurnoutPosition position = report.thrownOutputOn ? TurnoutPosition::Thrown : TurnoutPosition::Closed;
    return {true, {report.address, position}};
}
