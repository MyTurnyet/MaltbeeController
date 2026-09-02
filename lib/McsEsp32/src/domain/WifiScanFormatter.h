#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ScannedNetwork
{
    std::string ssid;
    int32_t rssi;
};

class WifiScanFormatter
{
public:
    // Drops entries with an empty ssid (hidden networks), keeps the
    // strongest rssi seen for each remaining ssid, sorts strongest-first.
    static std::vector<ScannedNetwork> dedupeAndSort(const std::vector<ScannedNetwork>& raw);

    // Appends a short signal-strength indicator to ssid, e.g.
    // "MyWifi" -> "MyWifi ▂▄▆█"
    static std::string withSignalBars(const std::string& ssid, int32_t rssi);

private:
    static std::string signalBars(int32_t rssi);
};
