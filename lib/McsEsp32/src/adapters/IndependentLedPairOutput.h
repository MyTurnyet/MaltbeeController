#pragma once

#include "../domain/IndependentLedPairDriver.h"
#include "ports/DigitalOutput.h"

class IndependentLedPairOutput final : public DigitalOutput
{
public:
    IndependentLedPairOutput(IndependentLedPairDriver& driver, LedPairColor color);

    void set(bool active) override;
    [[nodiscard]] bool isSet() const override;

private:
    IndependentLedPairDriver& driver_;
    LedPairColor color_;
};
