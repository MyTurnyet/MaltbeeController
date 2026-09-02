#include <catch2/catch_test_macros.hpp>

#include "domain/WifiScanFormatter.h"

TEST_CASE("dedupeAndSort returns an empty list for an empty input")
{
    REQUIRE(WifiScanFormatter::dedupeAndSort({}).empty());
}

TEST_CASE("dedupeAndSort drops entries with an empty ssid")
{
    const std::vector<ScannedNetwork> raw = {{"", -40}};

    REQUIRE(WifiScanFormatter::dedupeAndSort(raw).empty());
}

TEST_CASE("dedupeAndSort keeps the strongest signal when the same ssid appears twice")
{
    const std::vector<ScannedNetwork> raw = {{"MyWifi", -70}, {"MyWifi", -40}};

    const auto result = WifiScanFormatter::dedupeAndSort(raw);

    REQUIRE(result.size() == 1);
    REQUIRE(result[0].ssid == "MyWifi");
    REQUIRE(result[0].rssi == -40);
}

TEST_CASE("dedupeAndSort orders distinct networks strongest-first")
{
    const std::vector<ScannedNetwork> raw = {{"Weakest", -80}, {"Strongest", -30}, {"Middle", -55}};

    const auto result = WifiScanFormatter::dedupeAndSort(raw);

    REQUIRE(result.size() == 3);
    REQUIRE(result[0].ssid == "Strongest");
    REQUIRE(result[1].ssid == "Middle");
    REQUIRE(result[2].ssid == "Weakest");
}

TEST_CASE("withSignalBars appends 4 bars at -50 dBm or stronger")
{
    REQUIRE(WifiScanFormatter::withSignalBars("MyWifi", -50) == "MyWifi \xE2\x96\x82\xE2\x96\x84\xE2\x96\x86\xE2\x96\x88");
    REQUIRE(WifiScanFormatter::withSignalBars("MyWifi", -30) == "MyWifi \xE2\x96\x82\xE2\x96\x84\xE2\x96\x86\xE2\x96\x88");
}

TEST_CASE("withSignalBars appends 3 bars between -60 and -51 dBm")
{
    REQUIRE(WifiScanFormatter::withSignalBars("MyWifi", -60) == "MyWifi \xE2\x96\x82\xE2\x96\x84\xE2\x96\x86");
    REQUIRE(WifiScanFormatter::withSignalBars("MyWifi", -51) == "MyWifi \xE2\x96\x82\xE2\x96\x84\xE2\x96\x86");
}

TEST_CASE("withSignalBars appends 2 bars between -70 and -61 dBm")
{
    REQUIRE(WifiScanFormatter::withSignalBars("MyWifi", -70) == "MyWifi \xE2\x96\x82\xE2\x96\x84");
    REQUIRE(WifiScanFormatter::withSignalBars("MyWifi", -61) == "MyWifi \xE2\x96\x82\xE2\x96\x84");
}

TEST_CASE("withSignalBars appends 1 bar weaker than -70 dBm")
{
    REQUIRE(WifiScanFormatter::withSignalBars("MyWifi", -71) == "MyWifi \xE2\x96\x82");
    REQUIRE(WifiScanFormatter::withSignalBars("MyWifi", -90) == "MyWifi \xE2\x96\x82");
}
