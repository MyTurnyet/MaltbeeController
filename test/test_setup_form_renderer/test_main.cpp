#include <catch2/catch_test_macros.hpp>

#include "domain/SetupFormRenderer.h"

namespace
{
    WebFormSubmission emptyValues()
    {
        WebFormSubmission form;
        return form;
    }
}

TEST_CASE("escapeHtml escapes each HTML-significant character")
{
    REQUIRE(SetupFormRenderer::escapeHtml("&") == "&amp;");
    REQUIRE(SetupFormRenderer::escapeHtml("<") == "&lt;");
    REQUIRE(SetupFormRenderer::escapeHtml(">") == "&gt;");
    REQUIRE(SetupFormRenderer::escapeHtml("\"") == "&quot;");
    REQUIRE(SetupFormRenderer::escapeHtml("'") == "&#39;");
}

TEST_CASE("escapeHtml leaves ordinary characters untouched")
{
    REQUIRE(SetupFormRenderer::escapeHtml("MyLayoutWifi123") == "MyLayoutWifi123");
}

TEST_CASE("render embeds the escaped wifi ssid value")
{
    WebFormSubmission form = emptyValues();
    form.wifiSsid = "My\"Wifi";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("My&quot;Wifi") != std::string::npos);
}

TEST_CASE("render never embeds a wifi password, even when one is set")
{
    WebFormSubmission form = emptyValues();
    form.wifiPassword = "pass&word";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("name='wifi_password' type='password' value=''") != std::string::npos);
    REQUIRE(html.find("pass&word") == std::string::npos);
    REQUIRE(html.find("pass&amp;word") == std::string::npos);
}

TEST_CASE("render includes a hint that a blank password keeps the current one")
{
    const std::string html = SetupFormRenderer::render(emptyValues());

    REQUIRE(html.find("Leave blank to keep the current password.") != std::string::npos);
}

TEST_CASE("render embeds the broker host and port values")
{
    WebFormSubmission form = emptyValues();
    form.brokerHost = "192.168.1.50";
    form.brokerPort = "1883";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("192.168.1.50") != std::string::npos);
    REQUIRE(html.find("value='1883'") != std::string::npos);
}

TEST_CASE("render's id dropdown spans the full valid node id range")
{
    const std::string html = SetupFormRenderer::render(emptyValues());

    REQUIRE(html.find(">1</option>") != std::string::npos);
    REQUIRE(html.find(">99</option>") != std::string::npos);
    REQUIRE(html.find(">100</option>") == std::string::npos);
    REQUIRE(html.find(">0</option>") == std::string::npos);
}

TEST_CASE("render's selected node id is marked selected")
{
    WebFormSubmission form = emptyValues();
    form.nodeId = "7";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("value='7' selected") != std::string::npos);
}

TEST_CASE("render includes a labeled input for each of the 12 turnout channels")
{
    WebFormSubmission form = emptyValues();
    form.channelJmriNames[0] = "LT1";
    form.channelJmriNames[11] = "LT12";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("Turnout 1 JMRI Name") != std::string::npos);
    REQUIRE(html.find("name='t1_name'") != std::string::npos);
    REQUIRE(html.find("value='LT1'") != std::string::npos);
    REQUIRE(html.find("Turnout 12 JMRI Name") != std::string::npos);
    REQUIRE(html.find("name='t12_name'") != std::string::npos);
    REQUIRE(html.find("value='LT12'") != std::string::npos);
}
