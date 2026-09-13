#include "BrokerAddressResolver.h"

BrokerAddressResolver::BrokerAddressResolver(MdnsResolver& resolver, Clock& clock,
                                              const unsigned long minResolveIntervalMs)
    : resolver_(resolver), clock_(clock), minResolveIntervalMs_(minResolveIntervalMs)
{
}

std::string BrokerAddressResolver::resolve(const std::string& mdnsHostname, const std::string& fallbackHost)
{
    if (shouldQueryNow())
    {
        queryAndCache(mdnsHostname);
    }

    return cachedResolvedHost_.value_or(fallbackHost);
}

bool BrokerAddressResolver::shouldQueryNow() const
{
    if (!hasQueried_)
    {
        return true;
    }

    return clock_.nowMilliseconds() - lastAttemptMs_ >= minResolveIntervalMs_;
}

void BrokerAddressResolver::queryAndCache(const std::string& mdnsHostname)
{
    hasQueried_ = true;
    lastAttemptMs_ = clock_.nowMilliseconds();
    cachedResolvedHost_ = resolver_.resolveHost(mdnsHostname);
}
