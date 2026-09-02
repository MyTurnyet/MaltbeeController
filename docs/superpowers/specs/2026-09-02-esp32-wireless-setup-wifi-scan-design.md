# ESP32 Wireless Setup WiFi Network Scan — Design

## Context

The wireless-setup captive portal's form (`SetupFormRenderer`) currently
asks a technician to type the layout's WiFi SSID into a plain text field
from memory. The ESP32 Arduino core already exposes network scanning
(`WiFi.scanNetworks()`, part of the same `WiFi` library `CaptivePortalServer`
already depends on), so the form can instead offer a dropdown of nearby
networks while still allowing free-text entry for a network that doesn't
show up (weak signal, hidden SSID, scan timing).

Confirmed from the installed ESP32 Arduino core source
(`WiFiScan.cpp:66`): `scanNetworks()` calls `WiFi.enableSTA(true)`
internally, so it safely coexists with the AP that's already running via
`WiFi.softAP()` — no manual WiFi-mode management is needed on our side.

## Decisions (confirmed via Q&A)

1. **Scan is synchronous, not async-with-polling.** One blocking
   `WiFi.scanNetworks()` call (~2-3 seconds) is acceptable because it only
   ever runs while a human is deliberately standing at the panel during
   setup — simpler to build and reason about than an async-scan-plus-poll
   flow with a "scanning..." intermediate page state.
2. **Scan runs once, at `CaptivePortalServer::begin()`, not on every page
   load.** Phones and laptops fire several automatic background requests
   when joining a new AP (captive-portal detection probes), and this
   server's `onNotFound` handler already routes all of them to the same
   form page (`handleRoot()`). Scanning on every page load would trigger a
   multi-second scan per probe, making the portal sluggish or confusing
   OS captive-portal detection. Scanning once at `begin()` and caching the
   result means every subsequent page load (probes included) is instant.
   A `/rescan` route lets a technician request a fresh scan on demand
   without power-cycling the panel.
3. **Results are deduped by SSID (strongest signal wins) and sorted
   strongest-first**, with a text-based signal-strength indicator per
   entry (a native HTML `<select>`'s `<option>` only supports plain text —
   no icons/images/per-option styling — so the indicator is a short run of
   Unicode block characters appended to the label).
4. **The dropdown is a convenience on top of the existing free-text field,
   not a replacement for it.** Selecting a dropdown option copies that
   SSID into the same `wifi_ssid` text input that's already submitted
   today; typing a name that isn't in the list works exactly as it does
   now. No backend/commissioning-logic change — `WebFormCommissioningAdapter`
   and everything downstream of it are untouched.

## Components

### `WifiScanFormatter` (`lib/McsEsp32/src/domain/WifiScanFormatter.h`/`.cpp`, new)

Pure, native-testable — no Arduino dependency, matching this project's
established split between the AP-scanning hardware call (in
`CaptivePortalServer`) and the dedupe/sort/format logic (here).

```cpp
#pragma once

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
    // Dedupes by ssid (keeping the strongest rssi seen for each), sorts
    // strongest-first. Entries with an empty ssid (hidden networks) are
    // dropped -- there's nothing a technician could usefully select.
    static std::vector<ScannedNetwork> dedupeAndSort(const std::vector<ScannedNetwork>& raw);

    // Appends a short signal-strength indicator to a label, e.g.
    // "MyLayoutWifi" -> "MyLayoutWifi ▂▄▆█"
    static std::string withSignalBars(const std::string& ssid, int32_t rssi);

private:
    static std::string signalBars(int32_t rssi);
};
```

Signal-bar thresholds (four levels, matching common OS WiFi-picker
conventions):

| RSSI (dBm)   | Bars   |
|---|---|
| ≥ -50        | `▂▄▆█` |
| ≥ -60        | `▂▄▆`  |
| ≥ -70        | `▂▄`   |
| < -70        | `▂`    |

### `CaptivePortalServer` changes (`lib/McsEsp32/src/adapters/`)

`begin()` scans once, right after `WiFi.softAP()`:

