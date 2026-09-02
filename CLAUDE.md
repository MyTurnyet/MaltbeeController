# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Committing

Always use the `/arlo-commits` skill when committing changes in this repository — do not hand-write commit messages or `git commit` directly. Invoke it whenever the user asks for a commit, even if they don't name the skill explicitly.

## Project Purpose

MaltBee Control System (MCS) — a reusable Arduino/PlatformIO control platform for model railroad panels, targeting a Mega 2560. It replaces Team Digital SRC8 boards: reading panel pushbuttons, driving panel LEDs, tracking turnout state, and sending/receiving turnout commands over LocoNet.

The full architecture and milestone-by-milestone roadmap live in `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`. Read it before starting any non-trivial feature — it defines the target directory layout, every domain/application class with its intended responsibilities and test list, and the milestone order (native test harness → I/O ports → Indicator → Button → Turnout → TurnoutIndicator → TurnoutControl → hardware integration → LocoNet send → LocoNet feedback → multiple turnouts → routes → persistent config → hardening). Follow that milestone order rather than jumping ahead (e.g. don't start LocoNet work before the domain layer and native tests are in place).

## Commands

```bash
pio test -e native                       # run host-native unit tests (no hardware needed)
pio run -e megaatmega2560                # build the firmware for the Mega 2560
pio run -e megaatmega2560 --target upload
pio device monitor                       # serial monitor, 115200 baud
```

To run a single native test file, use PlatformIO's filter flag, e.g.:
```bash
pio test -e native -f test_indicator
```

There is no `native` build target for `src/` (`test_build_src = false` in `platformio.ini`) — native test binaries only compile `test/` plus whatever `lib/` code they include, not `main.cpp`.

On Windows, run `pio test -e native` via a Bash/Git Bash shell, not PowerShell — PowerShell in this environment has produced false `ERRORED`/crash results (a MinGW runtime DLL/PATH mismatch, not a real test failure) on the same commits that pass cleanly through Bash.

## Architecture

This is a hexagonal-architecture embedded project. The critical rule: **domain and application code must compile and run under the `native` PlatformIO environment without `Arduino.h`**. Hardware-specific code is isolated behind ports (interfaces) and only implemented in adapters.

```
Hardware Layer   → Arduino GPIO, LocoNet bus, EEPROM, Serial
Adapters         → ArduinoDigitalInput/Output, MrrwaLocoNetAdapter, ArduinoClock, SerialLogger, EepromConfigurationRepository
Application      → PanelController, TurnoutCommandService, RouteExecutionService, InputPollingService, LayoutStateCoordinator
Domain           → Button, Indicator, Turnout, Route, Signal, Panel, domain events
```

- **Domain classes** depend only on abstractions (ports), never on `digitalRead`/`digitalWrite`/LocoNet library calls directly.
- **Ports** (e.g. `DigitalInput`, `DigitalOutput`, `Clock`, `TurnoutCommandPort`, `Logger`, `ConfigurationRepository`) are pure interfaces that the domain/application depend on.
- **Adapters** implement ports against real hardware (Arduino GPIO, MRRWA LocoNet, EEPROM). Test doubles (`FakeDigitalOutput`, `FakeClock`, etc.) implement the same ports for native unit tests.
- **`main.cpp` is the composition root only** — it defines pin assignments, constructs adapters/domain objects/application services, calls `begin()` in `setup()`, and calls non-blocking `update()`/`poll()` methods in `loop()`. It must not contain business logic.
- **No C++ standard library on the AVR target.** The `megaatmega2560` toolchain has no real libstdc++ — `std::string`, `std::map`, and `std::optional` do not compile there (confirmed; a third-party port, ArduinoSTL, was evaluated and rejected for missing `std::optional`/`map::insert_or_assign`). Domain code must avoid all STL containers/strings/optionals and use fixed-capacity alternatives instead:
  - `FixedString32` (`lib/McsCore/src/domain/FixedString32.h`) replaces `std::string` — a 32-char buffer, truncates safely if constructed from a longer literal.
  - `TurnoutCollection` and `RouteService` use fixed-size arrays (`Turnout[64]`, `Route[32]`) with linear scan instead of `std::map`, since capacity has to be bounded on an 8KB-RAM part. Adding past capacity is a no-op (existing entries and count are unaffected) — this is a behavior TurnoutCollection/RouteService never had with `std::map` and is covered by dedicated tests.
  - `Route` uses a fixed `TurnoutCommand[64]` array instead of `std::map<int, TurnoutPosition>`, and a `TurnoutPositionLookup{bool found; TurnoutPosition position;}` struct instead of `std::optional<TurnoutPosition>`.
  - This was caught only once `main.cpp` first included domain headers (Milestone 8) — before that, `main.cpp` was a placeholder blink sketch and never forced the AVR compiler to build the domain layer.

### Current source layout

The codebase is split into three PlatformIO libraries by target, not by a
preprocessor guard. PlatformIO compiles every `.cpp` file in a library that's
"used" by an environment, so which of the three environments (`native`,
`megaatmega2560`, `esp32dev`) compiles a given file is controlled entirely by
which library it lives in — `platformio.ini`'s per-environment `lib_deps`/
`lib_ignore` — not by `#ifdef`. The one guard idiom that remains is
`#ifdef ARDUINO`, on the 12 files that are genuine hardware shims (4 in
`McsCore`, 2 in `McsLoconet`, 6 in `McsEsp32`); every other file needs no
guard at all because it's structurally impossible for the wrong environment
to ever compile it.

- `lib/McsCore/src/` — portable domain/ports/application/adapter classes usable from any of the three environments
  - `domain/`: Button, FixedString32 (fixed 32-char buffer, replaces std::string for AVR compatibility), Indicator, Route, RouteService, Turnout, TurnoutCollection, TurnoutIndicator, TurnoutService
  - `ports/`: Clock, DigitalInput, DigitalOutput, TurnoutCommandPort
  - `application/`: TurnoutControl (wires buttons, turnout, indicator, and TurnoutCommandPort together)
  - `adapters/`: ArduinoClock, ArduinoDigitalInput, ArduinoDigitalOutput (`#ifdef ARDUINO`-guarded generic GPIO adapters), NullTurnoutCommandPort (no-op fallback), TurnoutStation (`#ifdef ARDUINO`-guarded, config-driven per-turnout composition helper — owns one turnout's full Button/Indicator/Turnout/TurnoutIndicator/TurnoutControl stack)
- `lib/McsLoconet/src/` — LocoNet-specific code, compiled for `native` (pure-logic pieces only) and `megaatmega2560`
  - `ports/`: LocoNetFeedbackSource, LocoNetSwitchDriver, LocoNetTransport
  - `adapters/`: LocoNetFeedbackDecoder, MrrwaLocoNetTurnoutAdapter (native-tested translation layers, no Arduino dependency); MrrwaLocoNetFeedbackSource, MrrwaLocoNetSwitchDriver (`#ifdef ARDUINO`-guarded send/receive hardware shims); PulsingLocoNetTransport (non-blocking solenoid pulse-timing logic)
- `lib/McsEsp32/src/` — ESP32-specific code, compiled for `native` (pure-logic pieces only) and `esp32dev`
  - `domain/`: CommandLineParser, NodeConfig, ParsedCommand, TopicScheme (JMRI/MQTT topic naming), PayloadCodec (turnout position ↔ MQTT payload encoding), MatrixScanner (cycles the 3 row outputs one at a time, caching each column's reading per row for the 3x4 button matrix), LedPairDriver (drives a shared-GPIO red/green LED pair: steady color, non-blocking blink between last-displayed color and its opposite, or — sub-project #2d-b — a faster, synchronized `setIdentifying(true)` override that takes priority over both while active and is idempotent so a caller can invoke it unconditionally every tick), IdentifyModeTimer (elapsed-time check, no `update()` needed: `trigger()` stamps the clock, `isActive()` compares against a fixed duration computed on demand; a second `trigger()` before expiry extends rather than stacks), BootMode (enum: Normal/NeedsCommissioning/WirelessSetup — transient boot-time state, not part of persisted `NodeConfig`), BootModeSelector (picks `BootMode` from `NodeConfig::validate()` plus a pending wireless-setup request, request always wins), MacAddress (6-byte value type, `lastFourHexDigits()`), SetupApName (builds a WiFi AP name, `"MaltBee-Setup-" + lastFourHexDigits()`, from a `MacAddress`), WebFormSubmission (plain data struct: wifi/broker/id/12 channel JMRI names, as submitted by the wireless-commissioning web form), SetupFormRenderer (pure HTML generation/escaping for that form, served by `CaptivePortalServer`; the WiFi-password field always renders blank — never the real stored password — and a blank submission keeps the current password, the opposite blank-semantics from turnout channel names, which genuinely clear on a blank submission), PresenceTopics (`panel/<nodeId>/status`, `panel/<nodeId>/mac`, and — sub-project #2d-b — `panel/<nodeId>/identify` topic naming, mirrors `TopicScheme`'s pattern), NodeIdentityGuard (pure one-way latch: `onMacObserved()` compares an incoming MAC against the panel's own; a mismatch means a second panel is claiming this `nodeId`, and the latch never clears once tripped), WifiScanFormatter (pure dedupe/sort/format layer for the wireless-setup form's network dropdown — `dedupeAndSort()` keeps the strongest signal per SSID and drops hidden (empty-SSID) entries, `withSignalBars()` appends a 1-4-bar Unicode indicator per SSID since a native `<select>` `<option>` can't carry an icon; the actual `WiFi.scanNetworks()` call lives in `CaptivePortalServer`, which is the only Arduino-dependent piece of this feature)
  - `ports/`: ConfigStore, UartPort, MqttTransport, SetupModeRequestStore (a one-shot "enter wireless setup on next boot" flag, separate from `ConfigStore` since it's boot-intent, not configuration)
  - `application/`: CommissioningSession (commissioning command handling, wires a `ConfigStore` port; gained a read-only `draft()` accessor in sub-project #2c-b1 so `WebFormCommissioningAdapter` can pre-fill the web form), MqttPresenceAnnouncer (wires a `MqttTransport` port to `PresenceTopics`; publishes retained "online" status + the panel's own MAC on the disconnected-to-connected edge only, re-arming after a disconnect/reconnect cycle)
  - `adapters/`: SerialCommissioningAdapter, JmriTurnoutCommandAdapter, JmriFeedbackSource, WebFormCommissioningAdapter (translates a `WebFormSubmission` into `CommissioningSession` commands — deliberately bypasses `CommandLineParser` for the `wifi`/`turnout N name X` commands by constructing `ParsedCommand` directly, since `CommandLineParser` has no quote-handling and those two commands' string values could contain spaces) (native-tested translation layers, no Arduino dependency); EspUartPort, NvsConfigStore, WiFiLink, MqttLink, NvsSetupModeRequestStore, EspDeviceIdentity (reads the real ESP32 MAC via `esp_efuse_mac_get_default()`), CaptivePortalServer (`#ifdef ARDUINO`-guarded hardware shims; opens a WiFi AP + `DNSServer` (all queries redirected to the AP's own IP) + `WebServer` serving `SetupFormRenderer`'s page; `begin(apName)` opens an **open** (no-passphrase) AP, wired in `src/esp32/main.cpp` — this was WPA2-protected under sub-project #2c-b2 but the passphrase was later removed (see sub-project #2c-d, `docs/superpowers/specs/2026-09-02-esp32-open-wireless-setup-ap-design.md`); `begin()` also runs one `WiFi.scanNetworks()` (safe to combine with the AP — the core's `scanNetworks()` internally enables station mode alongside it — but deliberately called last in `begin()`, after `dnsServer_.start()`/`webServer_.begin()`, since a degraded RF environment can block it for up to 10 seconds and starting DNS/HTTP first keeps a phone's captive-portal-detection probe from arriving before the server can answer it) and caches the deduped/sorted result via `WifiScanFormatter`, so every subsequent page load (including the OS's automatic captive-portal-detection probes, which `onNotFound` also routes to the form) is instant rather than re-scanning; a `/rescan` route re-runs the scan on demand; `NvsSetupModeRequestStore` persists its flag in its own NVS namespace `"mcs-boot"`, distinct from `NvsConfigStore`'s `"mcsnode"`, and its `requestOnNextBoot()`/`consumeRequest()` both return `bool` and check their own NVS calls, so a persistence failure doesn't get silently swallowed or misreported); MatrixDigitalInput (implements `DigitalInput` for one fixed matrix cell, forwarding to `MatrixScanner`'s cached reading); LedPairOutput (implements `DigitalOutput` for one logical side, green or red, of a shared LED pair, forwarding to `LedPairDriver`); LedPairStation (`#ifdef ARDUINO`-guarded, config-driven per-turnout composition helper — owns one turnout's real LED-pair GPIO plus its `LedPairDriver`/two `LedPairOutput`s, mirroring `TurnoutStation`; `setIdentifying(bool)` one-line-forwards to the driver, sub-project #2d-b); ToggleTurnoutStation (portable, no `#ifdef ARDUINO` guard — config-driven per-turnout composition helper for the ESP32's single-button panel, owning one turnout's `Button`/`Turnout`/`TurnoutIndicator`/`ToggleTurnoutControl` stack behind ports only, mirroring `TurnoutStation`/`LedPairStation` but without any raw-GPIO construction of its own); GatedDigitalInput (implements `DigitalInput` by wrapping another and forcing `isActive()` false while `setSuppressed(true)`, otherwise forwarding — general-purpose, no knowledge of its caller); ButtonSetupModeTrigger (detects the ESP32 board's own BOOT button (GPIO0) held for a minimum duration then released, edge-triggered like `Button::wasPressed()`; wired in `src/esp32/main.cpp` against a dedicated `bootButton` input — sub-project #2c-c replaced the earlier T1+T2 `ComboSetupModeTrigger` combo with this dedicated-button gesture, which needs no new wiring since the BOOT button is already present on the board, and eliminates the staggered-two-finger-press gap the combo had; a release only confirms — and only then measures `heldFor` against `minHoldMs` — once the input has read inactive continuously for a fixed 50ms settle window, so a bounce back to active mid-release cancels the pending release and the original press time stands, rather than risking `ESP.restart()` landing mid-bounce and leaving GPIO0 sampled low at reset (which would boot into UART download mode instead of the firmware))
- `src/mega/main.cpp` and `src/esp32/main.cpp` — the two composition roots. `src/mega/main.cpp` builds a `TurnoutConfig[4]` table and 4 `TurnoutStation`s from it (range-`for` over `stations[]` in `setup()`/`loop()`, no per-turnout duplication), plus the shared real LocoNet send/receive adapter chain (`MrrwaLocoNetSwitchDriver` → `PulsingLocoNetTransport` → `MrrwaLocoNetTurnoutAdapter` for sending; `MrrwaLocoNetFeedbackSource` → `LocoNetFeedbackDecoder` → broadcast `TurnoutStation::applyFeedback()` for receiving, each station's own `TurnoutControl` self-filtering by address). `src/esp32/main.cpp` builds a `TurnoutPanelConfig[12]` table and 12 `ToggleTurnoutStation`s from it, plus a shared `MatrixScanner` (3 row/4 column GPIOs) feeding 12 `MatrixDigitalInput`s (each wrapped in a `GatedDigitalInput` before being handed to its station, so a latched `nodeId` collision can suppress all 12 buttons' normal toggle command), 12 `LedPairStation`s (one real LED-pair GPIO each), the shared JMRI/MQTT command+feedback adapters (`JmriTurnoutCommandAdapter`/`JmriFeedbackSource` over a `WiFiLink`+`MqttLink` pair), bench-serial commissioning (`EspUartPort` → `SerialCommissioningAdapter` → `CommissioningSession` → `NvsConfigStore`), and (sub-project #2c-b2) wireless commissioning (`WebFormCommissioningAdapter` wrapping that same `CommissioningSession`, served by a `CaptivePortalServer`). Boot computes a `BootMode` via `BootModeSelector::select()` — from `NodeConfig::validate()` plus a `NvsSetupModeRequestStore::consumeRequest()` one-shot flag, read exactly once at global-init time. In `BootMode::WirelessSetup`, `setup()`/`loop()` open the captive portal (`EspDeviceIdentity::mac()` → `SetupApName::from()` builds the AP name; the AP itself is open, no passphrase, as of sub-project #2c-d) and skip the WiFi-station/MQTT/JMRI/matrix/trigger/station machinery entirely for that boot (the 12 `LedPairStation`s are the one exception — see sub-project #2c-c below) — every one of those objects still exists as an unconditionally-constructed global (no lazy/heap construction; only `begin()`/`poll()`/`update()` calls are gated, since C++ global constructors can't be skipped based on a runtime decision anyway). `BootMode::Normal` and `BootMode::NeedsCommissioning` share this same non-WirelessSetup code path, still distinguished only by the pre-existing `configValid` gate. In that path, a `ButtonSetupModeTrigger` reads the *raw* `bootButton` input (GPIO0, the ESP32 board's own BOOT button, independent of the 12-button matrix) every `loop()` tick and, on a 3-second hold-then-release, calls `NvsSetupModeRequestStore::requestOnNextBoot()` and `ESP.restart()` — but only if that call returned `true`; a persistence failure logs a message and leaves the panel running normally instead of rebooting into a mode it might immediately revert out of. Boot explicitly gates Wi-Fi/MQTT connection attempts on `NodeConfig::validate().empty()`; `loop()` reverts every station's LED to blink/unconfirmed via `clearIndicator()` whenever the config is invalid, MQTT is disconnected, **or a `nodeId` collision is latched** (sub-project #2d-a — see below). An `NvsBootstrap` global (the very first global constructed in the file) calls `nvs_flash_init()` before anything else touches NVS — ESP-IDF runs C++ global constructors before `app_main()`/`initArduino()`, so without this, `NvsConfigStore::load()` (and now `NvsSetupModeRequestStore::consumeRequest()`, declared right after it) in a later global's initializer would silently see an uninitialized NVS partition and always return factory-default/no-request state. A `const MacAddress ownMac = EspDeviceIdentity().mac();` global (sub-project #2d-a, declared right after `systemClock`) reads the chip's real MAC unconditionally at global scope — safe because the underlying eFuse read has no NVS/WiFi-driver dependency, unlike the two hazards above — and feeds both `SetupApName::from()` (in the `WirelessSetup` branch) and the two new presence/collision globals below. `NodeIdentityGuard identityGuard` and `MqttPresenceAnnouncer presenceAnnouncer` (declared right after `mqttLink`, since the announcer holds it by reference) add MQTT presence: `setup()` subscribes to `PresenceTopics::macTopic(nodeId)` with a lambda forwarding to `identityGuard.onMacObserved()`; `loop()` computes `collision` once per tick from `identityGuard.collisionDetected()`, logs it once via a `static` one-shot guard, suppresses all 12 `gatedButtons` uniformly, driven by `collision` alone (no combo-hold special-casing remains now that the setup gesture uses the independent `bootButton` rather than two of the 12 turnout buttons), and folds `!collision` into the same `clearIndicator()` gate as `configValid`/MQTT-connected. **Sub-project #2c-c** also adds LED feedback for wireless setup mode itself: the `BootMode::WirelessSetup` branch of `loop()` calls `ledStation.setIdentifying(true)` and `ledStation.update()` on all 12 `LedPairStation`s every tick, reusing the #2d-b identify-blink override so all 12 LED pairs flash for as long as the setup AP is open — the two states can never overlap (wireless setup mode never starts MQTT, so the MQTT-triggered identify-blink can't fire at the same time), so sharing the exact visual is unambiguous in practice. `presenceAnnouncer.update(mqttLink.connected())` runs unconditionally every tick — this is load-bearing, not just safe: a collided panel must keep re-publishing its own MAC (retained) on every reconnect for the *other* colliding panel to ever observe the mismatch, since MQTT's broker-enforced duplicate-`clientId` disconnect means the two panels are never connected at the same time. **Sub-project #2d-b** adds identify-blink: `setup()` also subscribes to `PresenceTopics::identifyTopic(nodeId)` with a lambda that ignores the payload and calls `identifyTimer.trigger()` (an `IdentifyModeTimer` global, declared after `presenceAnnouncer`); `loop()` computes `identifying` once per tick from `identifyTimer.isActive()` and calls `ledStation.setIdentifying(identifying)` on all 12 stations in a loop that runs — and must keep running — *before* the existing `ledStation.update()` loop, so a fresh activation takes visible effect the same tick it arrives; merging the two loops would silently reintroduce a one-tick activation lag with no test to catch it. Identify has no button/feedback suppression (a deliberate contrast with the collision handling above — it's a pure visual overlay) and deliberately still works during a latched collision, since that is the intended way to tell two colliding panels apart.
- `test/test_<name>/test_main.cpp` — Catch2 test binaries (41 test suites)
- `test/support/` — test doubles (FakeDigitalInput, FakeClock, FakeDigitalOutput, FakeTurnoutCommandPort, FakeLocoNetTransport, FakeLocoNetSwitchDriver, FakeSetupModeRequestStore)
- `jmri/panel_mqtt_turnout_bridge.py` — JMRI-side Jython startup script, outside the PlatformIO/C++ codebase entirely, that completes the ESP32 panel's command/feedback loop. It bridges the MQTT topics `JmriTurnoutCommandAdapter`/`JmriFeedbackSource` speak (`track/turnout/<jmriSystemName>` command, `track/turnout/<jmriSystemName>/state` state, `THROWN`/`CLOSED` payloads — see `TopicScheme`/`PayloadCodec`) directly to JMRI's real `Turnout` objects, with no shadow "MT" turnouts or Logix. It discovers which turnouts to bridge dynamically (every registered `LT`-prefixed turnout at script-run time), applies an incoming command via `setCommandedState()` and always publishes the resulting state back (even a no-op command, since that's exactly when a panel is out of sync and most needs telling), and separately republishes every discovered turnout's `KnownState` on any change regardless of cause (this panel, another panel, PanelPro, a dispatcher, physical feedback) via a `PropertyChangeListener` — this is what makes "LEDs reflect changes made elsewhere" work end to end. State publishes are **not retained**, so a panel that reconnects mid-session stays in blink/unconfirmed state for a turnout until that turnout's state next actually changes. See `docs/ESP32_Turnout_Panel_Implementation.md`'s "JMRI-side bridge script" section for installation steps.

### Wireless setup: fully wired (sub-project #2c-b2)

Sub-project #2c-b2 wired every class from #2c-a (`BootMode`,
`BootModeSelector`, `SetupModeRequestStore`/`NvsSetupModeRequestStore`,
`GatedDigitalInput`, `ComboSetupModeTrigger` (since replaced — see below))
and #2c-b1 (`MacAddress`,
`SetupApName`, `EspDeviceIdentity`, `WebFormSubmission`,
`WebFormCommissioningAdapter`, `SetupFormRenderer`, `CaptivePortalServer`)
into `src/esp32/main.cpp` — see the `src/esp32/main.cpp` bullet above for
how. It also resolved both open design gaps #2c-b1's final review had
deferred here:
- The web form's blank-channel copy and behavior now agree — a blank
  channel field genuinely clears that channel's stored name
  (`WebFormCommissioningAdapter::submit()` no longer skips it).
- `CaptivePortalServer`'s AP required a WPA2 passphrase to join at the time
  (later removed — see sub-project #2c-d below), and the web
  form never renders the real stored WiFi password back into its HTML — a
  blank password field on submission keeps the current password instead of
  clearing it (the opposite blank-semantics from channel names, deliberately).

The staggered-press gap #2c-b2 had explicitly accepted (a human's
two-finger T1+T2 press could fire one ordinary toggle command on
whichever button was pressed first, before the combo's suppression
engaged) no longer exists: sub-project #2c-c
(`docs/superpowers/specs/2026-09-01-esp32-boot-button-setup-trigger-design.md`)
replaced the T1+T2 combo with a single dedicated-button gesture on the
ESP32's own BOOT button (GPIO0), which has no analogous two-input race
since there's only one input to hold.

### Presence + collision detection (sub-project #2d-a)

Each panel publishes two **retained** MQTT topics on every successful
connect (never on every tick — see `MqttPresenceAnnouncer` above):
`panel/<nodeId>/status` = `"online"` (the existing LWT already publishes
`"offline"` there on disconnect — this closes the asymmetry the #7b spec
left as a placeholder) and `panel/<nodeId>/mac` = the panel's own
last-4-hex-digit MAC. Each panel also subscribes to its own `mac` topic;
observing a value that isn't its own means a second physical panel is
claiming the same `nodeId` (an operator commissioning error). Detection
suppresses all 12 turnout buttons, stops applying turnout feedback (LEDs
fall back to the existing blink/unconfirmed state via `clearIndicator()`
— no new LED pattern was built), and logs once via serial.

**The `retained=true` flag on the mac topic is load-bearing, not
incidental — do not "simplify" it to a plain publish.** MQTT's
broker-enforced duplicate-`clientId` behavior means two colliding panels
are never connected at the same time (each new connection kicks the
other off); without retention, neither panel would ever be present on the
topic when the other one is there to see it, and detection would never
fire at all. With retention, whichever panel is *not* currently connected
still has its last claim sitting on the broker for the other to read on
its own next connect — this is what makes mutual detection work at all.

**Known, accepted limitation:** the retained mac topic is never cleared
or aged out. If a panel is decommissioned or a `nodeId` is legitimately
reassigned to a different physical panel (normal fleet management, not
an error), the replacement panel's first boot will see the old panel's
stale retained MAC and latch a false-positive collision — but since
`presenceAnnouncer.update()` runs unconditionally, that same boot also
overwrites the stale claim with the new panel's own MAC, so the false
positive costs exactly one boot and clears on the next reboot without
any operator intervention. Serial commissioning and the BOOT-button wireless-setup gesture (wired to
the *raw* `bootButton` input, independent of the gated turnout matrix —
see the `ButtonSetupModeTrigger` entry above) both keep working during a
collision lockout, so recovery never requires broker administration.

### Identify-blink (sub-project #2d-b)

Publishing any message (payload ignored) to `panel/<nodeId>/identify`
makes that one panel's 12 LED pairs flash green/red in unison at a fast,
fixed 150ms interval for 10 seconds (`IDENTIFY_DURATION_MS` in
`main.cpp`), then automatically revert — no explicit "stop" command
exists; re-publishing before the window expires extends it rather than
stacking a second one. MQTT-only trigger, deliberately no bench-serial
equivalent: a technician trying to identify which physical panel is a
given `nodeId` is by definition not already standing at the right panel
with a USB cable. Buttons and turnout feedback stay fully live during
identify — it's a pure visual overlay, unlike the collision suppression
above — and it deliberately still works during a latched collision, since
that's the intended way to tell two colliding panels apart. Publish this
topic **non-retained**: a retained message would re-trigger a fresh
10-second flash on every MQTT reconnect, since `MqttLink::connect()`
replays every subscribed topic's last retained value.

The 150ms flash rate is `LedPairDriver`'s own `kIdentifyIntervalMs`
constant (distinct from the per-instance `blinkIntervalMs_` used for the
existing 500ms "unconfirmed" blink) — a `setIdentifying(bool)` override,
idempotent so `main.cpp` can call it unconditionally every tick, takes
priority over the existing blink/steady logic while active and restores
whatever that logic would be showing (not a guess) once deactivated.
**Accepted, not defended against:** if real turnout feedback arrives
mid-identify, the existing feedback-driven GPIO write can desynchronize
one LED pair from the rest of the panel's flash for up to one 150ms tick
before self-correcting.

**Include convention:** within a library, includes stay relative
(`../ports/X.h`). A file depending on a header from a *different* library it
needs uses a rooted include (`"ports/X.h"`) instead — 35 such includes exist
today (reconciled via a full `grep` recount as of sub-project #2d-b, which
added one more via `IdentifyModeTimer.h`'s `#include "ports/Clock.h"`; the
previous "19" figure had drifted uncorrected across sub-projects #7a and
#7b, which each added rooted-including files — `ToggleTurnoutControl.h`,
`ToggleTurnoutStation.h` — without this paragraph's tally being updated),
split between `McsEsp32` (30) and `McsLoconet` (5), all of them referencing
`McsCore` headers: `adapters/ArduinoDigitalOutput.h`, `domain/Turnout.h`,
`domain/Button.h`, `domain/Indicator.h`, `domain/TurnoutIndicator.h`,
`ports/Clock.h`, `ports/DigitalInput.h`, `ports/DigitalOutput.h`,
`ports/TurnoutCommandPort.h`. `LedPairStation.h`'s
`adapters/ArduinoDigitalOutput.h` include remains the only rooted include of
an *adapter* header (every other one is `domain/` or `ports/`) — still safe
under the basename-uniqueness rule below, since no other library has a file
named `ArduinoDigitalOutput.h` under `adapters/`.

**Trap to watch for:** basenames must stay globally unique across all three
libraries. All three have parallel `domain/`/`ports/`/`adapters/` subtrees,
and `native`'s build has all three library roots on the include path
simultaneously (see `lib_deps` in `platformio.ini`'s `[env:native]`) — a
rooted include like `#include "ports/Clock.h"` only resolves correctly today
because no other library happens to have a file named `Clock.h` under
`ports/`. If a future file collides with an existing basename in a different
library, resolution becomes include-path-order-dependent and the failure
mode is confusing rather than a clean error.

**Test coverage (all Catch2, all passing):**
- ✅ Button (debouncing, edge detection)
- ✅ Indicator (on/off control)
- ✅ FixedString32 (construction, truncation, equality, default/empty state)
- ✅ Turnout (position, address, locking, disable)
- ✅ TurnoutCollection (registry, lookup, capacity limit)
- ✅ TurnoutService (coordination)
- ✅ TurnoutIndicator (display/clear reflecting thrown/closed position)
- ✅ TurnoutControl (button-edge commands, feedback applying to turnout/indicator)
- ✅ NullTurnoutCommandPort (no-op send is safely callable)
- ✅ Route (command sequences, capacity limit)
- ✅ RouteService (execution, capacity limit)
- ✅ MrrwaLocoNetTurnoutAdapter (command → LocoNet packet translation)
- ✅ PulsingLocoNetTransport (non-blocking solenoid pulse timing)
- ✅ LocoNetFeedbackDecoder (switch-outputs report → TurnoutFeedback translation)
- ✅ CommandLineParser (command-line tokenizing/parsing)
- ✅ CommissioningSession (commissioning command handling)
- ✅ NodeConfig (node configuration construction/validation)
- ✅ SerialCommissioningAdapter (serial line framing → ParsedCommand translation)
- ✅ TopicScheme (channel/JMRI-name → MQTT topic naming)
- ✅ PayloadCodec (turnout position ↔ MQTT payload encoding)
- ✅ JmriTurnoutCommandAdapter (channel → JMRI topic/payload publish translation)
- ✅ JmriFeedbackSource (MQTT payload → TurnoutFeedback translation)
- ✅ JMRI command/feedback wiring integration (end-to-end self-echo immunity + real-feedback proof)
- ✅ MatrixScanner (row-cycling scan, per-row column caching, default-false before first scan)
- ✅ MatrixDigitalInput (forwards to the scanner's cached reading for its own fixed cell)
- ✅ LedPairDriver (steady green/red, blink between last-displayed color and its opposite, `begin()` re-stamping the timer and re-writing the current color)
- ✅ LedPairOutput (forwards one logical side's `set()` to the shared `LedPairDriver`)
- ✅ ToggleTurnoutStation (begin() clears the indicator; a debounced press sends the opposite of the current position; applyFeedback matching/non-matching address; clearIndicator turns both outputs off even after a real applyFeedback lit one)
- ✅ PresenceTopics (status/mac topic string construction for a given node id)
- ✅ NodeIdentityGuard (no collision before any observation; own-mac echoes never false-positive; a foreign mac latches; the latch never clears once tripped)
- ✅ MqttPresenceAnnouncer (publishes retained online status + own mac on the connect edge only; no republish while already connected; re-arms after a disconnect/reconnect cycle)
- ✅ IdentifyModeTimer (inactive before any trigger; active immediately after; expires after the fixed duration; a second trigger before expiry extends the window rather than stacking a separate one)
- ✅ LedPairDriver's identify override (activating shows a color immediately at its own fixed interval, independent of the blink interval; idempotent — a redundant activate call doesn't reset the toggle timer; deactivating restores whatever the pre-existing blink/steady state would show, not a guess)

**Completed milestones:** 1-11 (foundation, ports, Button, Indicator, Turnout domain model, TurnoutIndicator, TurnoutControl, hardware integration programming, LocoNet output, LocoNet feedback, multiple turnouts). `pio run -e megaatmega2560` compiles successfully (RAM 9.9%, Flash 2.7% with 4 stations). Milestones 9-11 are complete on the programming side only — physical wiring and on-hardware verification (button-to-LED and LocoNet send/receive against a real DR5000/DR4018, across all 4 stations) are still outstanding — see the "Milestone 8 hardware" note below.

**Next milestones:** routes (12), persistent configuration (13)

### Milestone 8 hardware (not yet done)

The programming portion of Milestone 8 is complete and builds cleanly for the Mega 2560, but the hands-on portion still needs a physically connected board: `src/mega/main.cpp` now defines 4 stations (`STATION_CONFIGS[0..3]`) — pins 22/23/8/9 (throw/close/thrown-LED/closed-LED) for station 1, 24/25/10/11 for station 2, 26/27/12/13 for station 3, 28/29/14/15 for station 4. Wire at least one (ideally all 4, since Milestone 11's completion criteria calls for verifying 4), flash with `pio run -e megaatmega2560 --target upload`, and verify.

`src/mega/main.cpp` now wires the real LocoNet send and receive adapters (Milestones 9-10), not `NullTurnoutCommandPort`, so a button press does reach `LocoNet.requestSwitch()` and `TurnoutControl::applyFeedback()` is driven by real incoming `OPC_SW_REP` messages — but none of that has been exercised against real LocoNet hardware yet. "Verify behavior on hardware" for Milestone 8 realistically still means confirming button reads and LED drive work in isolation; full button-to-LED-via-LocoNet verification is Milestones 9-11's remaining hardware work, not Milestone 8's.

## Engineering Principles (from the roadmap)

- **TDD**: write a failing native test first, implement the minimum to pass, refactor only while green.
- **Dependency inversion**: domain depends on ports, adapters depend on domain-owned interfaces — never the reverse.
- **Single responsibility**: e.g. a `Button` only reads button state, a `Turnout` only models turnout position — it must not own LEDs or call LocoNet directly.
- **Explicit state**: state changes go through methods that enforce valid transitions, not direct field mutation.
- **No blocking calls** (`delay()`) in domain/application code — `loop()` must stay non-blocking via `update()`/`poll()`.
