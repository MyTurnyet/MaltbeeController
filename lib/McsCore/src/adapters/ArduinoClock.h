#pragma once

#include "../ports/Clock.h"

class ArduinoClock final : public Clock
{
public:
    [[nodiscard]] unsigned long nowMilliseconds() const override;
};
