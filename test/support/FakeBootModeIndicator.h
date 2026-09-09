#pragma once

#include "ports/BootModeIndicator.h"

class FakeBootModeIndicator : public BootModeIndicator
{
public:
    void show(BootMode mode) override
    {
        lastShown_ = mode;
    }

    void update() override
    {
        updateCalls_++;
    }

    BootMode lastShown() const
    {
        return lastShown_;
    }

    int updateCalls() const
    {
        return updateCalls_;
    }

private:
    BootMode lastShown_ = BootMode::Normal;
    int updateCalls_ = 0;
};
