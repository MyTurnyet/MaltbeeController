#pragma once

#include "../ports/ConfigStore.h"

class NvsConfigStore final : public ConfigStore
{
public:
    NodeConfig load() override;
    bool save(const NodeConfig& config) override;
};
