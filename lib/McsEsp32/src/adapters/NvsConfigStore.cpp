#ifdef ARDUINO

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
        config.channelTurnoutAddresses[i] = prefs.getInt(key.c_str(), 0);
    }

    prefs.end();
    return config;
}

bool NvsConfigStore::save(const NodeConfig& config)
{
    Preferences prefs;
    bool ok = prefs.begin(kNamespace, false);

    ok = prefs.putInt(kKeyNodeId, config.nodeId) > 0 && ok;
    // putString()'s return (bytes written) is not checked here: ESP-IDF's
    // Preferences::putString() returns 0 both on failure AND when writing a
    // legitimate empty string (e.g. an unconfigured wifiPassword, which
    // defaults to "" and is valid per NodeConfig::validate()'s
    // partial-commissioning rules). ANDing that into `ok` would report
    // failure on every normal partial-commissioning save, so only
    // prefs.begin() and the putInt calls (which always write a fixed
    // non-zero byte count on success, regardless of the value stored —
    // unlike putString, a stored 0 is not ambiguous with failure) are
    // trusted.
    prefs.putString(kKeyWifiSsid, config.wifiSsid.c_str());
    prefs.putString(kKeyWifiPassword, config.wifiPassword.c_str());
    prefs.putString(kKeyBrokerHost, config.brokerHost.c_str());
    ok = prefs.putInt(kKeyBrokerPort, config.brokerPort) > 0 && ok;

    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string key = std::string(kKeyChannelPrefix) + std::to_string(i);
        ok = prefs.putInt(key.c_str(), config.channelTurnoutAddresses[i]) > 0 && ok;
    }

    prefs.end();
    return ok;
}

#endif
