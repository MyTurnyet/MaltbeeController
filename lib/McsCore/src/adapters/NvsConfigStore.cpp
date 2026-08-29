#ifdef ESP32

#include "NvsConfigStore.h"

#include <Preferences.h>

namespace
{
    constexpr const char* kNamespace = "mcsnode";
    constexpr const char* kKeyNodeId = "nodeId";
    constexpr const char* kKeyWifiSsid = "wifiSsid";
    constexpr const char* kKeyWifiPassword = "wifiPw";
    constexpr const char* kKeyBrokerHost = "brokerHost";
    constexpr const char* kKeyBrokerPort = "brokerPort";
    constexpr const char* kKeyChannelPrefix = "ch";
}

NodeConfig NvsConfigStore::load()
{
    Preferences prefs;
    prefs.begin(kNamespace, true);

    NodeConfig config = NodeConfig::factoryDefault();
    config.nodeId = prefs.getInt(kKeyNodeId, 0);
    config.wifiSsid = prefs.getString(kKeyWifiSsid, "").c_str();
    config.wifiPassword = prefs.getString(kKeyWifiPassword, "").c_str();
    config.brokerHost = prefs.getString(kKeyBrokerHost, "").c_str();
    config.brokerPort = prefs.getInt(kKeyBrokerPort, 1883);

    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string key = std::string(kKeyChannelPrefix) + std::to_string(i);
        config.channelJmriNames[i] = prefs.getString(key.c_str(), "").c_str();
    }

    prefs.end();
    return config;
}

void NvsConfigStore::save(const NodeConfig& config)
{
    Preferences prefs;
    prefs.begin(kNamespace, false);

    prefs.putInt(kKeyNodeId, config.nodeId);
    prefs.putString(kKeyWifiSsid, config.wifiSsid.c_str());
    prefs.putString(kKeyWifiPassword, config.wifiPassword.c_str());
    prefs.putString(kKeyBrokerHost, config.brokerHost.c_str());
    prefs.putInt(kKeyBrokerPort, config.brokerPort);

    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string key = std::string(kKeyChannelPrefix) + std::to_string(i);
        prefs.putString(key.c_str(), config.channelJmriNames[i].c_str());
    }

    prefs.end();
}

#endif
