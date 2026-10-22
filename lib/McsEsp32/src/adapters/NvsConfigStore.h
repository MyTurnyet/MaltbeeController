#pragma once

#include "../ports/ConfigStore.h"

template <int N>
class BasicNvsConfigStore final : public BasicConfigStore<N>
{
public:
    BasicNodeConfig<N> load() override;
    bool save(const BasicNodeConfig<N>& config) override;
};

using NvsConfigStore = BasicNvsConfigStore<12>;
using NvsConfigStore4 = BasicNvsConfigStore<4>;
