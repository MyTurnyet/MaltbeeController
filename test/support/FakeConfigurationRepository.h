#pragma once

#include "ports/ConfigurationRepository.h"

class FakeConfigurationRepository final : public ConfigurationRepository
{
public:
    bool failNextSave = false;

    MegaConfig load() override
    {
        return stored_;
    }

    bool save(const MegaConfig& config) override
    {
        if (failNextSave)
        {
            failNextSave = false;
            return false;
        }
        stored_ = config;
        return true;
    }

private:
    MegaConfig stored_;
};
