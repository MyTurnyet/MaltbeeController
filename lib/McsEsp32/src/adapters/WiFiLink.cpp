#ifdef ARDUINO

#include "WiFiLink.h"

WiFiLink::WiFiLink(Clock &clock, const unsigned long retryIntervalMs)
    : clock_(clock), retryIntervalMs_(retryIntervalMs) {
}

void WiFiLink::begin(const std::string &ssid, const std::string &password) {
    ssid_ = ssid;
    password_ = password;

    // A prior wireless-setup session (slice 2c's captive portal) may call
    // WiFi.softAP(), and esp_wifi persists mode/config to flash by default -
    // so a leftover AP interface can still be active on a later boot,
    // including this one. Force STA-only before the first connect attempt.
    WiFi.mode(WIFI_STA);

    connect();
}

void WiFiLink::poll() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!wasConnected_) {
            Serial.print("WiFi connected, IP: ");
            Serial.println(WiFi.localIP());
            wasConnected_ = true;
        }
        return;
    }

    wasConnected_ = false;

    if (clock_.nowMilliseconds() - lastAttemptMs_ >= retryIntervalMs_) {
        connect();
    }
}

bool WiFiLink::connected() const {
    return WiFi.status() == WL_CONNECTED;
}

void WiFiLink::connect() {
    WiFi.begin(ssid_.c_str(), password_.c_str());
    lastAttemptMs_ = clock_.nowMilliseconds();
}

#endif
