# ESP32 Wireless Setup Composition-Root Wiring (Sub-project #2c-b2) — Design

This is sub-project **#2c-b2**, the final piece of sub-project #2c (Wireless
captive-portal commissioning), after #2c-a (complete and merged:
`BootMode`/`BootModeSelector`, `SetupModeRequestStore`/
`NvsSetupModeRequestStore`, `GatedDigitalInput`, `ComboSetupModeTrigger`) and
#2c-b1 (complete and merged: `MacAddress`/`SetupApName`/`EspDeviceIdentity`,
`WebFormSubmission`, `CommissioningSession::draft()`,
`WebFormCommissioningAdapter`, `SetupFormRenderer`, `CaptivePortalServer` —
none of it referenced by `src/esp32/main.cpp` yet). This spec wires all of
the above into `src/esp32/main.cpp` and resolves the three decisions #2c-a
and #2c-b1's final reviews explicitly deferred here rather than silently
inheriting them.

Assessed and rejected: further splitting #2c-b2. Unlike #7 and #2c
themselves, this sub-project's work is one cohesive unit — composition-root
wiring plus two small, tightly-coupled production fixes that only matter
once the wiring exists. There is no independently valuable slice smaller
than "finish wiring wireless setup."

## Context: why "must not construct" needed correcting

Sub-project #2c-b1's non-goals framed this sub-project's job as making sure
`BootMode::WirelessSetup` "does NOT construct the WiFiLink/MqttLink/JMRI/
ToggleTurnoutStation graph." Reading `src/esp32/main.cpp` (built in #7b)
shows this framing doesn't fit the code as it exists: every adapter/station
in that file is a plain C++ global, constructed unconditionally by the
runtime before `setup()` ever executes. There is no way to skip
*constructing* a global based on a runtime boot-mode decision without
converting the whole file's construction pattern to heap allocation or
`std::optional`-wrapped lazy init — a much larger, riskier restructuring for
no functional benefit, since construction alone (no `begin()` called) never
touches hardware or the network.

The actual requirement is narrower: **don't operate** the WiFi-station/MQTT/
JMRI stack while the panel is running as a captive-portal AP. That's exactly
what `src/esp32/main.cpp` already does for an invalid config today — it
gates `wifiLink.begin()`/`mqttLink.begin()`/`.poll()` behind a `configValid`
boolean, not behind whether those objects exist. This spec extends that same
gate to also check the boot mode, keeping every existing global's
construction unconditional.

## Decisions (confirmed via Q&A)

1. **No further split** — one sub-project covers the wiring and both
   deferred-decision fixes.
2. **Staggered two-finger combo press**: accept the possible single spurious
   toggle on whichever of T1/T2 is pressed first. Zero changes to normal
   button-press behavior anywhere on the panel; the gesture only matters
   when a technician is deliberately entering setup mode, and the operator
   self-corrects by pressing that turnout's button again. Rejected
   command-on-release (makes T1/T2 behave differently from the other 10
   buttons) and a cancelable hold-off (adds a timing state machine to the
   one interaction meant to feel instant) as not worth the complexity for a
   rare, self-correcting edge case.
