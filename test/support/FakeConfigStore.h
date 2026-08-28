#pragma once

#include "domain/NodeConfig.h"
#include "ports/ConfigStore.h"

class FakeConfigStore final : public ConfigStore
{
public:
    int saveCount = 0;

    NodeConfig load() override
    {
        return stored_;
    }

    void save(const NodeConfig& config) override
    {
        stored_ = config;
        saveCount++;
    }

private:
    NodeConfig stored_ = NodeConfig::factoryDefault();
};
