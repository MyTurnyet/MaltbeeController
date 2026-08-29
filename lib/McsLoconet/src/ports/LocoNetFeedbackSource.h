#pragma once

struct SwitchOutputsReport
{
    int address;
    bool closedOutputOn;
    bool thrownOutputOn;
};

class LocoNetFeedbackSource
{
public:
    virtual ~LocoNetFeedbackSource() = default;

    virtual bool poll(SwitchOutputsReport& outReport) = 0;
};