3. **AP security**: give the setup AP a fixed WPA2 passphrase, and stop the
   web form from ever reflecting the real stored WiFi password back into the
   page. Both are cheap, contained changes that close a real exposure
   (anyone in radio range could otherwise join with no credential and read
   the layout's live WiFi password via view-source) without adding new
   moving parts.
4. **`EspDeviceIdentity::mac()` call-site timing**: called inside `setup()`,
   only inside the `WirelessSetup` branch — not at global scope. It's the
   only place the MAC is ever needed, and this project has hit two prior
   "a global constructor ran before something it depended on was ready"
   bugs (NVS init in #7b, the `clock`/`clock_t` collision in #5); deferring
   a hardware read into `setup()` and only calling it when actually needed
   avoids adding a third global-scope hazard candidate for no benefit.

## Components

### `src/esp32/main.cpp` — boot-time globals

```cpp
NvsSetupModeRequestStore setupModeRequestStore;
NodeConfig runningConfig = configStore.load();
const bool wirelessSetupRequested = setupModeRequestStore.consumeRequest();
const BootMode bootMode = BootModeSelector::select(runningConfig, wirelessSetupRequested);
const bool configValid = runningConfig.validate().empty();
```

Placed alongside the existing `runningConfig`/`configValid` globals.
`consumeRequest()` (read-and-clear) must run exactly once per boot, at this
point — the same one-shot timing `configStore.load()` already has.

`BootMode::Normal` and `BootMode::NeedsCommissioning` collapse to the same
code path structurally (both differ from `WirelessSetup` in the same way,
and from each other only in the boot-log line) — `configValid` already
distinguishes them for every behavioral purpose `main.cpp` cares about.
Only `bootMode == BootMode::WirelessSetup` needs an actual branch.

### `src/esp32/main.cpp` — new globals for the web form / AP

```cpp
WebFormCommissioningAdapter webFormAdapter(commissioningSession);
CaptivePortalServer captivePortalServer(webFormAdapter);
```

`commissioningSession` already exists (constructed for bench-serial
commissioning) and is reused as-is — both commissioning paths share one
`CommissioningSession`/draft, consistent with only one being meaningfully
exercised in a given boot. Bench-serial commissioning is left running
during `WirelessSetup` mode too (harmless, and lets a technician with both
serial and phone access use either).

### `src/esp32/main.cpp` — combo trigger + suppression wiring

```cpp
constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;

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

`setupTrigger` reads the **raw** `matrixButtons[0]`/`[1]` (T1/T2) directly —
never the gated wrappers. Wiring it to gated inputs would deadlock:
suppression would force `bothActive` false the instant `holding_` becomes
true, causing endless on/off oscillation and the gesture never completing.

All 12 `ToggleTurnoutStation`s switch from constructing against
`matrixButtons[i]` to `gatedButtons[i]` — uniformly, matching this file's
existing table-driven-array convention (`TURNOUT_CONFIGS`, `ledStations`,
`matrixButtons`, `stations` are all already parallel 12-element arrays
walked by range-`for`), even though only indices 0/1 are ever actually
suppressed. `gatedButtons[2..11]` stay permanently unsuppressed
(`suppressed_` defaults to `false` and nothing ever calls
`setSuppressed(true)` on them).

### `src/esp32/main.cpp` — `setup()`

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

GPIO/station `begin()` calls stay unconditional (cheap, no hardware
conflict with WiFi/AP mode) — only the network-stack `begin()` calls and the
boot-log line are gated, minimizing the diff against today's `setup()`.
`WIRELESS_SETUP_AP_PASSPHRASE` is a new file-scope `constexpr const char*`
constant (see "AP security fix" below).

### `src/esp32/main.cpp` — `loop()`

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

Serial commissioning polling and its own reboot check stay first,
unconditionally, so a technician with a serial cable can still use it
during `WirelessSetup` mode. The `WirelessSetup` branch then returns before
touching the matrix, combo trigger, WiFi-station, MQTT, or any station/LED
logic — none of it is meaningful while the panel is an isolated AP.

In the normal branch, `setupTrigger.requested()` is handled immediately
(same tick it becomes true), since `ComboSetupModeTrigger::update()` resets
`requestedThisTick_` on the very next call. `requestOnNextBoot()`'s new
`bool` return (see below) is checked before restarting — a persistence
failure now produces a visible serial message and keeps the panel running
normally, instead of silently rebooting into a mode that immediately
reverts back to `Normal`/`NeedsCommissioning`.

### `SetupModeRequestStore::requestOnNextBoot()` — `void` → `bool`

```cpp
// ports/SetupModeRequestStore.h
class SetupModeRequestStore
{
public:
    virtual ~SetupModeRequestStore() = default;

    virtual bool requestOnNextBoot() = 0;
    virtual bool consumeRequest() = 0;
};
```

```cpp
// adapters/NvsSetupModeRequestStore.cpp
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

`Preferences::begin()` and `putBool()` both already return values indicating
success in the ESP32 Arduino framework (`putBool` returns the number of
bytes written, `0` on failure) — this change only starts checking and
propagating them; no new capability is added to the underlying library.
This is the first sub-project to actually call `requestOnNextBoot()` from
`main.cpp`, which is what makes the previously-silent failure mode worth
fixing now.

`FakeSetupModeRequestStore` (`test/support/`) updates its override to return
`true`:

```cpp
bool requestOnNextBoot() override
{
    requested_ = true;
    requestOnNextBootCallCount++;
    return true;
}
```

No existing test currently calls `requestOnNextBoot()` on the fake (it's
never been wired to a caller before this sub-project), so this is a pure
signature-widening change with nothing to break.

### `WebFormCommissioningAdapter` — blank-channel fix

`NodeConfig::validate()` only rejects empty `wifiSsid`/`brokerHost` and
duplicate *non-empty* channel names — it already tolerates an empty channel
name outright (skips it via `continue` in its duplicate-check loop), and
`withChannelName()` has no length/emptiness guard. So dropping the `submit()`
guard is safe with zero domain-layer changes:

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

A blank channel field now genuinely clears that channel's stored name,
matching `SetupFormRenderer`'s existing copy ("Leave a channel blank to
leave it unconfigured") — no copy change needed. This **inverts** the
#2c-b1 test `"an empty channel name leaves the stored name untouched,
rather than clearing it"` in `test/test_web_form_commissioning_adapter/
test_main.cpp` — the plan must replace that test's assertion (and ideally
its name) to check the new, opposite behavior, not just delete it.

### `WebFormCommissioningAdapter` — AP security fix (password handling)

```cpp
std::string WebFormCommissioningAdapter::submit(const WebFormSubmission& form)
{
    std::string response = session_.apply(CommandLineParser::parse("id " + form.nodeId));
    if (response != "OK\n") { return response; }

    ParsedCommand wifiCommand;
    wifiCommand.kind = CommandKind::Wifi;
    wifiCommand.stringArg1 = form.wifiSsid;
    wifiCommand.stringArg2 = form.wifiPassword.empty() ? session_.draft().wifiPassword : form.wifiPassword;
    response = session_.apply(wifiCommand);
    if (response != "OK\n") { return response; }

    // ... broker / channel loop / save / reboot unchanged from #2c-b1
}

WebFormSubmission WebFormCommissioningAdapter::currentValues() const
{
    const NodeConfig& config = session_.draft();

    WebFormSubmission form;
    form.nodeId = config.nodeId == 0 ? "" : std::to_string(config.nodeId);
    form.wifiSsid = config.wifiSsid;
    form.wifiPassword = "";  // never reflect the stored password back to the page
    form.brokerHost = config.brokerHost;
    form.brokerPort = std::to_string(config.brokerPort);
    form.channelJmriNames = config.channelJmriNames;

    return form;
}
```

A blank password field means "keep the current password" (reads it from
`session_.draft()`, which #2c-b1 already added `draft()` to expose) —
deliberately the *opposite* blank-semantics from channels, since clearing a
WiFi password outright is rarely what an operator submitting the rest of
the form wants, whereas clearing a channel name is a normal, common
operation. `currentValues()` never echoes a real password back regardless
of whether the caller intends to keep or change it, closing the
view-source exposure independent of the AP-join exposure below.

Known limitation, accepted as out of scope: there is no way to *explicitly*
set the WiFi password to empty (e.g. switching to an open network) through
the web form — the operator would need bench-serial for that. Not
requested, and layouts running open WiFi are not a case this project
otherwise supports.

### `SetupFormRenderer` — password field copy

```cpp
html += "<label>WiFi Password</label><input name='wifi_password' type='password' value=''>";
html += "<p class='hint'>Leave blank to keep the current password.</p>";
```

The password `<input>`'s `value` attribute is now always the literal empty
string (never `escapeHtml(values.wifiPassword)`), matching
`currentValues()` above. A new `.hint` CSS class (visually distinct from
the existing `.warning` used for the channel-blanking note) is added to
`kStyle`, styled as a plain muted caption.

### `CaptivePortalServer::begin()` — WPA2 passphrase

```cpp
void CaptivePortalServer::begin(const std::string& apName, const std::string& passphrase)
{
    WiFi.softAP(apName.c_str(), passphrase.c_str());

    IPAddress apIp = WiFi.softAPIP();
    dnsServer_.start(53, "*", apIp);

    webServer_.on("/", [this]() { handleRoot(); });
    webServer_.on("/submit", HTTP_POST, [this]() { handleSubmit(); });
    webServer_.onNotFound([this]() { handleRoot(); });
    webServer_.begin();
}
```

Header signature becomes `void begin(const std::string& apName, const
std::string& passphrase);`. `WiFi.softAP(ssid, password)` requires the
password be empty (open) or at least 8 characters (WPA2 minimum) — the
`main.cpp` constant below satisfies that.

### `src/esp32/main.cpp` — the passphrase constant

```cpp
constexpr const char* WIRELESS_SETUP_AP_PASSPHRASE = "maltbee-setup";
```

Placed in the existing anonymous namespace at the top of `main.cpp`
alongside `BLINK_INTERVAL_MS`/`DEFAULT_LED_COLOR`/`RETRY_INTERVAL_MS`/
`UART_BAUD_RATE`, not as a new standalone global. 13 characters, satisfies
the WPA2 8-character minimum. Fixed and shared
across every panel (not MAC-derived) — this project has no per-panel secret
provisioning mechanism, and a fixed, documented passphrase is the same
model consumer routers use for their own setup APs. Documented in
`docs/ESP32_Turnout_Panel_Implementation.md` (new short "Wireless Setup
Access Point" section) so a technician commissioning a panel in the field
knows it without reading source.

## Testing

Everything above is either composition-root wiring (build-check only, via
`pio run -e esp32dev`) or a change to an already native-tested class:

- **`src/esp32/main.cpp`**: no native test (it never has one — `native`
  doesn't build `src/`). Verified via `pio run -e esp32dev` build success
  and a manual read-through of `setup()`/`loop()` against this spec.
- **`SetupModeRequestStore` / `NvsSetupModeRequestStore` / 
  `FakeSetupModeRequestStore`**: interface signature change only, no new
  test needed for the fake (`NvsSetupModeRequestStore` stays build-check
  only, matching its existing convention). If any existing test exercises
  `FakeSetupModeRequestStore::requestOnNextBoot()`'s return value, verify
  it during implementation — none is currently known to.
- **`WebFormCommissioningAdapter`** (`test/test_web_form_commissioning_adapter/`):
  - Replace the existing "empty channel name leaves the stored name
    untouched" test with one asserting the opposite: submitting a form with
    a blank channel field over a config that previously had a name set for
    that channel results in that channel reading back empty.
  - New test: submitting a form with a blank `wifiPassword` over a session
    whose draft already has a non-empty password results in the *original*
    password being retained (verify via `FakeConfigStore.load()` after
    `save()`), not cleared.
  - New test: submitting a form with a non-blank `wifiPassword` still sets
    it, unchanged from #2c-b1's existing coverage — confirm the existing
    happy-path test still passes with the new logic (it should, since a
    non-empty password takes the `: form.wifiPassword` branch).
  - New test: `currentValues()` returns `wifiPassword == ""` even when the
    session's draft has a non-empty stored password.
- **`SetupFormRenderer`** (`test/test_setup_form_renderer/`): update/add a
  case asserting the password `<input>`'s `value` attribute is always
  `value=''` regardless of `values.wifiPassword`'s content, and that the new
  hint text renders.
- **`CaptivePortalServer`**: build-check only via `pio run -e esp32dev`,
  same convention as before — verify the two-argument `begin()` call in
  `main.cpp` compiles and the passphrase constant is passed through.

## File layout

- Modify: `src/esp32/main.cpp` (boot-mode globals, combo trigger + gated
  inputs, `setup()`/`loop()` branching, AP passphrase constant)
- Modify: `lib/McsEsp32/src/ports/SetupModeRequestStore.h` (`requestOnNextBoot()` return type)
- Modify: `lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.h`/`.cpp` (return type + failure propagation)
- Modify: `test/support/FakeSetupModeRequestStore.h` (return type)
- Modify: `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp` (blank-channel fix, password keep-current logic, `currentValues()` password blanking)
- Modify: `lib/McsEsp32/src/domain/SetupFormRenderer.h` (password field always blank, hint copy, new CSS class)
- Modify: `lib/McsEsp32/src/adapters/CaptivePortalServer.h`/`.cpp` (`begin()` gains passphrase parameter)
- Modify: `test/test_web_form_commissioning_adapter/test_main.cpp`, `test/test_setup_form_renderer/test_main.cpp` (per Testing above)
- Modify: `docs/ESP32_Turnout_Panel_Implementation.md` (new "Wireless Setup Access Point" section documenting the passphrase)

## Non-goals

- Any change to `ComboSetupModeTrigger`, `GatedDigitalInput`, `BootMode`,
  `BootModeSelector`, `MacAddress`, `SetupApName`, or `EspDeviceIdentity`'s
  own implementation — all reused exactly as built in #2c-a/#2c-b1.
- Actually closing the staggered-press gap (accepted as-is per Decision 2).
- Any change to bench-serial commissioning's own behavior or tests.
- Field identification / multi-panel collision safety (sub-project #2d).
- Physical hardware bring-up / on-device verification (sub-project #8) —
  this sub-project's own hardware-touching pieces (`CaptivePortalServer`,
  `NvsSetupModeRequestStore`, the GPIO wiring) get build-check verification
  only, same as every prior ESP32 adapter in this project.
