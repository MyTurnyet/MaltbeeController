#pragma once

#ifdef ARDUINO

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include <string>

#include "WebFormCommissioningAdapter.h"
#include "../domain/SetupFormRenderer.h"

class CaptivePortalServer
{
public:
    explicit CaptivePortalServer(WebFormCommissioningAdapter& adapter);

    void begin(const std::string& apName, const std::string& passphrase);
    void poll();

private:
    void handleRoot();
    void handleSubmit();
    WebFormSubmission readForm();

    WebFormCommissioningAdapter& adapter_;
    DNSServer dnsServer_;
    WebServer webServer_{80};
};

#endif
