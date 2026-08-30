#pragma once

#include "BootMode.h"
#include "NodeConfig.h"

class BootModeSelector
{
public:
    static BootMode select(const NodeConfig& config, const bool wirelessSetupRequested)
    {
        if (wirelessSetupRequested)
        {
            return BootMode::WirelessSetup;
        }

        return config.validate().empty() ? BootMode::Normal : BootMode::NeedsCommissioning;
    }
};
