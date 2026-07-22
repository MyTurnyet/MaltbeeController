#pragma once

#include "../ports/LocoNetFeedbackSource.h"

class MrrwaLocoNetFeedbackSource final : public LocoNetFeedbackSource
{
public:
    bool poll(SwitchOutputsReport& outReport) override;
};
