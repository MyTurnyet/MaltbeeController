#include "MegaConfig.h"

bool MegaConfig::validate() const
{
    for (int i = 0; i < kStationCount; ++i)
    {
        const int address = stations[i].address;
        if (address < kMinAddress || address > kMaxAddress)
        {
            return false;
        }

        for (int j = i + 1; j < kStationCount; ++j)
        {
            if (stations[j].address == address)
            {
                return false;
            }
        }
    }

    return true;
}
