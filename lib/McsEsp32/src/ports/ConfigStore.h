#pragma once

#include "../domain/NodeConfig.h"

class ConfigStore
{
public:
    virtual ~ConfigStore() = default;

    virtual NodeConfig load() = 0;
    virtual bool save(const NodeConfig& config) = 0;
};
