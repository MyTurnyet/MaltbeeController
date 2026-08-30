#pragma once

class SetupModeRequestStore
{
public:
    virtual ~SetupModeRequestStore() = default;

    virtual void requestOnNextBoot() = 0;
    virtual bool consumeRequest() = 0;
};
