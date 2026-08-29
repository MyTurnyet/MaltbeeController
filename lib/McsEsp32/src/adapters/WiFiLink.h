#pragma once

#ifdef ARDUINO

#include <WiFi.h>

#include <string>

#include "ports/Clock.h"

class WiFiLink
{
public:
    WiFiLink(Clock& clock, unsigned long retryIntervalMs);

    void begin(const std::string& ssid, const std::string& password);
    void poll();
    [[nodiscard]] bool connected() const;

private:
    void connect();

    Clock& clock_;
    unsigned long retryIntervalMs_;
    unsigned long lastAttemptMs_ = 0;
    std::string ssid_;
    std::string password_;
};

#endif
