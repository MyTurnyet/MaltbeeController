# ESP32 Wireless Commissioning Web Form (Sub-project #2c-b1) — Design

This is sub-project **#2c-b1**, split out of sub-project #2c-b (Wireless
captive-portal commissioning: web form + composition-root wiring) — itself
the second half of sub-project #2c, after #2c-a (complete and merged:
`BootMode`/`BootModeSelector`, `SetupModeRequestStore`/
`NvsSetupModeRequestStore`, `GatedDigitalInput`, `ComboSetupModeTrigger`).
This spec covers the web form and captive-portal server classes, all
buildable and (mostly) native-testable in isolation. Wiring any of it into
`src/esp32/main.cpp` — `BootMode` branching, the combo trigger, resolving
the suppression-timing gap #2c-a's final review deferred — is sub-project
**#2c-b2**.

## Context

The sibling project `../MaltbeeTurnoutController` has a working captive-portal
implementation this project's established "port the design, adapt the code"
reuse strategy draws from: `CaptivePortalServer.h` (`WiFi.softAP()` +
`DNSServer` + `WebServer`), `WebFormCommissioningAdapter.h` (translates a web
form into the same command-line text `CommandLineParser` already parses),
`SetupFormRenderer.h` (HTML rendering/escaping), `SetupApName.h`/`MacAddress.h`
(AP naming from the chip's MAC address, needed because a factory-fresh panel's
`nodeId` is still `0`/unset — MAC is the only thing that's already unique
across multiple freshly-unboxed panels being commissioned side by side).

This project's own form is simpler than the sibling's: GPIO wiring is fixed
by this panel's PCB design (per sub-project 2a's scope decision), so there
are no per-turnout pin/orientation/settle/timeout fields — just WiFi
credentials, broker address, node id, and 12 channel JMRI names, matching
`CommissioningSession`'s existing command set exactly (`id`, `wifi`,
`broker`, `turnout N name X`, `save`, `reboot`).

Two things don't port verbatim from the sibling:
- **Response-text conventions differ.** The sibling's `CommissioningSession`
  returns a `CommissioningResult{response, rebootRequested}` with a shared
  `"ERROR..."` failure prefix. This project's `CommissioningSession::apply()`
  (`lib/McsEsp32/src/application/CommissioningSession.cpp`) returns a plain
  `std::string` with no shared prefix across failure modes: `"invalid
  config:\n  - ...\n"` (validation), `"save failed: could not write to
  storage\n"` (storage), `"error: ...\n"` (invalid command / out-of-range
  channel). Success responses are `"OK\n"` (Id/Wifi/Broker/TurnoutName),
  `"saved\n"` (Save), `"rebooting\n"` (Reboot — never fails).
- **MAC-address timing.** This project has already hit two ESP32 "a global
  constructor ran before something it depended on was ready" bugs (NVS init
  in sub-project #7b; the `clock`/`clock_t` symbol collision in sub-project
  #5). Reading the chip's real MAC address may have its own static-init
  timing considerations. This spec sidesteps deciding that now by keeping
  `CaptivePortalServer` ignorant of *how* the AP name was produced (see
  `CaptivePortalServer` below) — #2c-b2 decides when `EspDeviceIdentity::mac()`
  actually gets called.

## Decisions (confirmed via Q&A)

1. **Split #2c-b into #2c-b1 (this spec) and #2c-b2**, mirroring #7/#7a/#7b
   and #2c/#2c-a/#2c-b.
2. **Broker host is a single plain-text field**, not the sibling's 4-octet
   IPv4-only UI. Matches `NodeConfig::brokerHost`'s actual type (any
   string) and what bench-serial already accepts (including hostnames like
   `mosquitto.local`), and is simpler to build/test.

## Components

### `MacAddress` (`lib/McsEsp32/src/domain/MacAddress.h`)

