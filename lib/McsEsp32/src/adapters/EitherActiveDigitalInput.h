#pragma once

#include "ports/DigitalInput.h"

// Active when either of the two composed inputs is active. Exactly two
// inputs, not a variadic list — the one need this exists for ("either
// the BOOT or the dedicated SETUP button") is for precisely two.
class EitherActiveDigitalInput final : public DigitalInput
{
public:
    EitherActiveDigitalInput(DigitalInput& first, DigitalInput& second);

    [[nodiscard]] bool isActive() const override;

private:
    DigitalInput& first_;
    DigitalInput& second_;
};
