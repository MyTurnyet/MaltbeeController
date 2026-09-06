#pragma once

#ifdef ARDUINO

#include <optional>
#include <string>

#include "ports/MdnsResolver.h"

class EspMdnsResolver final : public MdnsResolver
{
public:
    explicit EspMdnsResolver(std::string selfHostname);

    std::optional<std::string> resolveHost(const std::string& hostname) override;

private:
    static constexpr unsigned long kWifiWaitTimeoutMs = 8000;
    std::string selfHostname_;
    bool mdnsStarted_ = false;
};

#endif