Ported from the sibling essentially verbatim: wraps 6 bytes,
`lastFourHexDigits()` formats the last two bytes as 4 uppercase hex digits.
Pure value type, native-testable, no Arduino dependency.

### `SetupApName` (`lib/McsEsp32/src/domain/SetupApName.h`)

```cpp
class SetupApName
{
public:
    static std::string from(const MacAddress& mac)
    {
        return "MaltBee-Setup-" + mac.lastFourHexDigits();
    }
};
```

Renamed prefix from the sibling's `"Tortoise-Setup-"`. Native-testable.

### `EspDeviceIdentity` (`lib/McsEsp32/src/adapters/EspDeviceIdentity.h`/`.cpp`, `#ifdef ARDUINO`-guarded)

```cpp
class EspDeviceIdentity
{
public:
    [[nodiscard]] MacAddress mac() const;
};
```

Thin hardware shim wrapping the ESP32's real MAC address via
`esp_efuse_mac_get_default(uint8_t mac[6])` (`esp_mac.h`) — chosen over
Arduino's `WiFi.macAddress()` because it fills a `uint8_t[6]` directly
(matching `MacAddress`'s own `std::array<uint8_t, 6>` constructor with no
string round-trip) and, per Espressif's documentation, reads directly from
eFuse with no dependency on the WiFi driver having been started. Build-check
only via `pio run -e esp32dev`, no native test — same convention as
`NvsConfigStore`/`WiFiLink`/`MqttLink`. **#2c-b2 must explicitly decide, and
document why, whether `.mac()` is called at global scope or deferred into
`setup()`** — not decided here, flagged given this project's two prior
static-init hazards; `esp_efuse_mac_get_default()` not depending on the WiFi
driver makes global-scope use *more* plausible here than the NVS/`clock()`
cases were, but #2c-b2 must still verify rather than assume.

### `WebFormSubmission` (`lib/McsEsp32/src/domain/WebFormSubmission.h`)

```cpp
#pragma once

#include <array>
#include <string>

#include "NodeConfig.h"

struct WebFormSubmission
{
    std::string nodeId;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string brokerHost;
    std::string brokerPort;
    std::array<std::string, NodeConfig::kChannelCount> channelJmriNames;
};
```

All fields are strings (matching how HTML form fields naturally arrive,
before any parsing) except the channel array, which is naturally sized to
`NodeConfig::kChannelCount` (12).

### `WebFormCommissioningAdapter` (`lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.h`/`.cpp`)

```cpp
class WebFormCommissioningAdapter
{
public:
    explicit WebFormCommissioningAdapter(CommissioningSession& session);

    std::string submit(const WebFormSubmission& form);
    [[nodiscard]] bool rebootRequested() const;
    [[nodiscard]] WebFormSubmission currentValues() const;

private:
    CommissioningSession& session_;
};
```

`submit()` builds the same command-line text `CommandLineParser` already
parses and feeds each through `session_.apply()`, stopping at the first
response that doesn't exactly match the known success string for that
command:

```cpp
std::string WebFormCommissioningAdapter::submit(const WebFormSubmission& form)
{
    std::string response = session_.apply(CommandLineParser::parse("id " + form.nodeId));
    if (response != "OK\n") { return response; }

    response = session_.apply(
        CommandLineParser::parse("wifi \"" + form.wifiSsid + "\" \"" + form.wifiPassword + "\""));
    if (response != "OK\n") { return response; }

    response = session_.apply(CommandLineParser::parse("broker " + form.brokerHost + " " + form.brokerPort));
    if (response != "OK\n") { return response; }

    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        if (form.channelJmriNames[i].empty()) { continue; }
        response = session_.apply(
            CommandLineParser::parse("turnout " + std::to_string(i + 1) + " name " + form.channelJmriNames[i]));
        if (response != "OK\n") { return response; }
    }

    response = session_.apply(CommandLineParser::parse("save"));
    if (response != "saved\n") { return response; }

    return session_.apply(CommandLineParser::parse("reboot"));
}
```

