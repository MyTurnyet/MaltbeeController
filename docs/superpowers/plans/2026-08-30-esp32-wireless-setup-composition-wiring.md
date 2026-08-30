# ESP32 Wireless Setup Composition-Root Wiring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire sub-project #2c-a's boot-mode/trigger classes and #2c-b1's web-form/captive-portal classes into `src/esp32/main.cpp`, and make three small production-code fixes the design spec requires along the way.

**Architecture:** `src/esp32/main.cpp` keeps every existing global constructed unconditionally (no lazy/heap construction); only `setup()`/`loop()` gain a `bootMode == BootMode::WirelessSetup` branch that skips the WiFi-station/MQTT/JMRI/matrix/LED/station operations and instead runs the captive portal. A new `ComboSetupModeTrigger` (reading the raw matrix inputs) plus 12 `GatedDigitalInput` wrappers (feeding the `ToggleTurnoutStation`s) let a held T1+T2 combo request wireless setup on the next boot.

**Tech Stack:** PlatformIO / Arduino-ESP32, Catch2 (native tests), C++17.

## Global Constraints

- `CommissioningSession::apply()` response strings (unchanged, do not alter): `"OK\n"` (Id/Wifi/Broker/TurnoutName success), `"saved\n"` (Save success), `"rebooting\n"` (Reboot, never fails), `"invalid config:\n  - ...\n"` (Save validation failure), `"save failed: could not write to storage\n"` (Save storage failure), `"error: ...\n"` (invalid/out-of-range command).
- Combo-hold threshold: `constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;`.
- AP passphrase: `constexpr const char* WIRELESS_SETUP_AP_PASSPHRASE = "maltbee-setup";` — both constants live in `src/esp32/main.cpp`'s existing anonymous namespace, alongside `BLINK_INTERVAL_MS`/`DEFAULT_LED_COLOR`/`RETRY_INTERVAL_MS`/`UART_BAUD_RATE`.
- `ComboSetupModeTrigger` is constructed against the **raw** `matrixButtons[0]`/`matrixButtons[1]` (T1/T2) — never against `GatedDigitalInput`-wrapped ones. Wiring it to gated inputs deadlocks the gesture (suppression would force `bothActive` false the instant `holding_` becomes true).
- **All 12** `ToggleTurnoutStation`s switch from constructing against `matrixButtons[i]` to `gatedButtons[i]` (a new `GatedDigitalInput gatedButtons[12]` array wrapping each `matrixButtons[i]`) — uniformly, even though only `gatedButtons[0]`/`[1]` ever have `setSuppressed(true)` called on them. This matches the file's existing table-driven-array convention.
- The staggered two-finger combo-press gap (documented in `docs/superpowers/specs/2026-08-29-esp32-wireless-setup-trigger-design.md`) is accepted as-is — no code change addresses it. Do not add hold-off timers or change T1/T2 to command-on-release.
- `BootMode::Normal` and `BootMode::NeedsCommissioning` share the exact same code path in `main.cpp` — only `bootMode == BootMode::WirelessSetup` is ever branched on directly; the existing `configValid` boolean continues to distinguish Normal/NeedsCommissioning behavior exactly as it does today.
- A blank web-form channel-name field now **clears** that channel (inverts prior #2c-b1 behavior). A blank web-form WiFi-password field **keeps the current stored password** (opposite semantics, deliberately, per the design spec).
- `WebFormCommissioningAdapter::currentValues()` must never return the real stored WiFi password — always `""`.

---

### Task 1: `SetupModeRequestStore::requestOnNextBoot()` — `void` → `bool`

**Files:**
- Modify: `lib/McsEsp32/src/ports/SetupModeRequestStore.h`
- Modify: `lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.h`
- Modify: `lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.cpp`
- Modify: `test/support/FakeSetupModeRequestStore.h`

**Interfaces:**
- Produces: `virtual bool requestOnNextBoot() = 0;` on the `SetupModeRequestStore` port — Task 6's `main.cpp` wiring calls this and checks the returned `bool`.

No existing test (native or otherwise) currently calls `requestOnNextBoot()` on any implementation — confirmed by grepping `test/` for the name, which matches only `FakeSetupModeRequestStore.h`'s own declaration. This task is a pure interface-widening change with nothing existing to break; it's verified by full-suite + build success, not new test assertions.

- [ ] **Step 1: Change the port interface**

In `lib/McsEsp32/src/ports/SetupModeRequestStore.h`, change:

```cpp
    virtual void requestOnNextBoot() = 0;
```

to:

```cpp
    virtual bool requestOnNextBoot() = 0;
```

- [ ] **Step 2: Change the NVS adapter's declaration**

In `lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.h`, change:

```cpp
    void requestOnNextBoot() override;
```

to:

```cpp
    bool requestOnNextBoot() override;
```

- [ ] **Step 3: Change the NVS adapter's implementation**

In `lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.cpp`, replace:

```cpp
void NvsSetupModeRequestStore::requestOnNextBoot()
{
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.putBool(kKey, true);
    prefs.end();
}
```

with:

```cpp
bool NvsSetupModeRequestStore::requestOnNextBoot()
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false))
    {
        return false;
    }
    const bool ok = prefs.putBool(kKey, true) != 0;
    prefs.end();
    return ok;
}
```

`Preferences::begin()` returns `bool` (success/failure); `Preferences::putBool()` returns the number of bytes written, `0` on failure — both already exist in the ESP32 Arduino framework's `Preferences` library, no new dependency.

- [ ] **Step 4: Update the fake test double**

In `test/support/FakeSetupModeRequestStore.h`, replace:

```cpp
    void requestOnNextBoot() override
    {
        requested_ = true;
        requestOnNextBootCallCount++;
    }
```

with:

```cpp
    bool requestOnNextBoot() override
    {
        requested_ = true;
        requestOnNextBootCallCount++;
        return true;
    }
```

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: all suites still pass (this interface is not yet exercised by any test — this run confirms nothing else references the old `void` signature).

- [ ] **Step 6: Run the esp32dev build**

Run: `pio run -e esp32dev`
Expected: SUCCESS (confirms `NvsSetupModeRequestStore.cpp` compiles against the new signature; `McsEsp32` is listed directly in `esp32dev`'s `lib_deps` in `platformio.ini`, so PlatformIO compiles every `.cpp` in that library into its archive regardless of whether `main.cpp` references it yet).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/ports/SetupModeRequestStore.h lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.h lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.cpp test/support/FakeSetupModeRequestStore.h
git commit -m "feat: propagate NVS write failure from requestOnNextBoot()"
```

---

### Task 2: `WebFormCommissioningAdapter` — blank-channel clear + password keep-current

**Files:**
- Modify: `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp`
- Test: `test/test_web_form_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes: `CommissioningSession::draft()` (already exists, returns `const NodeConfig&`, added in #2c-b1).
- Produces: `WebFormCommissioningAdapter::submit()` and `::currentValues()` keep their existing signatures (`std::string submit(const WebFormSubmission&)`, `WebFormSubmission currentValues() const`) — only their internal behavior changes. Task 6's `main.cpp` does not call either method directly (it only constructs the adapter and hands it to `CaptivePortalServer`), so nothing downstream needs a signature update.

This is the task with genuine logic changes — the fix inverts one previously-tested behavior and adds new behavior. Read the existing full test file first; it is reproduced relevant-part-by-part below so you don't have to guess line numbers, but re-read the file yourself before editing since exact line numbers may drift.

- [ ] **Step 1: Replace the now-incorrect blank-channel test**

In `test/test_web_form_commissioning_adapter/test_main.cpp`, find and replace this existing test:

```cpp
TEST_CASE("an empty channel name leaves the stored name untouched, rather than clearing it")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("w", "p")
                   .withBroker("h", 1883)
                   .withChannelName(2, "LT2"));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    // form.channelJmriNames[1] (channel 2) is blank by default construction

    adapter.submit(form);

    REQUIRE(store.load().channelJmriNames[1] == "LT2");
}
```

with:

```cpp
TEST_CASE("a blank channel field clears a previously stored channel name")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("w", "p")
                   .withBroker("h", 1883)
                   .withChannelName(2, "LT2"));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    // form.channelJmriNames[1] (channel 2) is blank by default construction

    adapter.submit(form);

    REQUIRE(store.load().channelJmriNames[1].empty());
}
```

- [ ] **Step 2: Add the password-keep-current test**

Append to the end of `test/test_web_form_commissioning_adapter/test_main.cpp` (after the last existing `TEST_CASE`):

```cpp
TEST_CASE("a blank wifi password keeps the previously stored password")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("MyLayoutWifi", "existing-secret")
                   .withBroker("h", 1883));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    form.wifiPassword = "";

    const std::string response = adapter.submit(form);

    REQUIRE(response == "rebooting\n");
    REQUIRE(store.load().wifiPassword == "existing-secret");
}
```

- [ ] **Step 3: Add the currentValues()-blanks-password test**

Append immediately after the test from Step 2:

```cpp
TEST_CASE("currentValues never reflects a stored wifi password")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("MyLayoutWifi", "existing-secret")
                   .withBroker("h", 1883));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.wifiPassword.empty());
    REQUIRE(values.wifiSsid == "MyLayoutWifi");
}
```

- [ ] **Step 4: Run the suite to verify the new/changed tests fail**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: FAIL — "a blank channel field clears a previously stored channel name" fails (production code still `continue`s past blank channels, leaving `"LT2"` in place); "a blank wifi password keeps the previously stored password" fails (production code currently overwrites with the blank password); "currentValues never reflects a stored wifi password" fails (production code currently returns the real `"existing-secret"`).

- [ ] **Step 5: Fix `submit()` — drop the blank-channel skip guard**

In `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp`, replace:

```cpp
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        if (form.channelJmriNames[i].empty())
        {
            continue;
        }

        ParsedCommand turnoutCommand;
        turnoutCommand.kind = CommandKind::TurnoutName;
        turnoutCommand.intArg = i + 1;
        turnoutCommand.stringArg1 = form.channelJmriNames[i];
        response = session_.apply(turnoutCommand);
        if (response != "OK\n")
        {
            return response;
        }
    }
```

with:

```cpp
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        ParsedCommand turnoutCommand;
        turnoutCommand.kind = CommandKind::TurnoutName;
        turnoutCommand.intArg = i + 1;
        turnoutCommand.stringArg1 = form.channelJmriNames[i];
        response = session_.apply(turnoutCommand);
        if (response != "OK\n")
        {
            return response;
        }
    }
```

`NodeConfig::validate()` only rejects *duplicate non-empty* channel names (it `continue`s past empty ones in its own duplicate-check loop) and `withChannelName()` has no emptiness guard, so this is safe with zero domain-layer changes — confirmed by reading `lib/McsEsp32/src/domain/NodeConfig.cpp` directly.

- [ ] **Step 6: Fix `submit()` — blank password keeps current**

In the same file, replace:

```cpp
    ParsedCommand wifiCommand;
    wifiCommand.kind = CommandKind::Wifi;
    wifiCommand.stringArg1 = form.wifiSsid;
    wifiCommand.stringArg2 = form.wifiPassword;
    response = session_.apply(wifiCommand);
```

with:

```cpp
    ParsedCommand wifiCommand;
    wifiCommand.kind = CommandKind::Wifi;
    wifiCommand.stringArg1 = form.wifiSsid;
    wifiCommand.stringArg2 = form.wifiPassword.empty() ? session_.draft().wifiPassword : form.wifiPassword;
    response = session_.apply(wifiCommand);
```

- [ ] **Step 7: Fix `currentValues()` — never reflect the stored password**

In the same file, replace:

```cpp
    form.wifiPassword = config.wifiPassword;
```

with:

```cpp
    form.wifiPassword = "";
```

- [ ] **Step 8: Run the full suite to verify everything passes**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: PASS — all 9 test cases in this suite pass, including the 3 new/changed ones and the 6 pre-existing ones (in particular, "a fully valid submission saves and requests reboot" and "wifi credentials ... containing spaces round-trip intact" both use a non-blank password, so they take the `: form.wifiPassword` branch unchanged and still pass).

Also run: `pio test -e native`
Expected: full suite passes, no other suite references this file's behavior.

- [ ] **Step 9: Commit**

```bash
git add lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp test/test_web_form_commissioning_adapter/test_main.cpp
git commit -m "fix: clear blank channel names, keep current wifi password on blank"
```

---

### Task 3: `SetupFormRenderer` — never render the stored password

**Files:**
- Modify: `lib/McsEsp32/src/domain/SetupFormRenderer.h`
- Test: `test/test_setup_form_renderer/test_main.cpp`

**Interfaces:**
- Consumes: `WebFormSubmission::wifiPassword` (existing field, unchanged type).
- Produces: `SetupFormRenderer::render()` keeps its existing signature; only its output HTML changes for the password field.

Independent of Task 2 (different file, no code dependency) — both implement the same design decision (never surface the real password) from opposite ends: Task 2 stops `currentValues()` from returning it, this task stops `render()` from embedding it even if a caller passed one directly.

- [ ] **Step 1: Replace the password-escaping assertion in the existing render test**

In `test/test_setup_form_renderer/test_main.cpp`, replace:

```cpp
TEST_CASE("render embeds the escaped wifi ssid and password values")
{
    WebFormSubmission form = emptyValues();
    form.wifiSsid = "My\"Wifi";
    form.wifiPassword = "pass&word";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("My&quot;Wifi") != std::string::npos);
    REQUIRE(html.find("pass&amp;word") != std::string::npos);
}
```

with:

```cpp
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
```

- [ ] **Step 2: Run the suite to verify the new tests fail**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: FAIL — "render never embeds a wifi password..." fails (the field currently renders `pass&amp;word`); "render includes a hint..." fails (the hint text doesn't exist yet).

- [ ] **Step 3: Change the password field and add the hint**

In `lib/McsEsp32/src/domain/SetupFormRenderer.h`, inside `render()`, replace:

```cpp
        html += "<label>WiFi Password</label><input name='wifi_password' type='password' value='"
            + escapeHtml(values.wifiPassword) + "'>";
```

with:

```cpp
        html += "<label>WiFi Password</label><input name='wifi_password' type='password' value=''>";
        html += "<p class='hint'>Leave blank to keep the current password.</p>";
```

- [ ] **Step 4: Add the `.hint` CSS class**

In the same file, inside the `kStyle` string, replace:

```cpp
        ".warning{color:#b45309;font-size:0.8rem;margin:8px 0 0;}";
```

with:

```cpp
        ".warning{color:#b45309;font-size:0.8rem;margin:8px 0 0;}"
        ".hint{color:#6b7280;font-size:0.8rem;margin:4px 0 0;}";
```

- [ ] **Step 5: Run the suite to verify everything passes**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: PASS — all test cases pass, including the two split from the original and the new hint-text test.

Also run: `pio test -e native`
Expected: full suite passes.

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/domain/SetupFormRenderer.h test/test_setup_form_renderer/test_main.cpp
git commit -m "fix: never render the stored wifi password in the setup form"
```

---

### Task 4: `CaptivePortalServer::begin()` — WPA2 passphrase parameter

**Files:**
- Modify: `lib/McsEsp32/src/adapters/CaptivePortalServer.h`
- Modify: `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp`

**Interfaces:**
- Produces: `void begin(const std::string& apName, const std::string& passphrase);` — Task 6's `main.cpp` calls this with `WIRELESS_SETUP_AP_PASSPHRASE` (defined in Task 6, per Global Constraints).

`#ifdef ARDUINO`-guarded; no native test exists or is added for this class (matches its existing build-check-only convention, same as `NvsConfigStore`/`WiFiLink`/`MqttLink`).

- [ ] **Step 1: Change the header declaration**

In `lib/McsEsp32/src/adapters/CaptivePortalServer.h`, change:

```cpp
    void begin(const std::string& apName);
```

to:

```cpp
    void begin(const std::string& apName, const std::string& passphrase);
```

- [ ] **Step 2: Change the implementation**

In `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp`, replace:

```cpp
void CaptivePortalServer::begin(const std::string& apName)
{
    WiFi.softAP(apName.c_str());

    IPAddress apIp = WiFi.softAPIP();
    dnsServer_.start(53, "*", apIp);
```

with:

```cpp
void CaptivePortalServer::begin(const std::string& apName, const std::string& passphrase)
{
    WiFi.softAP(apName.c_str(), passphrase.c_str());

    IPAddress apIp = WiFi.softAPIP();
    dnsServer_.start(53, "*", apIp);
```

- [ ] **Step 3: Run the esp32dev build**

Run: `pio run -e esp32dev`
Expected: SUCCESS. (No caller exists yet — Task 6 adds one — but `McsEsp32` is listed directly in `esp32dev`'s `lib_deps`, so PlatformIO compiles this `.cpp` into the library archive regardless; a signature mismatch here would still fail the build on its own.)

Also run: `pio test -e native`
Expected: unaffected (this file is `#ifdef ARDUINO`-guarded out of the native build entirely).

- [ ] **Step 4: Commit**

```bash
git add lib/McsEsp32/src/adapters/CaptivePortalServer.h lib/McsEsp32/src/adapters/CaptivePortalServer.cpp
git commit -m "feat: require a WPA2 passphrase to join the wireless setup AP"
```

---

### Task 5: Document the AP passphrase

**Files:**
- Modify: `docs/ESP32_Turnout_Panel_Implementation.md`

**Interfaces:** None (documentation only).

- [ ] **Step 1: Add a new section**

In `docs/ESP32_Turnout_Panel_Implementation.md`, immediately after the `## Power` section (after its closing `---` separator, before the `## JMRI Communication (MQTT)` heading), insert:

```markdown
## Wireless Setup Access Point

**Decision (2026-08-30):** commissioning over Wi-Fi (sub-project #2c) opens a
temporary access point named `MaltBee-Setup-XXXX` (last 4 hex digits of the
ESP32's MAC address) whenever the panel boots into wireless setup mode —
either a factory-fresh panel with no valid configuration, or an
already-commissioned panel where a technician held turnout buttons T1+T2
together for 3 seconds to request it.

The AP requires a WPA2 passphrase to join: **`maltbee-setup`**. This is
fixed and shared across every panel (not per-panel or MAC-derived) — write
it down for field commissioning, since a technician without it cannot join
the AP to reach the setup web form.

The panel's own current WiFi password is never displayed by the setup
form — a blank password field on submission keeps the existing password
unchanged. Turnout JMRI name fields work the opposite way: a blank field on
submission clears that turnout's assigned name.

---
```

- [ ] **Step 2: Commit**

```bash
git add docs/ESP32_Turnout_Panel_Implementation.md
git commit -m "docs: record the wireless setup AP passphrase and blank-field semantics"
```

---

### Task 6: Wire everything into `src/esp32/main.cpp`

**Files:**
- Modify: `src/esp32/main.cpp`

**Interfaces:**
- Consumes: `BootMode`/`BootModeSelector::select(const NodeConfig&, bool)` (domain, #2c-a); `SetupModeRequestStore`/`NvsSetupModeRequestStore` with `bool requestOnNextBoot()` + `bool consumeRequest()` (Task 1); `GatedDigitalInput(DigitalInput&)` with `void setSuppressed(bool)` + `bool isActive() const` (#2c-a); `ComboSetupModeTrigger(DigitalInput&, DigitalInput&, Clock&, unsigned long)` with `void update()` + `bool isHolding() const` + `bool requested() const` (#2c-a); `EspDeviceIdentity` with `MacAddress mac() const` (#2c-b1); `SetupApName::from(const MacAddress&)` returning `std::string` (#2c-b1); `WebFormCommissioningAdapter(CommissioningSession&)` (#2c-b1); `CaptivePortalServer(WebFormCommissioningAdapter&)` with `void begin(const std::string&, const std::string&)` (Task 4) and `void poll()` (#2c-b1).
- Produces: nothing consumed by a later task — this is the final task.

This is the highest-judgment task in the plan: it restructures the composition root's `setup()`/`loop()` control flow, not just adding an isolated class. `src/` is excluded from the `native` test build (`test_build_src = false`), so there is no unit test for this file — verification is `pio run -e esp32dev` build success plus a careful manual read-through against this task's exact target code below. Do not improvise beyond what's written here; every line of the target `setup()`/`loop()`/globals is specified.

Depends on Tasks 1 and 4 being complete (needs `requestOnNextBoot()`'s new `bool` return and `CaptivePortalServer::begin()`'s new two-argument signature to compile). Does not depend on Tasks 2/3/5 functionally (this file never calls `WebFormCommissioningAdapter::submit()`/`currentValues()` or `SetupFormRenderer::render()` directly — `CaptivePortalServer` calls those internally) — but Tasks 2/3 should still land first so the full branch's behavior is correct end-to-end when this task is reviewed.

- [ ] **Step 1: Add the new includes**

In `src/esp32/main.cpp`, replace the existing include block:

```cpp
#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ArduinoDigitalOutput.h"
#include "adapters/EspUartPort.h"
#include "adapters/JmriFeedbackSource.h"
#include "adapters/JmriTurnoutCommandAdapter.h"
#include "adapters/LedPairStation.h"
#include "adapters/MatrixDigitalInput.h"
#include "adapters/MqttLink.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WiFiLink.h"
#include "application/CommissioningSession.h"
#include "domain/LedPairDriver.h"
#include "domain/MatrixScanner.h"
#include "domain/NodeConfig.h"
#include "ports/TurnoutCommandPort.h"
```

with:

```cpp
#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ArduinoDigitalOutput.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/ComboSetupModeTrigger.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspUartPort.h"
#include "adapters/GatedDigitalInput.h"
#include "adapters/JmriFeedbackSource.h"
#include "adapters/JmriTurnoutCommandAdapter.h"
#include "adapters/LedPairStation.h"
#include "adapters/MatrixDigitalInput.h"
#include "adapters/MqttLink.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "adapters/WiFiLink.h"
#include "application/CommissioningSession.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/LedPairDriver.h"
#include "domain/MatrixScanner.h"
#include "domain/NodeConfig.h"
#include "domain/SetupApName.h"
#include "ports/TurnoutCommandPort.h"
```

- [ ] **Step 2: Add the two new constants**

In the first anonymous namespace (the one containing `BLINK_INTERVAL_MS`), replace:

```cpp
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
    constexpr unsigned long RETRY_INTERVAL_MS = 5000;
    constexpr unsigned long UART_BAUD_RATE = 115200;
}
```

with:

```cpp
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
    constexpr unsigned long RETRY_INTERVAL_MS = 5000;
    constexpr unsigned long UART_BAUD_RATE = 115200;
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
    constexpr const char* WIRELESS_SETUP_AP_PASSPHRASE = "maltbee-setup";
}
```

- [ ] **Step 3: Add the commissioning-session-derived globals**

Replace:

```cpp
EspUartPort uartPort(UART_BAUD_RATE);
NvsConfigStore configStore;
CommissioningSession commissioningSession(configStore);
SerialCommissioningAdapter serialCommissioningAdapter(uartPort, commissioningSession);

NodeConfig runningConfig = configStore.load();
const bool configValid = runningConfig.validate().empty();
```

with:

```cpp
EspUartPort uartPort(UART_BAUD_RATE);
NvsConfigStore configStore;
CommissioningSession commissioningSession(configStore);
SerialCommissioningAdapter serialCommissioningAdapter(uartPort, commissioningSession);
WebFormCommissioningAdapter webFormAdapter(commissioningSession);
CaptivePortalServer captivePortalServer(webFormAdapter);

NvsSetupModeRequestStore setupModeRequestStore;
NodeConfig runningConfig = configStore.load();
const bool wirelessSetupRequested = setupModeRequestStore.consumeRequest();
const BootMode bootMode = BootModeSelector::select(runningConfig, wirelessSetupRequested);
const bool configValid = runningConfig.validate().empty();
```

`consumeRequest()` (read-and-clear) must run exactly once per boot, at this point, alongside the other one-shot boot-time reads.

- [ ] **Step 4: Add the combo trigger and gated-input array, after `matrixButtons`**

After the closing `};` of the existing `MatrixDigitalInput matrixButtons[12] = { ... };` array and before `WiFiLink wifiLink(...)`, insert:

```cpp
ComboSetupModeTrigger setupTrigger(matrixButtons[0], matrixButtons[1], systemClock, SETUP_TRIGGER_HOLD_MS);

GatedDigitalInput gatedButtons[12] = {
    GatedDigitalInput(matrixButtons[0]),  GatedDigitalInput(matrixButtons[1]),
    GatedDigitalInput(matrixButtons[2]),  GatedDigitalInput(matrixButtons[3]),
    GatedDigitalInput(matrixButtons[4]),  GatedDigitalInput(matrixButtons[5]),
    GatedDigitalInput(matrixButtons[6]),  GatedDigitalInput(matrixButtons[7]),
    GatedDigitalInput(matrixButtons[8]),  GatedDigitalInput(matrixButtons[9]),
    GatedDigitalInput(matrixButtons[10]), GatedDigitalInput(matrixButtons[11]),
};
```

`setupTrigger` reads the raw `matrixButtons[0]`/`[1]` (T1/T2) — never `gatedButtons` (see Global Constraints).

- [ ] **Step 5: Switch the 12 `ToggleTurnoutStation`s to the gated inputs**

In the `ToggleTurnoutStation stations[12] = { ... };` array, every element currently reads `matrixButtons[i]` as its third constructor argument, e.g.:

```cpp
    ToggleTurnoutStation(TURNOUT_CONFIGS[0].address, TURNOUT_CONFIGS[0].name, matrixButtons[0],
                          ledStations[0].green(), ledStations[0].red(), systemClock, turnoutCommandPort),
```

Change every one of the 12 entries' `matrixButtons[i]` to `gatedButtons[i]` (index unchanged, only the array name changes), e.g. the entry above becomes:

```cpp
    ToggleTurnoutStation(TURNOUT_CONFIGS[0].address, TURNOUT_CONFIGS[0].name, gatedButtons[0],
                          ledStations[0].green(), ledStations[0].red(), systemClock, turnoutCommandPort),
```

Repeat for all 12 entries (indices 0 through 11).

- [ ] **Step 6: Rewrite `setup()`**

Replace the entire `setup()` function:

```cpp
void setup()
{
    uartPort.begin();

    matrixRow0.begin();
    matrixRow1.begin();
    matrixRow2.begin();
    matrixCol0.begin();
    matrixCol1.begin();
    matrixCol2.begin();
    matrixCol3.begin();

    for (auto& ledStation : ledStations)
    {
        ledStation.begin();
    }

    for (auto& station : stations)
    {
        station.begin();
    }

    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
    }

    uartPort.write(configValid ? "MaltBee panel ready (configured).\n"
                                : "MaltBee panel needs commissioning. Type 'show'.\n");
}
```

with:

```cpp
void setup()
{
    uartPort.begin();

    matrixRow0.begin();
    matrixRow1.begin();
    matrixRow2.begin();
    matrixCol0.begin();
    matrixCol1.begin();
    matrixCol2.begin();
    matrixCol3.begin();

    for (auto& ledStation : ledStations)
    {
        ledStation.begin();
    }

    for (auto& station : stations)
    {
        station.begin();
    }

    if (bootMode == BootMode::WirelessSetup)
    {
        EspDeviceIdentity identity;
        const std::string apName = SetupApName::from(identity.mac());
        captivePortalServer.begin(apName, WIRELESS_SETUP_AP_PASSPHRASE);
        uartPort.write("MaltBee panel in wireless setup mode. Connect to " + apName + "\n");
        return;
    }

    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
    }

    uartPort.write(configValid ? "MaltBee panel ready (configured).\n"
                                : "MaltBee panel needs commissioning. Type 'show'.\n");
}
```

GPIO/LED/station `begin()` calls stay unconditional; only the network-stack `begin()` calls and the boot-log line are gated by boot mode, on top of the existing `configValid` gate.

- [ ] **Step 7: Rewrite `loop()`**

Replace the entire `loop()` function:

```cpp
void loop()
{
    matrixScanner.update();

    serialCommissioningAdapter.poll();
    if (serialCommissioningAdapter.rebootRequested())
    {
        Serial.flush();
        ESP.restart();
    }

    if (configValid)
    {
        wifiLink.poll();
        if (wifiLink.connected())
        {
            mqttLink.poll();
        }
    }

    if (configValid && mqttLink.connected())
    {
        TurnoutFeedback feedback{};
        while (feedbackSource.poll(feedback))
        {
            for (auto& station : stations)
            {
                station.applyFeedback(feedback);
            }
        }
    }
    else
    {
        for (auto& station : stations)
        {
            station.clearIndicator();
        }
    }

    for (auto& ledStation : ledStations)
    {
        ledStation.update();
    }

    for (auto& station : stations)
    {
        station.update();
    }
}
```

with:

```cpp
void loop()
{
    serialCommissioningAdapter.poll();
    if (serialCommissioningAdapter.rebootRequested())
    {
        Serial.flush();
        ESP.restart();
    }

    if (bootMode == BootMode::WirelessSetup)
    {
        captivePortalServer.poll();
        return;
    }

    matrixScanner.update();

    setupTrigger.update();
    gatedButtons[0].setSuppressed(setupTrigger.isHolding());
    gatedButtons[1].setSuppressed(setupTrigger.isHolding());

    if (setupTrigger.requested())
    {
        const bool stored = setupModeRequestStore.requestOnNextBoot();
        if (stored)
        {
            uartPort.write("Entering wireless setup...\n");
            Serial.flush();
            ESP.restart();
        }
        else
        {
            uartPort.write("Failed to persist wireless setup request; staying in normal mode.\n");
        }
    }

    if (configValid)
    {
        wifiLink.poll();
        if (wifiLink.connected())
        {
            mqttLink.poll();
        }
    }

    if (configValid && mqttLink.connected())
    {
        TurnoutFeedback feedback{};
        while (feedbackSource.poll(feedback))
        {
            for (auto& station : stations)
            {
                station.applyFeedback(feedback);
            }
        }
    }
    else
    {
        for (auto& station : stations)
        {
            station.clearIndicator();
        }
    }

    for (auto& ledStation : ledStations)
    {
        ledStation.update();
    }

    for (auto& station : stations)
    {
        station.update();
    }
}
```

Serial commissioning polling and its reboot check stay first, unconditionally, so a technician with a serial cable can still use it during `WirelessSetup` mode. The `WirelessSetup` branch then returns before touching the matrix, combo trigger, WiFi-station, MQTT, or any station/LED logic. In the normal branch, `setupTrigger.requested()` is handled the same tick it becomes true (its `requestedThisTick_` resets on the very next `update()` call).

- [ ] **Step 8: Run the esp32dev build**

Run: `pio run -e esp32dev`
Expected: SUCCESS. Note the RAM/Flash usage reported — unlike #2c-b1 (which reported byte-identical usage because nothing referenced the new classes), this build should show a measurable increase, since `main.cpp` now actually references `CaptivePortalServer`/`WebFormCommissioningAdapter`/`ComboSetupModeTrigger`/`GatedDigitalInput`/`EspDeviceIdentity` for the first time.

- [ ] **Step 9: Run the megaatmega2560 build (regression check)**

Run: `pio run -e megaatmega2560`
Expected: SUCCESS, usage unchanged from before this branch — `src/esp32/main.cpp` is excluded from this environment's `build_src_filter` entirely.

- [ ] **Step 10: Run the full native suite (regression check)**

Run: `pio test -e native`
Expected: all suites pass, unchanged count from before this task (this file is never part of the `native` build).

- [ ] **Step 11: Manually re-read `setup()`/`loop()` against this task's target code**

Confirm line-by-line: (a) GPIO/station `begin()` calls remain unconditional; (b) the `WirelessSetup` branch in `setup()` returns before the `configValid` block; (c) the `WirelessSetup` branch in `loop()` comes after serial polling but before `matrixScanner.update()`, and returns; (d) `setupTrigger` reads `matrixButtons[0]`/`[1]`, never `gatedButtons`; (e) all 12 `stations[]` entries read `gatedButtons[i]`, not `matrixButtons[i]`; (f) `requestOnNextBoot()`'s returned `bool` gates whether `ESP.restart()` is actually called.

- [ ] **Step 12: Commit**

```bash
git add src/esp32/main.cpp
git commit -m "feat: wire BootMode branching and the T1+T2 setup trigger into main.cpp"
```
