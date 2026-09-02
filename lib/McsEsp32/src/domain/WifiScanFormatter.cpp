#include "WifiScanFormatter.h"

#include <algorithm>
#include <map>

std::vector<ScannedNetwork> WifiScanFormatter::dedupeAndSort(const std::vector<ScannedNetwork>& raw)
{
    std::map<std::string, int32_t> strongestBySsid;
    for (const auto& network : raw)
    {
        if (network.ssid.empty())
        {
            continue;
        }
        const auto it = strongestBySsid.find(network.ssid);
        if (it == strongestBySsid.end() || network.rssi > it->second)
        {
            strongestBySsid[network.ssid] = network.rssi;
        }
    }

    std::vector<ScannedNetwork> result;
    result.reserve(strongestBySsid.size());
    for (const auto& entry : strongestBySsid)
    {
        result.push_back({entry.first, entry.second});
    }

    std::sort(result.begin(), result.end(),
              [](const ScannedNetwork& a, const ScannedNetwork& b) { return a.rssi > b.rssi; });

    return result;
}

std::string WifiScanFormatter::signalBars(const int32_t rssi)
{
    if (rssi >= -50)
    {
        return "\xE2\x96\x82\xE2\x96\x84\xE2\x96\x86\xE2\x96\x88";
    }
    if (rssi >= -60)
    {
        return "\xE2\x96\x82\xE2\x96\x84\xE2\x96\x86";
    }
    if (rssi >= -70)
    {
        return "\xE2\x96\x82\xE2\x96\x84";
    }
    return "\xE2\x96\x82";
}

std::string WifiScanFormatter::withSignalBars(const std::string& ssid, const int32_t rssi)
{
    return ssid + " " + signalBars(rssi);
}
