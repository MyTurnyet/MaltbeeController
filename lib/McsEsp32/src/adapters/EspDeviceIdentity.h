#pragma once

#ifdef ARDUINO

#include "../domain/MacAddress.h"

class EspDeviceIdentity
{
public:
    [[nodiscard]] MacAddress mac() const;
};

#endif
