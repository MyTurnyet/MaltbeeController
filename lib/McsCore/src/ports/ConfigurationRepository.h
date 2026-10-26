#pragma once

#include "../domain/MegaConfig.h"

class ConfigurationRepository
{
public:
    virtual ~ConfigurationRepository() = default;

    virtual MegaConfig load() = 0;
    virtual bool save(const MegaConfig& config) = 0;
};
