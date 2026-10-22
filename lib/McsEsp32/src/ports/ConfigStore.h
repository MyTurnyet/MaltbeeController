#pragma once

#include "../domain/NodeConfig.h"

template <int N>
class BasicConfigStore
{
public:
    virtual ~BasicConfigStore() = default;

    virtual BasicNodeConfig<N> load() = 0;
    virtual bool save(const BasicNodeConfig<N>& config) = 0;
};

using ConfigStore = BasicConfigStore<12>;
using ConfigStore4 = BasicConfigStore<4>;
