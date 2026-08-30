#pragma once

#ifdef ARDUINO

#include "../ports/SetupModeRequestStore.h"

class NvsSetupModeRequestStore final : public SetupModeRequestStore
{
public:
    bool requestOnNextBoot() override;
    bool consumeRequest() override;
};

#endif
