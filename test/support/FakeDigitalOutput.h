#pragma once

#include "ports/DigitalOutput.h"

class FakeDigitalOutput final : public DigitalOutput
{
public:
    void set(const bool active) override
    {
        active_ = active;
        setCallCount_++;
    }

    [[nodiscard]] bool isSet() const override
    {
        return active_;
    }

    [[nodiscard]] int setCallCount() const
    {
        return setCallCount_;
    }

private:
    bool active_ = false;
    int setCallCount_ = 0;
};
