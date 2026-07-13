#pragma once

#include "ports/DigitalInput.h"

class FakeDigitalInput final : public DigitalInput
{
public:
    bool active = false;

    [[nodiscard]] bool isActive() const override
    {
        return active;
    }
};
