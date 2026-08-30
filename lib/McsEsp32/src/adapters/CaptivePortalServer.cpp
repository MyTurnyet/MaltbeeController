#ifdef ARDUINO

#include "CaptivePortalServer.h"

#include "../domain/NodeConfig.h"

CaptivePortalServer::CaptivePortalServer(WebFormCommissioningAdapter& adapter) : adapter_(adapter)
{
}

void CaptivePortalServer::begin(const std::string& apName, const std::string& passphrase)
{
    WiFi.softAP(apName.c_str(), passphrase.c_str());

    IPAddress apIp = WiFi.softAPIP();
    dnsServer_.start(53, "*", apIp);

    webServer_.on("/", [this]() { handleRoot(); });
    webServer_.on("/submit", HTTP_POST, [this]() { handleSubmit(); });
    webServer_.onNotFound([this]() { handleRoot(); });
    webServer_.begin();
}

void CaptivePortalServer::poll()
{
    dnsServer_.processNextRequest();
    webServer_.handleClient();
}

void CaptivePortalServer::handleRoot()
{
    webServer_.send(200, "text/html", SetupFormRenderer::render(adapter_.currentValues()).c_str());
}

void CaptivePortalServer::handleSubmit()
{
    const WebFormSubmission form = readForm();
    const std::string response = adapter_.submit(form);
    webServer_.send(200, "text/plain", response.c_str());
}

WebFormSubmission CaptivePortalServer::readForm()
{
    WebFormSubmission form;
    form.nodeId = webServer_.arg("id").c_str();
    form.wifiSsid = webServer_.arg("wifi_ssid").c_str();
    form.wifiPassword = webServer_.arg("wifi_password").c_str();
    form.brokerHost = webServer_.arg("broker_host").c_str();
    form.brokerPort = webServer_.arg("broker_port").c_str();

    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string fieldName = "t" + std::to_string(i + 1) + "_name";
        form.channelJmriNames[i] = webServer_.arg(fieldName.c_str()).c_str();
    }

    return form;
}

#endif
