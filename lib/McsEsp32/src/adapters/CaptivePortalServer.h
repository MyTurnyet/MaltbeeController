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

template <int N>
class BasicCaptivePortalServer
{
public:
    explicit BasicCaptivePortalServer(BasicWebFormCommissioningAdapter<N>& adapter);

    void begin(const std::string& apName);
    void poll();

private:
    void handleRoot();
    void handleSubmit();
    void handleRescan();
    void scanNetworks();
    BasicWebFormSubmission<N> readForm();

    BasicWebFormCommissioningAdapter<N>& adapter_;
    DNSServer dnsServer_;
    WebServer webServer_{80};
    std::vector<ScannedNetwork> scannedNetworks_;
};

using CaptivePortalServer = BasicCaptivePortalServer<12>;
using CaptivePortalServer4 = BasicCaptivePortalServer<4>;

#endif
