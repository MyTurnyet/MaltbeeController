#pragma once

#include "ports/DigitalInput.h"

class GatedDigitalInput final : public DigitalInput
{
public:
    explicit GatedDigitalInput(DigitalInput& inner);

    void setSuppressed(bool suppressed);
    [[nodiscard]] bool isActive() const override;

private:
    DigitalInput& inner_;
    bool suppressed_ = false;
};
