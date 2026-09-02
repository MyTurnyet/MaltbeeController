# ESP32 Wireless Setup WiFi Network Scan Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a technician pick the layout's WiFi network from a dropdown of nearby networks on the wireless-setup form, while still being able to type a name that isn't listed — no change to what actually gets submitted or how it's processed downstream.

**Architecture:** A new pure, native-testable `WifiScanFormatter` (dedupe/sort/signal-bar formatting) sits behind `CaptivePortalServer`, which does the one Arduino-only thing (`WiFi.scanNetworks()`) and hands plain `ScannedNetwork` data to `SetupFormRenderer`, which renders the dropdown. The existing `wifi_ssid` text field, `WebFormCommissioningAdapter`, and everything downstream are untouched — the dropdown only ever writes into that same field via a tiny inline `onchange` script.

**Tech Stack:** C++17, PlatformIO (`native` for `WifiScanFormatter`/`SetupFormRenderer` tests, `esp32dev` build-check for `CaptivePortalServer`), Catch2.

## Global Constraints

- `WiFi.scanNetworks()` internally calls `WiFi.enableSTA(true)` (confirmed in the installed core's `WiFiScan.cpp:66`), so it's safe to call right after `WiFi.softAP()` with no manual WiFi-mode management.
- The scan runs exactly once, in `CaptivePortalServer::begin()` — never per page load — plus once more per `/rescan` request. This is deliberate: OS captive-portal-detection probes hit `onNotFound` → `handleRoot()` repeatedly, and scanning on every one of those would make the portal sluggish.
- `SetupFormRenderer::render()`'s existing single-argument call sites (all of `test/test_setup_form_renderer/`) must keep compiling unchanged — the new `networks` parameter gets a default value (`= {}`) rather than requiring every caller to be touched.
- `CaptivePortalServer` remains an `#ifdef ARDUINO`-guarded hardware shim with no dedicated native test — this plan doesn't change that convention.
- `WifiScanFormatter` drops entries with an empty SSID (hidden networks) — there's nothing a technician could usefully select for one.

---

### Task 1: `WifiScanFormatter`

**Files:**
- Create: `lib/McsEsp32/src/domain/WifiScanFormatter.h`
- Create: `lib/McsEsp32/src/domain/WifiScanFormatter.cpp`
- Test: `test/test_wifi_scan_formatter/test_main.cpp`

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces (consumed by Tasks 2 and 3):
  ```cpp
  struct ScannedNetwork
  {
      std::string ssid;
      int32_t rssi;
  };

  class WifiScanFormatter
  {
  public:
      static std::vector<ScannedNetwork> dedupeAndSort(const std::vector<ScannedNetwork>& raw);
      static std::string withSignalBars(const std::string& ssid, int32_t rssi);
  };
  ```

- [ ] **Step 1: Write the failing test file**

Create `test/test_wifi_scan_formatter/test_main.cpp`:

```cpp
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
```

(The `\xE2\x96\x82` etc. escapes are the UTF-8 byte sequences for `▂`/`▄`/`▆`/`█` — written as escapes here so the expected values are unambiguous regardless of this file's own encoding; the implementation in Step 3 uses the literal characters directly.)

- [ ] **Step 2: Run the test to verify it fails to compile**

Run: `pio test -e native -f test_wifi_scan_formatter`
Expected: FAIL — `WifiScanFormatter.h` does not exist yet.

- [ ] **Step 3: Write the header**

Create `lib/McsEsp32/src/domain/WifiScanFormatter.h`:

```cpp
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
```

- [ ] **Step 4: Write the implementation**

Create `lib/McsEsp32/src/domain/WifiScanFormatter.cpp`:

```cpp
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
```

(Using the same `\xE2\x96\x8X` escapes in the implementation as in the test avoids any risk of a source-file-encoding mismatch between the two — functionally identical to writing the literal `▂▄▆█` characters.)

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_wifi_scan_formatter`
Expected: PASS — 8 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites pass (41/41 — 40 existing plus this task's new suite).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/domain/WifiScanFormatter.h lib/McsEsp32/src/domain/WifiScanFormatter.cpp test/test_wifi_scan_formatter/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add WifiScanFormatter

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CGYUBEe91zFYStbvaxPD3c
EOF
)"
```

---

### Task 2: `SetupFormRenderer` network dropdown

**Files:**
- Modify: `lib/McsEsp32/src/domain/SetupFormRenderer.h`
- Test: `test/test_setup_form_renderer/test_main.cpp` (additive cases)

**Interfaces:**
- Consumes: `WifiScanFormatter`/`ScannedNetwork` (Task 1) — `static std::string withSignalBars(const std::string&, int32_t)`.
- Produces: `SetupFormRenderer::render(const WebFormSubmission& values, const std::vector<ScannedNetwork>& networks = {})` — the second parameter is new; every existing call site (all of `test/test_setup_form_renderer/`, unmodified by this task) keeps compiling via the default argument. `CaptivePortalServer` (Task 3) is the only caller that will pass a non-default `networks` argument.

- [ ] **Step 1: Write the failing additive tests**

Add to the end of `test/test_setup_form_renderer/test_main.cpp` (the file already ends after the "render includes a labeled input for each of the 12 turnout channels" test case — add these after it, and add `#include "domain/WifiScanFormatter.h"` to the top alongside the existing include):

```cpp
TEST_CASE("render's network dropdown includes an option for each scanned network")
{
    const std::vector<ScannedNetwork> networks = {{"MyLayoutWifi", -45}, {"NeighborNet", -68}};

    const std::string html = SetupFormRenderer::render(emptyValues(), networks);

    REQUIRE(html.find("<option value='MyLayoutWifi'>") != std::string::npos);
    REQUIRE(html.find(WifiScanFormatter::withSignalBars("MyLayoutWifi", -45)) != std::string::npos);
    REQUIRE(html.find("<option value='NeighborNet'>") != std::string::npos);
    REQUIRE(html.find(WifiScanFormatter::withSignalBars("NeighborNet", -68)) != std::string::npos);
}

TEST_CASE("render's network dropdown shows only the placeholder when no networks were scanned")
{
    const std::string html = SetupFormRenderer::render(emptyValues());

    REQUIRE(html.find("-- select a nearby network --") != std::string::npos);
    REQUIRE(html.find("<option value='MyLayoutWifi'>") == std::string::npos);
}

TEST_CASE("render escapes a scanned network name containing HTML-significant characters")
{
    const std::vector<ScannedNetwork> networks = {{"My\"Network", -50}};

    const std::string html = SetupFormRenderer::render(emptyValues(), networks);

    REQUIRE(html.find("My&quot;Network") != std::string::npos);
    REQUIRE(html.find("My\"Network") == std::string::npos);
}

TEST_CASE("render's existing wifi ssid text field is unaffected by the network dropdown")
{
    WebFormSubmission form = emptyValues();
    form.wifiSsid = "AlreadyConfiguredWifi";
    const std::vector<ScannedNetwork> networks = {{"SomeOtherNetwork", -50}};

    const std::string html = SetupFormRenderer::render(form, networks);

    REQUIRE(html.find("name='wifi_ssid' value='AlreadyConfiguredWifi'") != std::string::npos);
}
```

- [ ] **Step 2: Run the test to verify it fails to compile**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: FAIL — `render()` doesn't accept a second argument yet, and `WifiScanFormatter.h` isn't included.

- [ ] **Step 3: Add the include**

In `lib/McsEsp32/src/domain/SetupFormRenderer.h`, find:

```cpp
#pragma once

#include <string>

#include "NodeConfig.h"
#include "WebFormSubmission.h"
```

Replace with:

```cpp
#pragma once

#include <string>
#include <vector>

#include "NodeConfig.h"
#include "WebFormSubmission.h"
#include "WifiScanFormatter.h"
```

- [ ] **Step 4: Change the `render()` signature and add the dropdown markup**

Find:

```cpp
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
        html += "<label>WiFi Password</label><input name='wifi_password' type='password' value=''>";
```

Replace with:

```cpp
    static std::string render(const WebFormSubmission& values, const std::vector<ScannedNetwork>& networks = {})
    {
        std::string html;
        html += "<!DOCTYPE html><html><head>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>" + kStyle + "</style></head><body>";
        html += "<div class='card'><h1>MaltBee Panel Setup</h1>";
        html += "<p class='subtitle'>Configure this panel's network settings</p>";
        html += "<form method='POST' action='/submit'>";
        html += "<label>Node ID</label><select name='id'>" + renderIdOptions(values.nodeId) + "</select>";
        html += "<label>WiFi SSID</label>";
        html += "<select onchange=\"document.getElementsByName('wifi_ssid')[0].value=this.value\">"
            + renderNetworkOptions(networks) + "</select>";
        html += "<input name='wifi_ssid' value='" + escapeHtml(values.wifiSsid) + "'>";
        html += "<p class='hint'>Pick a network above, or type one in directly if it's not listed. "
            "<a href='/rescan'>Rescan</a></p>";
        html += "<label>WiFi Password</label><input name='wifi_password' type='password' value=''>";
```

- [ ] **Step 5: Add the `renderNetworkOptions()` helper**

Find:

```cpp
private:
    static std::string renderIdOptions(const std::string& selectedId)
```

Replace with:

```cpp
private:
    static std::string renderNetworkOptions(const std::vector<ScannedNetwork>& networks)
    {
        std::string options = "<option value='' disabled selected hidden>-- select a nearby network --</option>";
        for (const auto& network : networks)
        {
            const std::string label = WifiScanFormatter::withSignalBars(network.ssid, network.rssi);
            options += "<option value='" + escapeHtml(network.ssid) + "'>" + escapeHtml(label) + "</option>";
        }
        return options;
    }

    static std::string renderIdOptions(const std::string& selectedId)
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: PASS — 14 test cases (10 existing + 4 new), 0 failures.

- [ ] **Step 7: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites pass (41/41, unchanged from Task 1 — this task adds no new suite, only additive cases to an existing one).

- [ ] **Step 8: Commit**

```bash
git add lib/McsEsp32/src/domain/SetupFormRenderer.h test/test_setup_form_renderer/test_main.cpp
git commit -m "$(cat <<'EOF'
! F Add network dropdown to SetupFormRenderer

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CGYUBEe91zFYStbvaxPD3c
EOF
)"
```

---

### Task 3: `CaptivePortalServer` scan wiring

**Files:**
- Modify: `lib/McsEsp32/src/adapters/CaptivePortalServer.h`
- Modify: `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp`

**Interfaces:**
- Consumes: `WifiScanFormatter::dedupeAndSort()` (Task 1); `SetupFormRenderer::render(values, networks)` (Task 2).
- Produces: nothing — this is the last piece; nothing else depends on it.

No test — matches this class's existing convention (verified via `pio run -e esp32dev` build success).

- [ ] **Step 1: Add the include and new members to the header**

In `lib/McsEsp32/src/adapters/CaptivePortalServer.h`, find:

```cpp
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

    void begin(const std::string& apName);
    void poll();

private:
    void handleRoot();
    void handleSubmit();
    WebFormSubmission readForm();

    WebFormCommissioningAdapter& adapter_;
    DNSServer dnsServer_;
    WebServer webServer_{80};
};
```

Replace with:

```cpp
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include <string>
#include <vector>

