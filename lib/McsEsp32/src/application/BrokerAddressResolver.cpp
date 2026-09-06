#include "BrokerAddressResolver.h"

BrokerAddressResolver::BrokerAddressResolver(MdnsResolver& resolver) : resolver_(resolver)
{
}

std::string BrokerAddressResolver::resolve(const std::string& mdnsHostname, const std::string& fallbackHost) const
{
    const std::optional<std::string> resolved = resolver_.resolveHost(mdnsHostname);
    return resolved.value_or(fallbackHost);
}
