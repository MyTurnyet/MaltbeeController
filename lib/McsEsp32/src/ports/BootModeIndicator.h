#pragma once

#include "../domain/BootMode.h"

class BootModeIndicator
{
public:
    virtual ~BootModeIndicator() = default;
    virtual void show(BootMode mode) = 0;
    virtual void update() = 0;
};
