#pragma once

#include "domain/NodeConfig.h"
#include "ports/ConfigStore.h"

class FakeConfigStore final : public ConfigStore
{
public:
    int saveCount = 0;
    bool failNextSave = false;

    NodeConfig load() override
    {
        return stored_;
    }

    bool save(const NodeConfig& config) override
    {
        if (failNextSave)
        {
            failNextSave = false;
            return false;
        }
        stored_ = config;
        saveCount++;
        return true;
    }

private:
    NodeConfig stored_ = NodeConfig::factoryDefault();
};
