#pragma once

#include "domain/NodeConfig.h"
#include "ports/ConfigStore.h"

class FakeConfigStore4 final : public ConfigStore4
{
public:
    int saveCount = 0;
    bool failNextSave = false;

    NodeConfig4 load() override
    {
        return stored_;
    }

    bool save(const NodeConfig4& config) override
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
    NodeConfig4 stored_ = NodeConfig4::factoryDefault();
};