#include "WebFormCommissioningAdapter.h"
#include "../domain/SetupFormRenderer.h"
#include "../domain/WifiScanFormatter.h"

class CaptivePortalServer
{
public:
    explicit CaptivePortalServer(WebFormCommissioningAdapter& adapter);

    void begin(const std::string& apName);
    void poll();

private:
    void handleRoot();
    void handleSubmit();
    void handleRescan();
    void scanNetworks();
    WebFormSubmission readForm();

    WebFormCommissioningAdapter& adapter_;
    DNSServer dnsServer_;
    WebServer webServer_{80};
    std::vector<ScannedNetwork> scannedNetworks_;
};
```

- [ ] **Step 2: Scan once in `begin()`, register the `/rescan` route**

In `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp`, find:

```cpp
void CaptivePortalServer::begin(const std::string& apName)
{
    WiFi.softAP(apName.c_str());

    IPAddress apIp = WiFi.softAPIP();
    dnsServer_.start(53, "*", apIp);

    webServer_.on("/", [this]() { handleRoot(); });
    webServer_.on("/submit", HTTP_POST, [this]() { handleSubmit(); });
    webServer_.onNotFound([this]() { handleRoot(); });
    webServer_.begin();
}
```

Replace with:

```cpp
void CaptivePortalServer::begin(const std::string& apName)
{
    WiFi.softAP(apName.c_str());
    scanNetworks();

    IPAddress apIp = WiFi.softAPIP();
    dnsServer_.start(53, "*", apIp);

    webServer_.on("/", [this]() { handleRoot(); });
    webServer_.on("/submit", HTTP_POST, [this]() { handleSubmit(); });
    webServer_.on("/rescan", [this]() { handleRescan(); });
    webServer_.onNotFound([this]() { handleRoot(); });
    webServer_.begin();
}
```

- [ ] **Step 3: Pass the scanned networks to the renderer**

Find:

```cpp
void CaptivePortalServer::handleRoot()
{
    webServer_.send(200, "text/html", SetupFormRenderer::render(adapter_.currentValues()).c_str());
}
```

Replace with:

```cpp
void CaptivePortalServer::handleRoot()
{
    webServer_.send(200, "text/html",
                     SetupFormRenderer::render(adapter_.currentValues(), scannedNetworks_).c_str());
}
```

- [ ] **Step 4: Add `handleRescan()` and `scanNetworks()`**

Find:

```cpp
void CaptivePortalServer::handleSubmit()
{
    const WebFormSubmission form = readForm();
    const std::string response = adapter_.submit(form);
    webServer_.send(200, "text/plain", response.c_str());
}
```

Replace with:

```cpp
void CaptivePortalServer::handleSubmit()
{
    const WebFormSubmission form = readForm();
    const std::string response = adapter_.submit(form);
    webServer_.send(200, "text/plain", response.c_str());
}

