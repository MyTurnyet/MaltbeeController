#pragma once

#include <optional>
#include <string>

#include "../ports/MdnsResolver.h"
#include "ports/Clock.h"

// mDNS resolution is a blocking hardware call (see EspMdnsResolver) - it
// must never run on every MqttLink retry, or a disconnected broker freezes
// the whole panel's loop() every retryIntervalMs. This throttles actual
// resolver_.resolveHost() calls to at most once per minResolveIntervalMs,
// still self-healing a broker IP change without a reboot, just not on
// every single connect attempt.
class BrokerAddressResolver
{
public:
    BrokerAddressResolver(MdnsResolver& resolver, Clock& clock, unsigned long minResolveIntervalMs);

    [[nodiscard]] std::string resolve(const std::string& mdnsHostname, const std::string& fallbackHost);

private:
    [[nodiscard]] bool shouldQueryNow() const;
    void queryAndCache(const std::string& mdnsHostname);

    MdnsResolver& resolver_;
    Clock& clock_;
    unsigned long minResolveIntervalMs_;
    bool hasQueried_ = false;
    unsigned long lastAttemptMs_ = 0;
    std::optional<std::string> cachedResolvedHost_;
};
