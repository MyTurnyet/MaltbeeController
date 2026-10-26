#pragma once

#include "../ports/ConfigurationRepository.h"

// A real, reusable ConfigurationRepository backed by a plain in-memory
// MegaConfig - not merely a test double. Useful for native tests (no
// EEPROM dependency) and anywhere a caller wants configuration without
// persistence.
class InMemoryConfigurationRepository final : public ConfigurationRepository
{
public:
    MegaConfig load() override
    {
        return stored_;
    }

    bool save(const MegaConfig& config) override
    {
        stored_ = config;
        return true;
    }

private:
    MegaConfig stored_;
};