void CaptivePortalServer::handleRescan()
{
    scanNetworks();
    webServer_.sendHeader("Location", "/");
    webServer_.send(302);
}

void CaptivePortalServer::scanNetworks()
{
    std::vector<ScannedNetwork> raw;
    const int16_t count = WiFi.scanNetworks();
    for (int16_t i = 0; i < count; ++i)
    {
        raw.push_back({std::string(WiFi.SSID(i).c_str()), WiFi.RSSI(i)});
    }
    scannedNetworks_ = WifiScanFormatter::dedupeAndSort(raw);
}
```

- [ ] **Step 5: Build-check the ESP32 target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites pass (41/41, unchanged — `CaptivePortalServer` isn't part of the native build).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/adapters/CaptivePortalServer.h lib/McsEsp32/src/adapters/CaptivePortalServer.cpp
git commit -m "$(cat <<'EOF'
! F Scan for nearby WiFi networks in CaptivePortalServer

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CGYUBEe91zFYStbvaxPD3c
EOF
)"
```

---

### Task 4: Update documentation

**Files:**
- Modify: `CLAUDE.md`
- Modify: `docs/ESP32_Turnout_Panel_Implementation.md`

No test — documentation only.

- [ ] **Step 1: `CLAUDE.md` — add `WifiScanFormatter` to the domain class list**

