#pragma once

#ifdef ARDUINO

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include <string>
#include <vector>

#include "WebFormCommissioningAdapter.h"
#include "../domain/SetupFormRenderer.h"
#include "../domain/WifiScanFormatter.h"

class CaptivePortalServer
{
public:
    explicit CaptivePortalServer(WebFormCommissioningAdapter& adapter);

    void begin(const std::string& apName);
    void poll();

private:
    void handleRoot();
    void handleSubmit();
    void handleRescan();
    void scanNetworks();
    WebFormSubmission readForm();

    WebFormCommissioningAdapter& adapter_;
    DNSServer dnsServer_;
    WebServer webServer_{80};
    std::vector<ScannedNetwork> scannedNetworks_;
};

#endif
