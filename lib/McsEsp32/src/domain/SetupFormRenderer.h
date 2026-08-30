#pragma once

#include <string>

#include "NodeConfig.h"
#include "WebFormSubmission.h"

class SetupFormRenderer
{
public:
    static std::string escapeHtml(const std::string& text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (char c : text)
        {
            switch (c)
            {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&#39;";
                break;
            default:
                escaped += c;
            }
        }
        return escaped;
    }

    static std::string render(const WebFormSubmission& values)
    {
        std::string html;
        html += "<!DOCTYPE html><html><head>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>" + kStyle + "</style></head><body>";
        html += "<div class='card'><h1>MaltBee Panel Setup</h1>";
        html += "<p class='subtitle'>Configure this panel's network settings</p>";
        html += "<form method='POST' action='/submit'>";
        html += "<label>Node ID</label><select name='id'>" + renderIdOptions(values.nodeId) + "</select>";
        html += "<label>WiFi SSID</label><input name='wifi_ssid' value='" + escapeHtml(values.wifiSsid) + "'>";
        html += "<label>WiFi Password</label><input name='wifi_password' type='password' value='"
            + escapeHtml(values.wifiPassword) + "'>";
        html += "<label>Broker Host</label><input name='broker_host' value='" + escapeHtml(values.brokerHost) + "'>";
        html += "<label>Broker Port</label><input name='broker_port' type='number' value='"
            + escapeHtml(values.brokerPort) + "'>";
        html += "<details><summary>Turnout JMRI Names</summary>";
        html += "<p class='warning'>Leave a channel blank to leave it unconfigured.</p>";
        for (int i = 0; i < NodeConfig::kChannelCount; ++i)
        {
            html += renderChannelField(i + 1, values.channelJmriNames[i]);
        }
        html += "</details>";
        html += "<button type='submit'>Save</button>";
        html += "</form></div></body></html>";
        return html;
    }

private:
    static std::string renderIdOptions(const std::string& selectedId)
    {
        std::string options;
        if (selectedId.empty())
        {
            options += "<option value='' disabled selected hidden>-- select --</option>";
        }
        for (int i = NodeConfig::kMinNodeId; i <= NodeConfig::kMaxNodeId; ++i)
        {
            const std::string value = std::to_string(i);
            options += "<option value='" + value + "'" + (value == selectedId ? " selected" : "") + ">" + value
                + "</option>";
        }
        return options;
    }

    static std::string renderChannelField(int channelNumber, const std::string& jmriName)
    {
        const std::string fieldName = "t" + std::to_string(channelNumber) + "_name";
        std::string html;
        html += "<label>Turnout " + std::to_string(channelNumber) + " JMRI Name</label>";
        html += "<input name='" + fieldName + "' value='" + escapeHtml(jmriName) + "'>";
        return html;
    }

    static inline const std::string kStyle =
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
        "background:#f2f4f7;margin:0;padding:24px 16px;color:#1f2933;}"
        ".card{max-width:420px;margin:0 auto;background:#fff;border-radius:12px;"
        "box-shadow:0 1px 3px rgba(0,0,0,0.1);padding:24px;}"
        "h1{font-size:1.25rem;margin:0 0 4px;}"
        ".subtitle{color:#6b7280;font-size:0.875rem;margin:0 0 20px;}"
        "label{display:block;font-size:0.8rem;font-weight:600;color:#374151;margin:16px 0 4px;}"
        "input,select{width:100%;box-sizing:border-box;padding:8px 10px;border:1px solid #d1d5db;"
        "border-radius:6px;font-size:0.95rem;}"
        "button{margin-top:24px;width:100%;padding:10px;background:#2563eb;color:#fff;"
        "border:none;border-radius:6px;font-size:1rem;font-weight:600;cursor:pointer;}"
        "button:hover{background:#1d4ed8;}"
        "details{margin-top:20px;}"
        "summary{cursor:pointer;font-size:0.85rem;font-weight:600;color:#374151;}"
        ".warning{color:#b45309;font-size:0.8rem;margin:8px 0 0;}";
};
