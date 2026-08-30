#pragma once

#ifdef ARDUINO

#include "../ports/SetupModeRequestStore.h"

class NvsSetupModeRequestStore final : public SetupModeRequestStore
{
public:
    void requestOnNextBoot() override;
    bool consumeRequest() override;
};

#endif
