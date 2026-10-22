#pragma once

#include "BootMode.h"
#include "NodeConfig.h"

class BootModeSelector
{
public:
    template <int N>
    static BootMode select(const BasicNodeConfig<N>& config, const bool wirelessSetupRequested)
    {
        if (wirelessSetupRequested)
        {
            return BootMode::WirelessSetup;
        }

        return config.validate().empty() ? BootMode::Normal : BootMode::NeedsCommissioning;
    }
};
