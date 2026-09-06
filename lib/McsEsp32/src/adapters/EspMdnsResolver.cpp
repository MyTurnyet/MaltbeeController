#ifdef ARDUINO

#include "EspMdnsResolver.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <utility>

EspMdnsResolver::EspMdnsResolver(std::string selfHostname) : selfHostname_(std::move(selfHostname))
{
}

std::optional<std::string> EspMdnsResolver::resolveHost(const std::string& hostname)
{
    const unsigned long waitStart = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - waitStart >= kWifiWaitTimeoutMs)
        {
            return std::nullopt;
        }
        delay(100);
    }

    if (!mdnsStarted_)
    {
        if (!MDNS.begin(selfHostname_.c_str()))
        {
            return std::nullopt;
        }
        mdnsStarted_ = true;
    }

    const IPAddress resolved = MDNS.queryHost(hostname.c_str());
    if (resolved == IPAddress(0, 0, 0, 0))
    {
        return std::nullopt;
    }

    return std::string(resolved.toString().c_str());
}

#endif