This checks **exact equality** against each command's known success string,
not a shared error prefix (unlike the sibling — see Context above, this
project's `CommissioningSession` has no shared failure-response
convention to key off of). `rebootRequested()` forwards to
`session_.rebootRequested()`. `currentValues()` reads back from the
session's draft (see the `CommissioningSession::draft()` addition below) to
pre-fill the form, including after a validation error re-renders it.

**Wifi credentials containing a space:** `CommandLineParser` already handles
quoted arguments (the sibling's own `buildCommandLines` wraps SSID/password
in `\"..\"` for exactly this reason, and this project's bench-serial 2a
design accepted the limitation that *unquoted* whitespace-containing values
aren't representable — quoting resolves that here). Verify
`CommandLineParser::parse()`'s actual quote-handling behavior against real
values (including a value containing an embedded `"`) as part of
implementation; if it turns out not to support quoting, this is a plan-time
finding to resolve, not something to silently work around.

### One small, deliberate change to existing code: `CommissioningSession::draft()`

`lib/McsEsp32/src/application/CommissioningSession.h` gains:

```cpp
[[nodiscard]] const NodeConfig& draft() const { return draft_; }
```

A pure, read-only, additive accessor — the only touch to already-tested
code in this sub-project. One small test case added to the existing
`test/test_commissioning_session/test_main.cpp` suite.

### `SetupFormRenderer` (`lib/McsEsp32/src/domain/SetupFormRenderer.h`)

Pure string-building, native-testable, no Arduino dependency:

- `escapeHtml(text)` — escapes `& < > " '` before embedding any saved value
  into the page. A saved SSID/password/broker-host could contain
  HTML-significant characters; this prevents both broken markup and
  stored-XSS-style injection back into the same form.
- `render(const WebFormSubmission& values)` — builds the full page:
  - Node id as a `<select>` populated from `NodeConfig::kMinNodeId` to
    `NodeConfig::kMaxNodeId` (1–99) — avoids typos, matches the domain's own
    valid range.
  - WiFi SSID (text) and password (`type='password'`) inputs.
  - A single broker-host text input and a broker-port number input (per
    Decision 2 — no octet splitting).
  - A collapsed `<details>` section ("Turnout JMRI Names") with 12 plain
    text inputs, one per channel — collapsed by default since partial
    commissioning is expected (a panel may be commissioned incrementally,
    per the 2a spec).
  - A submit button, `<form method='POST' action='/submit'>`.
  - Inline CSS: a minimal styled-card look (same structural approach as the
    sibling's page, rebranded — not pixel-identical, just the same "clean
    mobile-friendly card" shape).

### `CaptivePortalServer` (`lib/McsEsp32/src/adapters/CaptivePortalServer.h`/`.cpp`, `#ifdef ARDUINO`-guarded)

```cpp
#pragma once

#ifdef ARDUINO

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
    void poll();  // non-blocking, call every loop() tick

private:
    void handleRoot();
    void handleSubmit();
    WebFormSubmission readForm();

    WebFormCommissioningAdapter& adapter_;
    DNSServer dnsServer_;
    WebServer webServer_{80};
};

#endif
```

`begin(apName)`: `WiFi.softAP(apName.c_str())`, then `dnsServer_.start(53,
"*", WiFi.softAPIP())` (redirects all DNS queries to the AP's own IP — the
actual captive-portal auto-popup mechanism), then registers `/` and
`/submit` (`HTTP_POST`) handlers plus `onNotFound` routed to the same root
handler (so the DNS redirect always lands somewhere real, regardless of
what path a client's OS probes), then `webServer_.begin()`. `poll()`:
`dnsServer_.processNextRequest(); webServer_.handleClient();` — both
non-blocking.

Deliberately takes the AP name as a plain `std::string` via `begin()`
rather than computing it internally from a `MacAddress` (unlike the
sibling's constructor-time `apMac`) — keeps "how do we name this AP"
entirely out of the one class that touches real WiFi/DNS/HTTP hardware,
so #2c-b2 has full freedom over when/where `EspDeviceIdentity::mac()`
actually gets called.

`readForm()` reads `webServer_.arg("id")`, `"wifi_ssid"`, `"wifi_password"`,
`"broker_host"`, `"broker_port"`, and `"t1_name"`..`"t12_name"` into a
`WebFormSubmission`, matching `SetupFormRenderer`'s field names exactly.

## Testing

- **`MacAddress`**: construction, `lastFourHexDigits()` formatting for
  representative byte values (including one requiring zero-padding).
- **`SetupApName`**: `from()` produces `"MaltBee-Setup-"` + the expected 4
  hex digits.
- **`SetupFormRenderer`**: `escapeHtml()` escapes each of the five
  significant characters; `render()`'s output contains each supplied
  field's (escaped) value; the id `<select>` spans 1–99; each of the 12
  channel names renders in its own labeled input under the same channel
  number used elsewhere in this project (1-indexed, matching
  `CommissioningSession::formatShow()`'s existing convention).
- **`WebFormCommissioningAdapter::submit()`**, via a real
  `CommissioningSession` + `FakeConfigStore` (not a mock, matching this
  project's established convention of testing through real collaborators):
  - A fully valid submission ends with `rebootRequested()` true and the
    saved config readable back from `FakeConfigStore`.
  - An out-of-range channel number (e.g. via a malformed `WebFormSubmission`
    constructed directly in the test, since the real form's `<select>`
    can't produce one) stops immediately, returns that exact error text,
    and never reaches `save`.
  - A `FakeConfigStore` with `failNextSave = true` returns `"save failed:
    could not write to storage\n"` and does not request reboot.
  - `currentValues()` reflects the draft after some fields were set via a
    prior `submit()` (or directly via `CommissioningSession::apply()`,
    whichever makes the test clearest).
- **`CommissioningSession::draft()`**: one additive case in the existing
  suite — returns the same `NodeConfig` a fresh session was constructed
  with, and reflects a mutation after `apply()`.
- **`CaptivePortalServer`, `EspDeviceIdentity`**: build-check only via `pio
  run -e esp32dev`, no native test — same convention as
  `NvsConfigStore`/`WiFiLink`/`MqttLink`.

## File layout

- `lib/McsEsp32/src/domain/`: `MacAddress.h`, `SetupApName.h`,
  `WebFormSubmission.h`, `SetupFormRenderer.h`
- `lib/McsEsp32/src/adapters/`: `WebFormCommissioningAdapter.h`/`.cpp`,
  `CaptivePortalServer.h`/`.cpp` (guarded), `EspDeviceIdentity.h`/`.cpp`
  (guarded)
- Modify: `lib/McsEsp32/src/application/CommissioningSession.h` (add
  `draft()`)
- `test/test_mac_address/`, `test/test_setup_ap_name/`,
  `test/test_setup_form_renderer/`,
  `test/test_web_form_commissioning_adapter/`, plus one added case in
  `test/test_commissioning_session/`

## Non-goals

- Any `src/esp32/main.cpp` wiring.
- Deciding when/where `EspDeviceIdentity::mac()` is actually called (global
  scope vs. inside `setup()`).
- `BootMode` branching, constructing the AP only in `WirelessSetup` mode,
  or NOT constructing the WiFi/MQTT/JMRI/`ToggleTurnoutStation` graph in
  that mode.
- The combo trigger wiring, or resolving the suppression-timing gap #2c-a's
  final review deferred (`docs/superpowers/specs/2026-08-29-esp32-wireless-setup-trigger-design.md`'s
  "Known gap for #2c-b to resolve" section).
- All of the above are sub-project #2c-b2.

## Known gaps for #2c-b2 to resolve (found by the final whole-branch review)

**Resolved in #2c-b2** (see `docs/superpowers/specs/2026-08-30-esp32-wireless-setup-composition-wiring-design.md`): gap 1 was fixed by dropping `submit()`'s blank-channel skip guard, so a blank field now genuinely clears the channel. Gap 2 was closed at two independent layers — `CaptivePortalServer::begin()` now requires a WPA2 passphrase, and `WebFormCommissioningAdapter`/`SetupFormRenderer` both independently stop the real stored password from ever reaching the rendered page (neither layer depends on the other being correct). **Update (sub-project #2c-d):** the WPA2 passphrase layer was later removed by design — the AP is now open, relying solely on physical BOOT-button access as the gate; the second layer (stored password never rendered back into the form) is unaffected and still holds. Left as-is below for historical record of what the gap was and why it mattered.

This branch produces zero runtime effect (nothing here is referenced by
`src/esp32/main.cpp` yet), so neither of the following manifests until
#2c-b2 actually turns the AP on. Both are genuine design decisions, not
implementation defects — every class in this branch does exactly what it
was specified to do.

1. **The rendered form's own copy contradicts what `submit()` does with a
   blank channel field.** `SetupFormRenderer`'s "Turnout JMRI Names"
   section tells the operator *"Leave a channel blank to leave it
   unconfigured,"* but `WebFormCommissioningAdapter::submit()` `continue`s
   past a blank channel — meaning a blank field leaves that channel's
   *existing* stored name untouched, not cleared. There is currently no
   way to unconfigure a previously-set channel through the web form at
   all: re-load the form, see the name, blank it, submit — the old name
   comes right back. Both behaviors are individually reasonable (skip
   silently, matching the bench-serial `save` command's own "partial
   commissioning" allowance vs. genuinely clearing a field the operator
   visibly emptied) — but a form that shows the opposite of what it does
   is a real usability defect regardless of which behavior #2c-b2 picks.
   **#2c-b2 must choose one and either change the code (drop the `empty()`
   guard so blank truly clears — `withChannelName(n, "")` and `validate()`
   already handle an empty name safely) or change the copy** (e.g. *"Leave
   a channel blank to keep its current name"*) — not leave the mismatch
   standing.
2. **The AP itself is open, and it renders the layout's existing WiFi
   password back into the page in cleartext.** `CaptivePortalServer::begin()`
   calls the single-argument `WiFi.softAP(apName)`, which creates an
   unauthenticated access point — anyone within radio range during the
   commissioning window can join with no credential at all. Once joined,
   `GET /` returns a page whose password `<input>`'s `value='...'`
   attribute is `escapeHtml(values.wifiPassword)` — `type='password'`
   masks the *rendered* field from a casual glance, not the page source,
   so the layout's already-configured WiFi password (not just a
   newly-typed one) is readable by anyone who joins the open AP and views
   source. The same unauthenticated party can also POST a new
   configuration and trigger a reboot. This is a materially larger
   exposure than the 2a spec's already-accepted "a password being typed
   travels over plaintext HTTP" — it doesn't require the operator to be
   mid-commissioning at all, just physically nearby with a WiFi device,
   for as long as the AP stays up. Whether this is acceptable for a home
   layout's brief, operator-triggered setup window is a legitimate
   judgment call, but nothing has recorded it as a deliberate decision —
   right now it's an unexamined default. **#2c-b2 must explicitly decide**
   between (in roughly ascending cost): accept the exposure as-is,
   documented; give `begin()` a fixed or MAC-derived WPA2 passphrase
   (`WiFi.softAP(apName, password)`); or render the password field always
   empty with "leave blank to keep current" semantics (which, note, is the
   same skip-vs-clear question as gap 1 above, applied to the password
   field specifically — resolving them together is likely cheaper than
   resolving them separately).
