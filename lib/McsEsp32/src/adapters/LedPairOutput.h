#pragma once

#include "../domain/LedPairDriver.h"
#include "ports/DigitalOutput.h"

class LedPairOutput final : public DigitalOutput
{
public:
    LedPairOutput(LedPairDriver& driver, LedPairColor color);

    void set(bool active) override;
    [[nodiscard]] bool isSet() const override;

private:
    LedPairDriver& driver_;
    LedPairColor color_;
};
