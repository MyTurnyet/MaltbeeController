#pragma once

#include "MegaStationConfig.h"

struct MegaConfig
{
    static constexpr int kStationCount = 4;
    static constexpr int kMinAddress = 1;
    static constexpr int kMaxAddress = 2048;

    MegaStationConfig stations[kStationCount];

    [[nodiscard]] bool validate() const;
};
