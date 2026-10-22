#ifdef ARDUINO

#include "CaptivePortalServer.h"

template <int N>
BasicCaptivePortalServer<N>::BasicCaptivePortalServer(BasicWebFormCommissioningAdapter<N>& adapter)
    : adapter_(adapter)
{
}

template <int N>
void BasicCaptivePortalServer<N>::begin(const std::string& apName)
{
    WiFi.softAP(apName.c_str());

    IPAddress apIp = WiFi.softAPIP();
    dnsServer_.start(53, "*", apIp);

    webServer_.on("/", [this]() { handleRoot(); });
    webServer_.on("/submit", HTTP_POST, [this]() { handleSubmit(); });
    webServer_.on("/rescan", [this]() { handleRescan(); });
    webServer_.onNotFound([this]() { handleRoot(); });
    webServer_.begin();

    scanNetworks();
}

template <int N>
void BasicCaptivePortalServer<N>::poll()
{
    dnsServer_.processNextRequest();
    webServer_.handleClient();
}

template <int N>
void BasicCaptivePortalServer<N>::handleRoot()
{
    webServer_.send(200, "text/html; charset=utf-8",
                     SetupFormRenderer::render(adapter_.currentValues(), scannedNetworks_).c_str());
}

template <int N>
void BasicCaptivePortalServer<N>::handleSubmit()
{
    const BasicWebFormSubmission<N> form = readForm();
    const std::string response = adapter_.submit(form);
    webServer_.send(200, "text/plain", response.c_str());
}

template <int N>
void BasicCaptivePortalServer<N>::handleRescan()
{
    scanNetworks();
    webServer_.sendHeader("Location", "/");
    webServer_.send(302);
}

template <int N>
void BasicCaptivePortalServer<N>::scanNetworks()
{
    std::vector<ScannedNetwork> raw;
    const int16_t count = WiFi.scanNetworks();
    for (int16_t i = 0; i < count; ++i)
    {
        raw.push_back({std::string(WiFi.SSID(i).c_str()), WiFi.RSSI(i)});
    }
    scannedNetworks_ = WifiScanFormatter::dedupeAndSort(raw);
}

template <int N>
BasicWebFormSubmission<N> BasicCaptivePortalServer<N>::readForm()
{
    BasicWebFormSubmission<N> form;
    form.nodeId = webServer_.arg("id").c_str();
    form.wifiSsid = webServer_.arg("wifi_ssid").c_str();
    form.wifiPassword = webServer_.arg("wifi_password").c_str();
    form.brokerHost = webServer_.arg("broker_host").c_str();
    form.brokerPort = webServer_.arg("broker_port").c_str();

    for (int i = 0; i < N; ++i)
    {
        const std::string fieldName = "t" + std::to_string(i + 1) + "_address";
        form.channelTurnoutAddresses[i] = webServer_.arg(fieldName.c_str()).c_str();
    }

    return form;
}

template class BasicCaptivePortalServer<12>;
template class BasicCaptivePortalServer<4>;

#endif
