#pragma once

#include <optional>
#include <string>

#include "ports/MdnsResolver.h"

class FakeMdnsResolver final : public MdnsResolver
{
public:
    std::optional<std::string> result;
    std::string lastRequestedHostname;

    std::optional<std::string> resolveHost(const std::string& hostname) override
    {
        lastRequestedHostname = hostname;
        return result;
    }
};