```cpp
void CaptivePortalServer::begin(const std::string& apName)
{
    WiFi.softAP(apName.c_str());
    scanNetworks();
    // ...existing DNS/HTTP setup unchanged...
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

New `/rescan` route registered alongside the existing `/` and `/submit`
routes, calling `scanNetworks()` again and redirecting back to `/`:

```cpp
webServer_.on("/rescan", [this]() {
    scanNetworks();
    webServer_.sendHeader("Location", "/");
    webServer_.send(302);
});
```

`handleRoot()` passes the cached list to the renderer:

```cpp
void CaptivePortalServer::handleRoot()
{
    webServer_.send(200, "text/html",
                     SetupFormRenderer::render(adapter_.currentValues(), scannedNetworks_).c_str());
}
```

New private member: `std::vector<ScannedNetwork> scannedNetworks_;`

No native test for this class (matches its existing convention — `#ifdef
ARDUINO`-guarded hardware shim, verified via `pio run -e esp32dev` build
success).

### `SetupFormRenderer` changes (`lib/McsEsp32/src/domain/`)

`render()` gains a second parameter, `const std::vector<ScannedNetwork>&
networks`, and renders a dropdown above the existing SSID text field:

```cpp
static std::string render(const WebFormSubmission& values, const std::vector<ScannedNetwork>& networks)
{
    // ...
    html += "<label>WiFi SSID</label>";
    html += "<select onchange=\"document.getElementsByName('wifi_ssid')[0].value=this.value\">";
    html += "<option value='' disabled selected hidden>-- select a nearby network --</option>";
    for (const auto& network : networks)
    {
        const std::string label = WifiScanFormatter::withSignalBars(network.ssid, network.rssi);
        html += "<option value='" + escapeHtml(network.ssid) + "'>" + escapeHtml(label) + "</option>";
    }
    html += "</select>";
    html += "<input name='wifi_ssid' value='" + escapeHtml(values.wifiSsid) + "'>";
    html += "<p class='hint'>Pick a network above, or type one in directly if it's not listed.</p>";
    html += "<p class='hint'><a href='/rescan'>Rescan for networks</a></p>";
    // ...
}
```

An empty `networks` list (no scan results, e.g. no networks in range)
renders the dropdown with only the placeholder option — the free-text
field remains fully usable regardless.

## Testing

- **`WifiScanFormatter`**: dedupe keeps the strongest of two entries with
  the same SSID; sort orders strongest-first; hidden (empty-SSID) entries
  are dropped; each of the four signal-bar thresholds produces the right
  bar string, including the boundary values (-50, -60, -70 exactly); an
  empty input list produces an empty output list.
- **`SetupFormRenderer`** (additive cases in the existing suite): renders
  a `<select>` option for each network in the list with the right
  SSID/label; renders only the placeholder option when the list is empty;
  the SSID value is still HTML-escaped (reuses the existing `escapeHtml()`
  path — a network named e.g. `<script>` must not break the page); the
  existing free-text field and its pre-fill behavior are unchanged.
- **`CaptivePortalServer`**: no native test (matches existing convention)
  — verified via `pio run -e esp32dev` build success and a manual
  read-through, same as every other change to this class.

## File layout

- `lib/McsEsp32/src/domain/WifiScanFormatter.h`/`.cpp` (new)
- `test/test_wifi_scan_formatter/` (new)
- Modify: `lib/McsEsp32/src/domain/SetupFormRenderer.h` (new parameter,
  dropdown markup)
- Modify: `lib/McsEsp32/src/adapters/CaptivePortalServer.h`/`.cpp` (scan
  at `begin()`, `/rescan` route, pass results to the renderer)
- Additive cases in `test/test_setup_form_renderer/`

## Non-goals

- Async scanning / a "scanning..." intermediate page state (Decision 1).
- Persisting or caching scan results across a reboot — a fresh scan runs
  every time `begin()` is called, same as every other piece of
  wireless-setup state.
- Showing encryption type (open vs. secured) per network — not requested;
  can be added later the same way signal bars were if wanted.
- Any change to `WebFormCommissioningAdapter`, `CommissioningSession`, or
  how the submitted `wifi_ssid` value is validated/stored — the dropdown
  only ever writes into the existing text field before submission.
