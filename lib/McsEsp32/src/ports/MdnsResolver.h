#pragma once

#include <optional>
#include <string>

class MdnsResolver
{
public:
    virtual ~MdnsResolver() = default;

    // hostname is the bare mDNS name without ".local", e.g. "loco2mqtt".
    virtual std::optional<std::string> resolveHost(const std::string& hostname) = 0;
};
