#pragma once

#include <string>

#include "MacAddress.h"

class SetupApName
{
public:
    static std::string from(const MacAddress& mac)
    {
        return "MaltBee-Setup-" + mac.lastFourHexDigits();
    }
};
