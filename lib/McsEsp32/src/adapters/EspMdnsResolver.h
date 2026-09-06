#pragma once

#ifdef ARDUINO

#include <optional>
#include <string>

#include "ports/MdnsResolver.h"

class EspMdnsResolver final : public MdnsResolver
{
public:
    std::optional<std::string> resolveHost(const std::string& hostname) override;

private:
    static constexpr unsigned long kWifiWaitTimeoutMs = 8000;
    bool mdnsStarted_ = false;
};

#endif
