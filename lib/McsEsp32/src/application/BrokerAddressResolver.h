#pragma once

#include <string>

#include "../ports/MdnsResolver.h"

class BrokerAddressResolver
{
public:
    explicit BrokerAddressResolver(MdnsResolver& resolver);

    [[nodiscard]] std::string resolve(const std::string& mdnsHostname, const std::string& fallbackHost) const;

private:
    MdnsResolver& resolver_;
};
