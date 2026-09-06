#ifdef ARDUINO

#include "EspMdnsResolver.h"

#include <ESPmDNS.h>
#include <WiFi.h>

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
        if (!MDNS.begin("maltbee-resolver"))
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
