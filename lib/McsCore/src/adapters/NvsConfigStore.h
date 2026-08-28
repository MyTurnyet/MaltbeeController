#pragma once

#include "../ports/ConfigStore.h"

class NvsConfigStore final : public ConfigStore
{
public:
    NodeConfig load() override;
    void save(const NodeConfig& config) override;
};
