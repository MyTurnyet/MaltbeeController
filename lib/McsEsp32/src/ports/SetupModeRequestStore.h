#pragma once

class SetupModeRequestStore
{
public:
    virtual ~SetupModeRequestStore() = default;

    virtual bool requestOnNextBoot() = 0;
    virtual bool consumeRequest() = 0;
};
