#pragma once

#include <string>
#include <vector>

#include "../domain/NodeConfig.h"
#include "../domain/ParsedCommand.h"
#include "../ports/ConfigStore.h"

class CommissioningSession
{
public:
    explicit CommissioningSession(ConfigStore& store);

    std::string apply(const ParsedCommand& command);

    [[nodiscard]] bool rebootRequested() const;

private:
    [[nodiscard]] std::string formatShow() const;
    [[nodiscard]] std::string formatErrors(const std::vector<std::string>& errors) const;

    ConfigStore& store_;
    NodeConfig draft_;
    bool rebootRequested_ = false;
};