Find (inside the McsEsp32 `domain/` bullet — this is one very long line; the substring below is unique, don't touch anything else in it):

```
NodeIdentityGuard (pure one-way latch: `onMacObserved()` compares an incoming MAC against the panel's own; a mismatch means a second panel is claiming this `nodeId`, and the latch never clears once tripped)
```

Replace with:

```
NodeIdentityGuard (pure one-way latch: `onMacObserved()` compares an incoming MAC against the panel's own; a mismatch means a second panel is claiming this `nodeId`, and the latch never clears once tripped), WifiScanFormatter (pure dedupe/sort/format layer for the wireless-setup form's network dropdown — `dedupeAndSort()` keeps the strongest signal per SSID and drops hidden (empty-SSID) entries, `withSignalBars()` appends a 1-4-bar Unicode indicator per SSID since a native `<select>` `<option>` can't carry an icon; the actual `WiFi.scanNetworks()` call lives in `CaptivePortalServer`, which is the only Arduino-dependent piece of this feature)
```

- [ ] **Step 2: `CLAUDE.md` — update the `SetupFormRenderer`/`CaptivePortalServer` descriptions**

Find (inside the McsEsp32 `adapters/` bullet):

```
CaptivePortalServer (`#ifdef ARDUINO`-guarded hardware shims; opens a WiFi AP + `DNSServer` (all queries redirected to the AP's own IP) + `WebServer` serving `SetupFormRenderer`'s page; `begin(apName)` opens an **open** (no-passphrase) AP, wired in `src/esp32/main.cpp` — this was WPA2-protected under sub-project #2c-b2 but the passphrase was later removed (see sub-project #2c-d, `docs/superpowers/specs/2026-09-02-esp32-open-wireless-setup-ap-design.md`);
```

Replace with:

```
CaptivePortalServer (`#ifdef ARDUINO`-guarded hardware shims; opens a WiFi AP + `DNSServer` (all queries redirected to the AP's own IP) + `WebServer` serving `SetupFormRenderer`'s page; `begin(apName)` opens an **open** (no-passphrase) AP, wired in `src/esp32/main.cpp` — this was WPA2-protected under sub-project #2c-b2 but the passphrase was later removed (see sub-project #2c-d, `docs/superpowers/specs/2026-09-02-esp32-open-wireless-setup-ap-design.md`); `begin()` also runs one `WiFi.scanNetworks()` right after `WiFi.softAP()` (safe to combine — the core's `scanNetworks()` internally enables station mode alongside the existing AP) and caches the deduped/sorted result via `WifiScanFormatter`, so every subsequent page load (including the OS's automatic captive-portal-detection probes, which `onNotFound` also routes to the form) is instant rather than re-scanning; a `/rescan` route re-runs the scan on demand;
```

- [ ] **Step 3: `docs/ESP32_Turnout_Panel_Implementation.md` — note the dropdown in the Wireless Setup Access Point section**

Find (the last paragraph of the "Wireless Setup Access Point" section):

```
The AP is **open** (no password required to join) — physical access to
the panel (the BOOT-button hold that opens it) is the only gate.
Wireless setup mode has no timeout, so an abandoned mid-commissioning
panel stays open to anyone in range until someone completes the form or
power-cycles the board; don't leave a panel in setup mode unattended any
longer than necessary. Because the AP itself has no encryption, avoid
changing a panel's WiFi password over the setup form while in an
untrusted RF environment — the new password is sent in cleartext over
the open AP during that submission (stored credentials are never read
back out, only ever written).
```

Replace with:

```
The AP is **open** (no password required to join) — physical access to
the panel (the BOOT-button hold that opens it) is the only gate.
Wireless setup mode has no timeout, so an abandoned mid-commissioning
panel stays open to anyone in range until someone completes the form or
power-cycles the board; don't leave a panel in setup mode unattended any
longer than necessary. Because the AP itself has no encryption, avoid
changing a panel's WiFi password over the setup form while in an
untrusted RF environment — the new password is sent in cleartext over
the open AP during that submission (stored credentials are never read
back out, only ever written).

The WiFi SSID field also offers a dropdown of nearby networks (scanned
once when the AP opens, refreshable via a "Rescan" link on the page) —
selecting one fills the text field, which can still be typed into
directly for a network that isn't listed.
```

- [ ] **Step 4: Commit**

```bash
git add CLAUDE.md docs/ESP32_Turnout_Panel_Implementation.md
git commit -m "$(cat <<'EOF'
. d Document the wireless-setup WiFi network dropdown

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CGYUBEe91zFYStbvaxPD3c
EOF
)"
```

---

## Self-review notes

- **Spec coverage:** all 4 Decisions from `docs/superpowers/specs/2026-09-02-esp32-wireless-setup-wifi-scan-design.md` are covered: Decision 1 (synchronous scan) → Task 3 Step 4's plain blocking `WiFi.scanNetworks()` call; Decision 2 (scan once at `begin()`, `/rescan` route) → Task 3 Steps 2 and 4; Decision 3 (dedupe/sort/signal-bars) → Task 1; Decision 4 (dropdown writes into the existing text field, no backend change) → Task 2 Step 4's `onchange` script, and no task touches `WebFormCommissioningAdapter` or anything downstream of `wifi_ssid`. The spec's full test list for `WifiScanFormatter` is present verbatim as 8 cases in Task 1. The Non-goals (no async scanning, no cross-reboot caching, no encryption-type display, no commissioning-logic changes) are respected — no task does any of those.
- **Placeholder scan:** no TBD/TODO; every step has complete, concrete code or exact find/replace text (Task 4 Step 3's anchor was verified directly against the file's current content before this plan was written).
- **Type consistency:** `ScannedNetwork{std::string ssid; int32_t rssi;}` and `WifiScanFormatter::dedupeAndSort()`/`withSignalBars()`'s signatures, defined in Task 1, are used identically in Task 2's tests and `SetupFormRenderer::render()`, and in Task 3's `CaptivePortalServer::scanNetworks()`. `SetupFormRenderer::render()`'s new default-valued second parameter is used consistently — Task 2's own new tests pass it explicitly, Task 3's `CaptivePortalServer` passes `scannedNetworks_`, and every pre-existing single-argument call site is left untouched by both tasks.
