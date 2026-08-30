#pragma once

#include "ports/SetupModeRequestStore.h"

class FakeSetupModeRequestStore final : public SetupModeRequestStore
{
public:
    int requestOnNextBootCallCount = 0;

    bool requestOnNextBoot() override
    {
        requested_ = true;
        requestOnNextBootCallCount++;
        return true;
    }

    bool consumeRequest() override
    {
        const bool pending = requested_;
        requested_ = false;
        return pending;
    }

private:
    bool requested_ = false;
};
