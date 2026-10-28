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
"used" by an environment, so which of the four environments (`native`,
`megaatmega2560`, `esp32dev`, `esp32node4`) compiles a given file is
controlled entirely by
which library it lives in — `platformio.ini`'s per-environment `lib_deps`/
`lib_ignore` — not by `#ifdef`. The one guard idiom that remains is
`#ifdef ARDUINO`, on the 12 files that are genuine hardware shims (4 in
`McsCore`, 2 in `McsLoconet`, 6 in `McsEsp32`); every other file needs no
guard at all because it's structurally impossible for the wrong environment
to ever compile it.

- `lib/McsCore/src/` — portable domain/ports/application/adapter classes usable from any of the four environments
  - `domain/`: Button, FixedString32 (fixed 32-char buffer, replaces std::string for AVR compatibility), Indicator, Route, RouteService, Turnout, TurnoutCollection, TurnoutIndicator, TurnoutService, MegaStationConfig/MegaConfig (Milestone 13 — per-station address/name struct and the 4-station config it's collected into, plus `MegaConfig::validate()`: every address in range and no two stations sharing an address)
  - `ports/`: Clock, DigitalInput, DigitalOutput, TurnoutCommandPort, ConfigurationRepository (Milestone 13 — `load()`/`save(config)`, mirroring the ESP32 side's own `ConfigStore` API shape)
  - `application/`: TurnoutControl (wires buttons, turnout, indicator, and TurnoutCommandPort together), RouteExecutionService (Milestone 12 — executes a `Route`'s turnout commands one at a time directly through `TurnoutCommandPort`, spaced over time via its own `update()` since `PulsingLocoNetTransport` can only track one pending pulse at a time; built fresh rather than reusing the ported `RouteService`, which operates against a separate, unwired `TurnoutService`/`TurnoutCollection` registry and never reaches real hardware), MegaCommissioningSession (Milestone 13 — minimal hand-parsed serial commissioning for the Mega's `address`/`name`/`save` commands against an in-memory draft; see the "Milestone 13: Persistent configuration (Mega panel)" section below)
  - `adapters/`: ArduinoClock, ArduinoDigitalInput, ArduinoDigitalOutput (`#ifdef ARDUINO`-guarded generic GPIO adapters), NullTurnoutCommandPort (no-op fallback), TurnoutStation (`#ifdef ARDUINO`-guarded, config-driven per-turnout composition helper — owns one turnout's full Button/Indicator/Turnout/TurnoutIndicator/TurnoutControl stack), InMemoryConfigurationRepository (Milestone 13 — a real, reusable `ConfigurationRepository` backed by a plain in-memory `MegaConfig`, not merely a test double), EepromConfigurationRepository (Milestone 13 — `#ifdef ARDUINO`-guarded hardware adapter, same precedent as `TurnoutStation`/`MrrwaLocoNetSwitchDriver`; persists a 139-byte magic+version+`MegaConfig` schema to the ATmega2560's EEPROM)
- `lib/McsLoconet/src/` — LocoNet-specific code, compiled for `native` (pure-logic pieces only) and `megaatmega2560`
  - `ports/`: LocoNetFeedbackSource, LocoNetSwitchDriver, LocoNetTransport
  - `adapters/`: LocoNetFeedbackDecoder, MrrwaLocoNetTurnoutAdapter (native-tested translation layers, no Arduino dependency); MrrwaLocoNetFeedbackSource, MrrwaLocoNetSwitchDriver (`#ifdef ARDUINO`-guarded send/receive hardware shims); PulsingLocoNetTransport (non-blocking solenoid pulse-timing logic)
- `lib/McsEsp32/src/` — ESP32-specific code, compiled for `native` (pure-logic pieces only) and `esp32dev`
  - `domain/`: CommandLineParser, NodeConfig, ParsedCommand, TopicScheme (Loco2MQTT topic naming — `loconet/turnout/<address>/set` command, `.../state` feedback), PayloadCodec (turnout position ↔ MQTT payload encoding), MatrixScanner (cycles the 3 row outputs one at a time, caching each column's reading per row for the 3x4 button matrix), LedPairDriver (drives a shared-GPIO red/green LED pair: steady color, non-blocking blink between last-displayed color and its opposite, or — sub-project #2d-b — a faster, synchronized `setIdentifying(true)` override that takes priority over both while active and is idempotent so a caller can invoke it unconditionally every tick), IndependentLedPairDriver (sub-project #2 — the node board's equivalent for two independent real GPIOs instead of one shared pin; deliberately a separate class carrying its own full copy of `LedPairDriver`'s Mode/blink/identify state machine rather than sharing it, reusing only the `LedPairColor` enum via an include; `writeColor()` always drives both GPIOs together so exactly one is ever lit, reproducing `LedPairDriver`'s exact blink visual on hardware that could otherwise show off or amber; **any state-machine bugfix to one must be mirrored to the other** — see the "Independent-output turnout indicator" section below), IdentifyModeTimer (elapsed-time check, no `update()` needed: `trigger()` stamps the clock, `isActive()` compares against a fixed duration computed on demand; a second `trigger()` before expiry extends rather than stacks), BootMode (enum: Normal/NeedsCommissioning/WirelessSetup — transient boot-time state, not part of persisted `NodeConfig`), BootModeSelector (picks `BootMode` from `NodeConfig::validate()` plus a pending wireless-setup request, request always wins), MacAddress (6-byte value type, `lastFourHexDigits()`), SetupApName (builds a WiFi AP name, `"MaltBee-Setup-" + lastFourHexDigits()`, from a `MacAddress`), WebFormSubmission (plain data struct: wifi/broker/id/12 channel turnout addresses, as submitted by the wireless-commissioning web form), SetupFormRenderer (pure HTML generation/escaping for that form, served by `CaptivePortalServer`; the WiFi-password field always renders blank — never the real stored password — and a blank submission keeps the current password, the opposite blank-semantics from turnout channel addresses, which genuinely clear on a blank submission), PresenceTopics (`panel/<nodeId>/status`, `panel/<nodeId>/mac`, and — sub-project #2d-b — `panel/<nodeId>/identify` topic naming, mirrors `TopicScheme`'s pattern), FirmwareVersion (a manually-bumped `kFirmwareVersion` constant, sub-project #9a, shown on the commissioning web page so a fleet of panels can be checked for consistent firmware), NodeIdentityGuard (pure one-way latch: `onMacObserved()` compares an incoming MAC against the panel's own; a mismatch means a second panel is claiming this `nodeId`, and the latch never clears once tripped), WifiScanFormatter (pure dedupe/sort/format layer for the wireless-setup form's network dropdown — `dedupeAndSort()` keeps the strongest signal per SSID and drops hidden (empty-SSID) entries, `withSignalBars()` appends a 1-4-bar Unicode indicator per SSID since a native `<select>` `<option>` can't carry an icon; the actual `WiFi.scanNetworks()` call lives in `CaptivePortalServer`, which is the only Arduino-dependent piece of this feature), StatusLedDriver (sub-project #10 — generic single-GPIO LED driver with four states: Off/On/SlowBlink/FastBlink, drives the node board's network status LED)
  - `ports/`: ConfigStore, UartPort, MqttTransport, SetupModeRequestStore (a one-shot "enter wireless setup on next boot" flag, separate from `ConfigStore` since it's boot-intent, not configuration), MdnsResolver (sub-project #9a — resolves an mDNS hostname to an IP; implemented by `EspMdnsResolver` on hardware, backs `BrokerAddressResolver`'s mDNS-first broker discovery), BootModeIndicator (`show(mode)` once when boot mode is decided, `update()` every tick — implemented here by `LedPairFlashIndicator`)
  - `application/`: CommissioningSession (commissioning command handling, wires a `ConfigStore` port; gained a read-only `draft()` accessor in sub-project #2c-b1 so `WebFormCommissioningAdapter` can pre-fill the web form), MqttPresenceAnnouncer (wires a `MqttTransport`+`Clock` pair to `PresenceTopics`; publishes "online" status + the panel's own MAC immediately on the disconnected-to-connected edge and every 30s thereafter while connected — sub-project #9b's heartbeat redesign, not retained), BrokerAddressResolver (sub-project #9a — mDNS-first, manual-fallback broker host resolution: tries `MdnsResolver` for `loco2mqtt.local`, falls back to the configured `brokerHost` if resolution fails)
  - `adapters/`: SerialCommissioningAdapter, Loco2MqttTurnoutCommandAdapter, Loco2MqttFeedbackSource (renamed from `JmriTurnoutCommandAdapter`/`JmriFeedbackSource` in sub-project #9a, re-keyed by LocoNet turnout address instead of JMRI system name; templated for N=4 alongside N=12 in sub-project #6, `Basic<X><N>` with `...4` aliases, mirroring sub-project #1's pattern for the two other `.cpp`-based classes), WebFormCommissioningAdapter (translates a `WebFormSubmission` into `CommissioningSession` commands — deliberately bypasses `CommandLineParser` only for the `wifi` command by constructing `ParsedCommand` directly, since `CommandLineParser` has no quote-handling and `wifi`'s string values could contain spaces; sub-project #9a removed the analogous bypass for the turnout-address command, since a LocoNet address is a bare integer with no spaces concern, so it now routes through `CommandLineParser` like every other field) (native-tested translation layers, no Arduino dependency); EspUartPort, NvsConfigStore, WiFiLink, MqttLink, NvsSetupModeRequestStore, EspMdnsResolver (sub-project #9a — `#ifdef ARDUINO`-guarded hardware adapter implementing `MdnsResolver`: waits up to 8s for WiFi association, then queries `loco2mqtt.local` via `ESPmDNS`, one-shot at boot), EspDeviceIdentity (reads the real ESP32 MAC via `esp_efuse_mac_get_default()`), CaptivePortalServer (`#ifdef ARDUINO`-guarded hardware shims; opens a WiFi AP + `DNSServer` (all queries redirected to the AP's own IP) + `WebServer` serving `SetupFormRenderer`'s page; `begin(apName)` opens an **open** (no-passphrase) AP, wired in `src/esp32/main.cpp` — this was WPA2-protected under sub-project #2c-b2 but the passphrase was later removed (see sub-project #2c-d, `docs/superpowers/specs/2026-09-02-esp32-open-wireless-setup-ap-design.md`); `begin()` also runs one `WiFi.scanNetworks()` (safe to combine with the AP — the core's `scanNetworks()` internally enables station mode alongside it — but deliberately called last in `begin()`, after `dnsServer_.start()`/`webServer_.begin()`, since a degraded RF environment can block it for up to 10 seconds and starting DNS/HTTP first keeps a phone's captive-portal-detection probe from arriving before the server can answer it) and caches the deduped/sorted result via `WifiScanFormatter`, so every subsequent page load (including the OS's automatic captive-portal-detection probes, which `onNotFound` also routes to the form) is instant rather than re-scanning; a `/rescan` route re-runs the scan on demand; `NvsSetupModeRequestStore` persists its flag in its own NVS namespace `"mcs-boot"`, distinct from `NvsConfigStore`'s `"mcsnode"`, and its `requestOnNextBoot()`/`consumeRequest()` both return `bool` and check their own NVS calls, so a persistence failure doesn't get silently swallowed or misreported); MatrixDigitalInput (implements `DigitalInput` for one fixed matrix cell, forwarding to `MatrixScanner`'s cached reading); LedPairOutput (implements `DigitalOutput` for one logical side, green or red, of a shared LED pair, forwarding to `LedPairDriver`); IndependentLedPairOutput (sub-project #2 — mirrors `LedPairOutput` exactly, forwarding to an `IndependentLedPairDriver` instead; constructed by `IndependentLedPairStation` as of sub-project #4); IndependentLedPairStation (sub-project #4 — `#ifdef ARDUINO`-guarded, config-driven per-turnout composition helper mirroring `LedPairStation` but owning **two** real GPIOs, red and green, instead of one shared pin, plus its `IndependentLedPairDriver` and two `IndependentLedPairOutput`s; zero logic of its own, so no native test, same precedent as `LedPairStation`; copy construction/assignment are `= delete`d because the class is self-referential — see the "Turnout station wiring (sub-project #4)" section below); IndependentLedPairFlashIndicator (sub-project #5 — header-only `BootModeIndicator` adapter mirroring `LedPairFlashIndicator<N>`, templated against `IndependentLedPairDriver*` instead of `LedPairDriver*`; see the "Boot-mode LED indicator (sub-project #5)" section below); LedPairStation (`#ifdef ARDUINO`-guarded, config-driven per-turnout composition helper — owns one turnout's real LED-pair GPIO plus its `LedPairDriver`/two `LedPairOutput`s, mirroring `TurnoutStation`; `setIdentifying(bool)` one-line-forwards to the driver, sub-project #2d-b); ToggleTurnoutStation (portable, no `#ifdef ARDUINO` guard — config-driven per-turnout composition helper for the ESP32's single-button panel, owning one turnout's `Button`/`Turnout`/`TurnoutIndicator`/`ToggleTurnoutControl` stack behind ports only, mirroring `TurnoutStation`/`LedPairStation` but without any raw-GPIO construction of its own); GatedDigitalInput (implements `DigitalInput` by wrapping another and forcing `isActive()` false while `setSuppressed(true)`, otherwise forwarding — general-purpose, no knowledge of its caller); EitherActiveDigitalInput (sub-project #3 — implements `DigitalInput` by composing exactly two other `DigitalInput&`s with OR semantics, so one `ButtonSetupModeTrigger` can be driven by either of two physical buttons; deliberately two, not variadic — see the "Composite setup-mode trigger" section below); ButtonSetupModeTrigger (detects the ESP32 board's own BOOT button (GPIO0) held for a minimum duration then released, edge-triggered like `Button::wasPressed()`; wired in `src/esp32/main.cpp` against a dedicated `bootButton` input — sub-project #2c-c replaced the earlier T1+T2 `ComboSetupModeTrigger` combo with this dedicated-button gesture, which needs no new wiring since the BOOT button is already present on the board, and eliminates the staggered-two-finger-press gap the combo had; a release only confirms — and only then measures `heldFor` against `minHoldMs` — once the input has read inactive continuously for a fixed 50ms settle window, so a bounce back to active mid-release cancels the pending release and the original press time stands, rather than risking `ESP.restart()` landing mid-bounce and leaving GPIO0 sampled low at reset (which would boot into UART download mode instead of the firmware)), ActivityTrackingTurnoutCommandPort (sub-project #10 — decorates a TurnoutCommandPort, forwarding send() while triggering an injected IdentifyModeTimer, used to flash the node board's activity LED on outbound Loco2MQTT traffic)
- `src/mega/main.cpp`, `src/esp32/main.cpp` and `src/esp32node4/main.cpp` — the three composition roots. `src/mega/main.cpp` builds a `TurnoutConfig[4]` table and 4 `TurnoutStation`s from it (range-`for` over `stations[]` in `setup()`/`loop()`, no per-turnout duplication), plus the shared real LocoNet send/receive adapter chain (`MrrwaLocoNetSwitchDriver` → `PulsingLocoNetTransport` → `MrrwaLocoNetTurnoutAdapter` for sending; `MrrwaLocoNetFeedbackSource` → `LocoNetFeedbackDecoder` → broadcast `TurnoutStation::applyFeedback()` for receiving, each station's own `TurnoutControl` self-filtering by address). `src/esp32/main.cpp` builds a `TurnoutPanelConfig[12]` table and 12 `ToggleTurnoutStation`s from it, plus a shared `MatrixScanner` (3 row/4 column GPIOs) feeding 12 `MatrixDigitalInput`s (each wrapped in a `GatedDigitalInput` before being handed to its station, so a latched `nodeId` collision can suppress all 12 buttons' normal toggle command), 12 `LedPairStation`s (one real LED-pair GPIO each), the shared Loco2MQTT command+feedback adapters (`Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource`, re-keyed by LocoNet turnout address, over a `WiFiLink`+`MqttLink` pair) with mDNS-first broker discovery (sub-project #9a — `EspMdnsResolver`/`BrokerAddressResolver` resolve `loco2mqtt.local` once at boot, falling back to the configured `brokerHost` on failure, before `mqttLink.begin()`), bench-serial commissioning (`EspUartPort` → `SerialCommissioningAdapter` → `CommissioningSession` → `NvsConfigStore`), and (sub-project #2c-b2) wireless commissioning (`WebFormCommissioningAdapter` wrapping that same `CommissioningSession`, served by a `CaptivePortalServer`). Boot computes a `BootMode` via `BootModeSelector::select()` — from `NodeConfig::validate()` plus a `NvsSetupModeRequestStore::consumeRequest()` one-shot flag, read exactly once at global-init time. In `BootMode::WirelessSetup`, `setup()`/`loop()` open the captive portal (`EspDeviceIdentity::mac()` → `SetupApName::from()` builds the AP name; the AP itself is open, no passphrase, as of sub-project #2c-d) and skip the WiFi-station/MQTT/Loco2MQTT/matrix/trigger/station machinery entirely for that boot (the 12 `LedPairStation`s are the one exception — see sub-project #2c-c below) — every one of those objects still exists as an unconditionally-constructed global (no lazy/heap construction; only `begin()`/`poll()`/`update()` calls are gated, since C++ global constructors can't be skipped based on a runtime decision anyway). `BootMode::Normal` and `BootMode::NeedsCommissioning` share this same non-WirelessSetup code path, still distinguished only by the pre-existing `configValid` gate. In that path, a `ButtonSetupModeTrigger` reads the *raw* `bootButton` input (GPIO0, the ESP32 board's own BOOT button, independent of the 12-button matrix) every `loop()` tick and, on a 3-second hold-then-release, calls `NvsSetupModeRequestStore::requestOnNextBoot()` and `ESP.restart()` — but only if that call returned `true`; a persistence failure logs a message and leaves the panel running normally instead of rebooting into a mode it might immediately revert out of. Boot explicitly gates Wi-Fi/MQTT connection attempts on `NodeConfig::validate().empty()`; `loop()` reverts every station's LED to blink/unconfirmed via `clearIndicator()` whenever the config is invalid, MQTT is disconnected, **or a `nodeId` collision is latched** (sub-project #2d-a — see below). An `NvsBootstrap` global (the very first global constructed in the file) calls `nvs_flash_init()` before anything else touches NVS — ESP-IDF runs C++ global constructors before `app_main()`/`initArduino()`, so without this, `NvsConfigStore::load()` (and now `NvsSetupModeRequestStore::consumeRequest()`, declared right after it) in a later global's initializer would silently see an uninitialized NVS partition and always return factory-default/no-request state. A `const MacAddress ownMac = EspDeviceIdentity().mac();` global (sub-project #2d-a, declared right after `systemClock`) reads the chip's real MAC unconditionally at global scope — safe because the underlying eFuse read has no NVS/WiFi-driver dependency, unlike the two hazards above — and feeds both `SetupApName::from()` (in the `WirelessSetup` branch) and the two new presence/collision globals below. `NodeIdentityGuard identityGuard` and `MqttPresenceAnnouncer presenceAnnouncer` (declared right after `mqttLink`, since the announcer holds it by reference) add MQTT presence: `setup()` subscribes to `PresenceTopics::macTopic(nodeId)` with a lambda forwarding to `identityGuard.onMacObserved()`; `loop()` computes `collision` once per tick from `identityGuard.collisionDetected()`, logs it once via a `static` one-shot guard, suppresses all 12 `gatedButtons` uniformly, driven by `collision` alone (no combo-hold special-casing remains now that the setup gesture uses the independent `bootButton` rather than two of the 12 turnout buttons), and folds `!collision` into the same `clearIndicator()` gate as `configValid`/MQTT-connected. **Sub-project #2c-c** also adds LED feedback for wireless setup mode itself: `setup()` calls `bootModeIndicator.show(bootMode)` once, which calls `setIdentifying(true)` on all 12 `LedPairStation`s for `BootMode::WirelessSetup` (reusing the #2d-b identify-blink override); the `BootMode::WirelessSetup` branch of `loop()` then calls only `ledStation.update()` on all 12 stations every tick, so all 12 LED pairs flash for as long as the setup AP is open — the two states can never overlap (wireless setup mode never starts MQTT, so the MQTT-triggered identify-blink can't fire at the same time), so sharing the exact visual is unambiguous in practice. `presenceAnnouncer.update(mqttLink.connected())` runs unconditionally every tick — this is load-bearing, not just safe: it's what drives the 30-second heartbeat (sub-project #9b) that lets two simultaneously-connected colliding panels each observe the other's live MAC publish, now that `mqttClientId` (declared above, MAC-derived as of #9b, not `nodeId`-derived) no longer forces them apart. **Sub-project #2d-b** adds identify-blink: `setup()` also subscribes to `PresenceTopics::identifyTopic(nodeId)` with a lambda that ignores the payload and calls `identifyTimer.trigger()` (an `IdentifyModeTimer` global, declared after `presenceAnnouncer`); `loop()` computes `identifying` once per tick from `identifyTimer.isActive()` and calls `ledStation.setIdentifying(identifying)` on all 12 stations in a loop that runs — and must keep running — *before* the existing `ledStation.update()` loop, so a fresh activation takes visible effect the same tick it arrives; merging the two loops would silently reintroduce a one-tick activation lag with no test to catch it. Identify has no button/feedback suppression (a deliberate contrast with the collision handling above — it's a pure visual overlay) and deliberately still works during a latched collision, since that is the intended way to tell two colliding panels apart. `src/esp32node4/main.cpp` (sub-project #1, `[env:esp32node4]`) is the 4-channel "node board" composition root and started life (sub-project #1) as a **minimal placeholder** wiring only `NvsBootstrap`, `EspDeviceIdentity`, `EspUartPort`, `NvsConfigStore4` → `CommissioningSession4` → `SerialCommissioningAdapter4` for bench-serial commissioning, plus `WebFormCommissioningAdapter4` → `CaptivePortalServer4` and a `NvsSetupModeRequestStore`/`BootModeSelector::select()` boot-mode decision. Sub-project #3 added the file's first `Clock` plus a two-button composite setup-mode trigger, which is what makes `BootMode::WirelessSetup` reachable there. Sub-project #4 added a `TurnoutNodeConfig[4]` table and the node board's turnout hardware: 4 direct-GPIO `ArduinoDigitalInput` buttons, 4 `IndependentLedPairStation`s (two real GPIOs per pair), one shared `NullTurnoutCommandPort`, and 4 unmodified `ToggleTurnoutStation`s. No button matrix exists — and never will, since the node board uses direct-GPIO buttons — and no WiFi station, MQTT, presence/collision, or identify machinery exists yet (WiFi/MQTT now exist as of sub-project #7 — see below; presence/collision was wired in sub-project #8 — see below; identify was wired in sub-project #9 — see below). See the "4-channel node board: channel-count templating (sub-project #1)", "Composite setup-mode trigger (sub-project #3)", "Turnout station wiring (sub-project #4)" and "Boot-mode LED indicator (sub-project #5)" sections below.
- `test/test_<name>/test_main.cpp` — Catch2 test binaries (60 test suites)
- `test/support/` — test doubles (FakeDigitalInput, FakeClock, FakeDigitalOutput, FakeTurnoutCommandPort, FakeLocoNetTransport, FakeLocoNetSwitchDriver, FakeSetupModeRequestStore, FakeBootModeIndicator)
- `jmri/panel_mqtt_turnout_bridge.py` — **removed in sub-project #9c.** This was the JMRI-side Jython startup script that bridged MQTT to JMRI `Turnout` objects for the ESP32 panel's original JMRI-over-MQTT integration (`track/turnout/<jmriSystemName>` topics, no shadow "MT" turnouts or Logix). Orphaned by sub-project #9a's migration to Loco2MQTT (nothing in `lib/`/`src/`/`test/` referenced it any more) and deleted here. See `jmri/panel_mqtt_turnout_bridge.py` in this file's git history (last present at commit `2d893e4^`) if you need to see what it did.

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

### Presence + collision detection (sub-project #2d-a, heartbeat redesign in #9b)

Each panel publishes two MQTT topics — `panel/<nodeId>/status` = `"online"`
(the existing LWT already publishes `"offline"` there on disconnect — this
closes the asymmetry the #7b spec left as a placeholder) and
`panel/<nodeId>/mac` = the panel's own last-4-hex-digit MAC — immediately on
every successful connect, **and again every 30 seconds while connected**
(`MqttPresenceAnnouncer::kHeartbeatIntervalMs`). Neither publish is retained
(`retained=false`) as of #9b — see below for why. Each panel also
subscribes to its own `mac` topic; observing a value that isn't its own
means a second physical panel is claiming the same `nodeId` (an operator
commissioning error). Detection suppresses all 12 turnout buttons, stops
applying turnout feedback (LEDs fall back to the existing blink/unconfirmed
state via `clearIndicator()` — no new LED pattern was built), and logs once
via serial.

**Why a heartbeat instead of retained messages (sub-project #9b):** the
original #2d-a design relied entirely on the MQTT retained flag, because
each panel's MQTT client ID was derived from its `nodeId`
(`"maltbee-esp32-" + nodeId`) — so two colliding panels shared a client ID,
and MQTT's broker-enforced duplicate-`clientId` behavior meant they could
never be connected at the same time (each new connection kicked the other
off). Without retention, the disconnected panel's last claim would never
still be on the broker for the other to read. Loco2MQTT (sub-project #9a)
ignores the retained flag entirely, so that whole mechanism went silently
dead against it. #9b fixes this at the root instead of working around it:
`src/esp32/main.cpp`'s MQTT client ID now derives from the panel's own MAC
(`"maltbee-esp32-" + ownMac.lastFourHexDigits()`), so two colliding panels
get *distinct* client IDs and both stay connected to the broker
simultaneously — and since both are now alive at once and heartbeating
every 30 seconds, each observes the other's live publish without needing
retention at all. **Do not revert either topic's `retained` flag to `true`
or revert the client ID back to a `nodeId`-derived string — both are
required together for detection to work against Loco2MQTT.**
`NodeIdentityGuard`'s existing self-echo immunity (a panel observing its
own MAC is never a false collision) already handles a heartbeat's repeated
self-observation correctly; it was not changed by #9b.

**Detection latency:** up to 30 seconds worst case (the heartbeat interval)
from the moment both panels are simultaneously connected — a deliberate
tradeoff for a robust, non-retention-dependent mechanism; previously
detection was near-instant on the reconnect edge, since a fresh connection
was itself the trigger.

**Known, accepted limitation (from #2d-a, no longer applicable against
Loco2MQTT):** the original design's "stale retained MAC on a decommissioned/
reassigned panel causes one false-positive boot" limitation was specific to
retained messages persisting on the broker after the publisher disconnects.
Since #9b's topics are no longer retained, this exact scenario can no
longer occur against Loco2MQTT — though `presenceAnnouncer.update()`
running unconditionally every tick, which is what made the false positive
self-heal within one boot under the old design, remains true today as
well. Serial commissioning and the BOOT-button wireless-setup gesture
(wired to the *raw* `bootButton` input, independent of the gated turnout
matrix — see the `ButtonSetupModeTrigger` entry above) both keep working
during a collision lockout, so recovery never requires broker
administration.

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
above — and it deliberately still works during a latched collision: it
locates the physical panel(s) currently claiming a given `nodeId`, which is
useful even mid-collision, but when the colliding pair are both panels
(same family), both flash identically and simultaneously, so it cannot tell
which-is-which between them — that still requires a serial terminal on each
candidate panel to check its own reported `nodeId`/MAC. Publish this
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

### 4-channel node board: channel-count templating (sub-project #1)

**This is the most recent work in the repo (2026-10-21), despite its low
sub-project number** — it opens a new "4-turnout node board" effort whose
numbering restarts at #1, so it is chronologically *after* everything
numbered #2c-*, #2d-* and #9* above. Design spec:
`docs/superpowers/specs/2026-10-21-esp32-node4-channel-count-design.md`;
plan: `docs/superpowers/plans/2026-10-21-esp32-node4-channel-count.md`.

The 12-channel panel's channel count was hard-coded as
`NodeConfig::kChannelCount` throughout the ESP32 commissioning chain. A new
4-channel "node" board needs the same chain at `N == 4`, so the count became
a `template <int N>` parameter. The pattern, applied uniformly: each class
`X` was renamed in place to `BasicX<N>`, with two aliases at the bottom of
its header — `using X = BasicX<12>;` (the pre-existing name, so the
12-channel panel and every existing test and `src/esp32/main.cpp` reference
is **zero behavior change**) and `using X4 = BasicX<4>;` (the node board).
Eight classes became class templates this way:

- `NodeConfig` (`domain/NodeConfig.h`) → `BasicNodeConfig<N>`
- `WebFormSubmission` (`domain/WebFormSubmission.h`) → `BasicWebFormSubmission<N>`
- `ConfigStore` (`ports/ConfigStore.h`) → `BasicConfigStore<N>`
- `CommissioningSession` (`application/CommissioningSession.h`) → `BasicCommissioningSession<N>`
- `WebFormCommissioningAdapter` (`adapters/WebFormCommissioningAdapter.h`) → `BasicWebFormCommissioningAdapter<N>`
- `SerialCommissioningAdapter` (`adapters/SerialCommissioningAdapter.h`) → `BasicSerialCommissioningAdapter<N>`
- `NvsConfigStore` (`adapters/NvsConfigStore.h`/`.cpp`) → `BasicNvsConfigStore<N>`
- `CaptivePortalServer` (`adapters/CaptivePortalServer.h`/`.cpp`) → `BasicCaptivePortalServer<N>`

The classes with a `.cpp` (`NvsConfigStore`, `CaptivePortalServer`) keep
their definitions out of the header and carry explicit instantiations at the
bottom of the `.cpp` instead — `template class BasicNvsConfigStore<12>;` /
`<4>;` and the same pair for `BasicCaptivePortalServer`, `BasicLoco2MqttTurnoutCommandAdapter`
(sub-project #6), and `BasicLoco2MqttFeedbackSource` (sub-project #6). **Add
a line there for any future `N`**, or the link will fail with undefined
symbols rather than a clear compile error.

Two further classes are stateless, so they got a **templated method** rather
than becoming class templates (deliberately — a stateless helper has no `N`
to store):

- `SetupFormRenderer::render()` (`domain/SetupFormRenderer.h`) — now
  `template <int N> static std::string render(const BasicWebFormSubmission<N>&, ...)`,
  deducing `N` from the submission it is handed, and loops to `N` instead of
  `NodeConfig::kChannelCount`.
- `BootModeSelector::select()` (`domain/BootModeSelector.h`) — now
  `template <int N> static BootMode select(const BasicNodeConfig<N>&, bool)`.

A new `[env:esp32node4]` PlatformIO environment (same `espressif32`/
`esp32dev` board/`-std=gnu++17`/`lib_ignore = McsLoconet` settings as
`[env:esp32dev]`, differing only in `build_src_filter = -<*> +<esp32node4/*>`)
builds a new composition root, `src/esp32node4/main.cpp`. That root wires the
`*4` aliases only: `NvsBootstrap` (first global, same NVS-before-global-init
hazard as the panel's), `EspDeviceIdentity().mac()`, `EspUartPort`,
`NvsConfigStore4` → `CommissioningSession4` → `SerialCommissioningAdapter4`,
`WebFormCommissioningAdapter4` → `CaptivePortalServer4`, a
`NvsSetupModeRequestStore`, and a `BootModeSelector::select()` boot-mode
decision. At the time of #1 it deliberately wired **no** turnout stations, LED
pairs, button matrix, WiFi station, MQTT, presence/collision detection, or
identify-blink. Turnout stations and LED pairs have since been wired
(sub-project #4 — 4 `ToggleTurnoutStation`s and 4
`IndependentLedPairStation`s; see the "Turnout station wiring (sub-project #4)"
section below). The button **matrix** is correctly still absent and always
will be — the node board uses four direct-GPIO buttons, not a scanned matrix.
WiFi station and MQTT were wired in sub-project #7 (see below); presence/collision detection was wired in sub-project #8 (see below); identify-blink was wired in sub-project #9 (see below). Serial commissioning
(`show`/`id`/`wifi`/`broker`/`turnout <1-4> address <n>`/`save`/`reboot`) is
fully reachable and functional on the node board today.

**The gap this sub-project left open — `BootMode::WirelessSetup` being
unreachable at runtime on the node board — is now closed (sub-project #3).**
At the time of #1, `CaptivePortalServer4` and `WebFormCommissioningAdapter4`
were constructed and the `BootMode::WirelessSetup` branches in `setup()`/
`loop()` were written, but nothing in `src/esp32node4/main.cpp` ever called
`NvsSetupModeRequestStore::requestOnNextBoot()`, so `consumeRequest()` always
returned `false` and `select()` could only ever return `Normal` or
`NeedsCommissioning`. Sub-project #3 wired the missing trigger: holding either
GPIO0 (the kit's BOOT button) or GPIO16 (a dedicated SETUP button) for 3
seconds and releasing now requests wireless setup and reboots into it, the two
buttons OR'd together behind a new `EitherActiveDigitalInput` feeding an
unmodified `ButtonSetupModeTrigger`. See the "Composite setup-mode trigger
(sub-project #3)" section below for the full detail.

**Done, sub-project #6 — see the "Loco2MQTT channel-count templating"
section below.** The note below described a real gap at the time it was
written; both classes now have the `Basic<X><N>` treatment it called for.

**Forward-looking note for whichever sub-project wires MQTT onto the node
board:** `Loco2MqttTurnoutCommandAdapter` and `Loco2MqttFeedbackSource`
(`lib/McsEsp32/src/adapters/`) were **not** part of this templating effort and
still hard-bind to `std::array<int, NodeConfig::kChannelCount>` (i.e.
`std::array<int, 12>`) in both their headers and their `.cpp` files, and
`Loco2MqttTurnoutCommandAdapter::send()`/`Loco2MqttFeedbackSource`'s
constructor loop still range-check against `NodeConfig::kChannelCount`
directly. `NodeConfig4::channelTurnoutAddresses` is a `std::array<int, 4>`
and will not bind to a reference parameter expecting `std::array<int, 12>`,
so both classes need the same `Basic<X><N>` treatment (plus
`using Loco2Mqtt…4 = Basic…<4>;` aliases) before the node board can talk to
Loco2MQTT. Budget for it.

**Test coverage added:** four new N=4 suites —
`test/test_node_config4`, `test/test_commissioning_session4`,
`test/test_web_form_commissioning_adapter4`,
`test/test_serial_commissioning_adapter4` (44 → 48 suites) — plus appended
N=4 `TEST_CASE`s inside two *existing* suites,
`test/test_setup_form_renderer` and `test/test_boot_mode_selector`, which
cover the two templated-method classes and so did not need new suite
directories. `test/support/FakeConfigStore4.h` is the N=4 test double,
mirroring `FakeConfigStore` (including its `failNextSave` flag).

### Independent-output turnout indicator (sub-project #2)

`docs/pcb/turnout-node-4ch-pcb-design.md` §2.3 gives each node-board
turnout's red and green LED its own real GPIO, instead of the 12-channel
panel's single shared pin — so "both off" and "both on" (amber) are
physically real for the first time on an ESP32 target. Decided with Paige:
keep the exact visual the panel already has (alternating red/green) for
`TurnoutIndicator::clear()`'s unconfirmed state, rather than spend that new
off/amber vocabulary on a state that doesn't need it.

Two new classes in `lib/McsEsp32/`, both **deliberately independent of**
`LedPairDriver`/`LedPairOutput` — not sharing logic via inheritance or an
extracted helper, and not modifying either existing class:

- `IndependentLedPairDriver` (domain) carries its own complete copy of
  `LedPairDriver`'s `Mode`/blink/identify state machine (reusing only the
  `LedPairColor` enum, via `#include "LedPairDriver.h"`), but its
  constructor takes two real `DigitalOutput&`s (green, red) instead of one
  shared pin, and `writeColor()` writes both together so exactly one is
  ever lit — including mid-blink and mid-identify — reproducing the shared-
  pin driver's exact visual on hardware that could otherwise go dark or
  show amber. `setIdentifying()` exists on it for interface completeness,
  mirroring `LedPairDriver`'s own override — sub-project #5's
  `IndependentLedPairFlashIndicator` is its first caller (see below);
  `setIdentifying()` gained its second caller, the MQTT-triggered
  identify-overlay, in sub-project #9 (see the "Identify-overlay
  (sub-project #9)" section below).
- `IndependentLedPairOutput` (adapter) mirrors `LedPairOutput` exactly,
  implementing `DigitalOutput` and forwarding `set()`/`isSet()` to one side
  of an `IndependentLedPairDriver`.

**The duplication is deliberate, not an oversight — and it's a liability to
track, not just a design note.** The two drivers' state machines are
byte-for-byte equivalent today only because of the "keep the old blink"
decision above; a future change to either one's `Mode`/blink/identify
logic (e.g. fixing `setIdentifying(false)`'s known quirk of not
re-stamping `lastToggleTime_`, which both drivers share unfixed since
`LedPairDriver.cpp:52-55` and `IndependentLedPairDriver.cpp`'s mirrored
lines are identical) **must be applied to both files**, since neither
includes or calls the other. The natural point to revisit whether they
should stay separate — or to extract `LedPairColor` out of
`LedPairDriver.h` into its own header, removing the one include dependency
the "independent" driver still has on the driver it's otherwise
independent of — is whichever later sub-project adds the node board's
GPIO-owning composition helper (the `IndependentLedPairStation` equivalent
to `LedPairStation`, intentionally not built yet — see below).

**Deferred at the time, now done (sub-project #4):** the composition-helper
class (real `ArduinoDigitalOutput` construction for the node board's 8 LED
GPIOs) and the wiring into `src/esp32node4/main.cpp` both landed in
sub-project #4 as `IndependentLedPairStation` — see the "Turnout station
wiring (sub-project #4)" section below. Identify-overlay wiring for these
LEDs — `IndependentLedPairDriver::setIdentifying()`'s *MQTT-triggered*
use — is **now done** (§7 item 4's remaining half, sub-project #9 — see
the "Identify-overlay (sub-project #9)" section below); sub-project #5
had already wired a boot-mode-triggered call to it (see below), and
`setIdentifying()` now has both callers.

**Test coverage added:** `test/test_independent_led_pair_driver` (18
cases, mirroring `test_led_pair_driver`'s 18 — every mirrored case asserts
both GPIOs, not one, so assertion count is higher: 67 vs. 37) and
`test/test_independent_led_pair_output` (3 cases, exact parity with
`test_led_pair_output`) — 48 → 50 suites.

### Composite setup-mode trigger (sub-project #3)

`docs/pcb/turnout-node-4ch-pcb-design.md` §3.4 gives the node board its own
6 mm SETUP button on GPIO 16 (to GND, internal pull-up), while the ESP32 dev
kit's own BOOT button (GPIO 0) stays available as a second trigger. Firmware
had to make *either* button perform the panel's existing 3-second
hold-then-release wireless-setup gesture. Design spec:
`docs/superpowers/specs/2026-10-21-esp32-node4-setup-trigger-wiring-design.md`;
plan: `docs/superpowers/plans/2026-10-21-esp32-node4-setup-trigger-wiring.md`.

One new class in `lib/McsEsp32/src/adapters/EitherActiveDigitalInput.h`/`.cpp`:
it implements `DigitalInput` over exactly two composed `DigitalInput&`s, with
`isActive()` returning their OR. **Exactly two, deliberately not variadic or
array-backed** — the one need it exists for is precisely two buttons, and a
fixed pair needs no capacity bound, no count, and no AVR-hostile container.
It reads both references live on every `isActive()` call rather than latching
anything at construction, which matters because it is polled every `loop()`
tick. It is general-purpose and knows nothing about buttons or setup mode,
mirroring `GatedDigitalInput`'s framing.

The wiring in `src/esp32node4/main.cpp`:

- `ArduinoClock systemClock;` — the file's **first-ever `Clock`**. The #1
  placeholder root had no time source at all, since bench-serial
  commissioning needs none.
- Two real GPIO buttons, both active-low with the internal pull-up:
  `bootButton` on GPIO 0 (the kit's BOOT button) and `setupButton` on GPIO 16
  (the new dedicated SETUP button), matching §3.4.
- `EitherActiveDigitalInput setupTriggerInput(bootButton, setupButton);`
  feeding an **unmodified** `ButtonSetupModeTrigger setupTrigger(...,
  SETUP_TRIGGER_HOLD_MS)`, where `SETUP_TRIGGER_HOLD_MS = 3000` matches the
  12-channel panel's constant. The trigger's hold logic and its 50 ms
  release-settle window are untouched — the composite is the only new piece.
- `setup()` calls both buttons' `begin()` **unconditionally, before** the
  boot-mode check, so GPIO 16's pull-up is configured on every boot including
  a `WirelessSetup` one. `loop()`'s existing `setupTrigger.update()` /
  `requested()` block (which calls `requestOnNextBoot()` and only reboots if
  that returned `true`) is reached on the non-`WirelessSetup` path, exactly as
  on the panel.

**Net effect: `BootMode::WirelessSetup` is genuinely reachable on the node
board for the first time.** A 3-second hold-then-release of *either* button
enters wireless setup, so the captive portal and web-form commissioning are
live on a fresh node board rather than dormant.

**Scope note:** this sub-project was deliberately narrowed, with Paige, from
the original roadmap item — which bundled "direct-GPIO turnout button inputs
**and** the composite setup trigger" (§7 items 2 and 6) — down to the trigger
alone (§7 item 6). At the time, the four direct-GPIO turnout buttons on GPIO
34/35/36/39 had no per-channel turnout station to be wired *to*, so wiring
them then would have produced four inputs nothing reads. **That deferral is
now closed (sub-project #4)**, which built `IndependentLedPairStation` and
wired all four buttons into four `ToggleTurnoutStation`s — see the "Turnout
station wiring (sub-project #4)" section below. This section's parenthetical
guess that it would need "`ToggleTurnoutStation4`s" turned out to be
unnecessary: `ToggleTurnoutStation` needed no channel-count templating at
all, since it already took individual button/output instances per channel
rather than a channel array.

**Test coverage added:** `test/test_either_active_digital_input` (5 cases —
the four OR truth-table combinations, plus one proving the composite reads
through to the live references on every call rather than latching at
construction) — 50 → 51 suites. `ButtonSetupModeTrigger` itself needed no new
tests: it was handed a `DigitalInput&` already and is unchanged.

### Turnout station wiring (sub-project #4)

Closes both deferrals above at once — sub-project #2's missing GPIO-owning
composition helper and sub-project #3's four unwired turnout buttons. Neither
is independently useful: a button needs a station to feed its edge into, and a
station needs real LED outputs to drive. Design spec:
`docs/superpowers/specs/2026-10-22-esp32-node4-turnout-station-wiring-design.md`;
plan: `docs/superpowers/plans/2026-10-22-esp32-node4-turnout-station-wiring.md`.

One new class,
`lib/McsEsp32/src/adapters/IndependentLedPairStation.h`/`.cpp`: a direct
mirror of `LedPairStation`, `#ifdef ARDUINO`-guarded, owning **two** real
`ArduinoDigitalOutput` GPIOs (red, green) instead of one shared pin, plus its
`IndependentLedPairDriver` and two `IndependentLedPairOutput`s. It has **zero
logic of its own** — pure construction and forwarding (`begin()`, `update()`,
`setIdentifying()`, `green()`, `red()`, `driver()`) — so it has **no native
test**, the same precedent `LedPairStation` already sets.

Two safety properties were added to it deliberately and should not be removed:

- **Copy construction and copy assignment are `= delete`d.** The class is
  self-referential: `driver_` holds `DigitalOutput&` references into its own
  `redGpio_`/`greenGpio_` members, and `green_`/`red_` hold an
  `IndependentLedPairDriver&` into its own `driver_`. Any copy would silently
  produce dangling references into the source object. Declaring the copy
  constructor also suppresses the implicit move constructor/assignment, which
  would have the same defect. This does **not** break `main.cpp`'s
  `IndependentLedPairStation ledStations[4] = {...}` array-of-prvalues
  initialization, which relies on C++17's guaranteed copy elision — both
  `[env:esp32node4]` and `[env:esp32dev]` build with `-std=gnu++17`, so no
  copy or move constructor is ever invoked there.
- **`IndependentLedPairConfig` is constructed with designated initializers**
  (`IndependentLedPairConfig{.redGpioPin = ..., .greenGpioPin = ...}`) rather
  than bare positional braces. Its two members are both `int`, so a
  transposition would compile silently with no warning and surface only as
  inverted LED colors on the bench. Designated initializers are available
  because the build uses the **GNU** dialect (`-std=gnu++17`, not strict
  `-std=c++17`).

`src/esp32node4/main.cpp` wiring follows the panel's established
`TURNOUT_CONFIGS[]` + range-`for` pattern: a `TurnoutNodeConfig[4]` table
(address, name, button/red/green GPIO per row) with the §4 pin map —

| Channel | Button GPIO | Red GPIO | Green GPIO |
|---|---|---|---|
| T1 | 34 | 32 | 33 |
| T2 | 35 | 25 | 26 |
| T3 | 36 | 27 | 13 |
| T4 | 39 | 23 | 19 |

— then 4 `ArduinoDigitalInput` buttons (`activeLow=true`,
`useInternalPullup=false`, matching the hardware spec's external 10 kΩ
pull-up), 4 `IndependentLedPairStation`s reusing the panel's own
`BLINK_INTERVAL_MS = 500` / `DEFAULT_LED_COLOR = LedPairColor::Red` values, a
single shared `NullTurnoutCommandPort` across all 4 stations (mirroring how
the panel shares one real command port across its 12), and 4 **unmodified**
`ToggleTurnoutStation`s constructed `(address, name, button,
ledStation.green(), ledStation.red(), systemClock, turnoutCommandPort)` —
green = closed, red = thrown, the panel's convention. `setup()` calls
`begin()` on buttons, then LED stations, then stations; `loop()` calls
`update()` on LED stations then stations, after the `WirelessSetup` branch's
`return`, with the `WirelessSetup` branch itself also gaining its own
`update()` loop in sub-project #5 (see below) so the LEDs still
flash/tick during that mode.

**Closed by sub-project #7 — see below.** This paragraph described the
situation at the time; it no longer reflects the shipped behavior.
**Known, accepted gap: a button press currently produces zero observable
effect.** `ToggleTurnoutControl::update()` only calls
`turnoutCommandPort_.send(...)` — it never mutates the `Turnout` and never
calls `indicator_.display()`. The LED changes only via `applyFeedback()`, a
separate method, and nothing on the node board calls it: the command port is
`NullTurnoutCommandPort` (a no-op) and there is no feedback source, because
`Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource` are still
hard-bound to 12 channels (see sub-project #1's forward-looking note). So all
4 LED pairs sit on their post-`begin()` unconfirmed blink forever, completely
independent of button activity. **Bring-up today can verify** that all 4
channels' red/green GPIOs are wired to the right pins and blink at 500 ms.
**It cannot verify anything about button response** — that capability arrives
only with real Loco2MQTT send/receive wiring, a future sub-project. The
design spec for this sub-project originally claimed it "proves the
button-to-LED mechanics work on real hardware"; that claim was false and has
been corrected in place.

**Closed by sub-project #5 — see below.** This paragraph described the
situation at the time; it no longer reflects the shipped behavior.
**Known, accepted gap: all 4 LED pairs sit solid red for the duration of any
`BootMode::WirelessSetup` session.** `setup()` runs every LED station's
`begin()` *before* the boot-mode check (deliberately — moving it below risks
leaving the LED GPIOs unconfigured and floating during setup mode, which is
worse), which leaves each driver at its resting `Mode::Blink` state showing
red; `loop()` then returns early on the `WirelessSetup` path before ever
reaching `ledStation.update()`, so the red never blinks or changes. Harmless
and deliberately not reordered. Whichever future sub-project owns boot-mode
LED feedback (§7 item 4) should give wireless-setup mode its own visual here
— the panel's equivalent calls `setIdentifying(true)` once via
`bootModeIndicator.show(bootMode)` in `setup()`, then calls only
`ledStation.update()` every tick inside its `WirelessSetup` branch of
`loop()`, which the node board does not do yet.

**Test coverage added: none.** `IndependentLedPairStation` is pure
construction/forwarding with no logic to test (`LedPairStation`'s precedent),
and `main.cpp` is a composition root verified by build only. Suite count stays
at 51.

### Boot-mode LED indicator (sub-project #5)

Closes half of the roadmap item "Boot-mode, setup and identify feedback
for the new LEDs" — the half that doesn't need MQTT. Discussed with
Paige: on the panel, `BootModeIndicator`/`LedPairFlashIndicator<N>` is
driven purely by the locally-known `BootMode` value computed at boot, with
no MQTT dependency; the identify-overlay (`IdentifyModeTimer` plus an MQTT
subscription callback) is a separate mechanism the node board can't have
yet, since `Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource`
needed the `Basic<X><N>` treatment (now done, sub-project #6 — see below)
before any WiFi/MQTT wiring lands on the node board (sub-project #1's
forward-looking note, now closed). **This
sub-project is the boot-mode indicator only; the identify-overlay moves to
whichever future sub-project adds real Loco2MQTT wiring to the node
board** (that sub-project turned out to be #7, which deliberately did NOT
include the identify-overlay — it was wired later, in sub-project #9; see
the "Identify-overlay (sub-project #9)" section below).

One new class,
`lib/McsEsp32/src/adapters/IndependentLedPairFlashIndicator.h` — a
near-byte-for-byte mirror of the panel's own
`LedPairFlashIndicator<N>` (`lib/McsEsp32/src/adapters/LedPairFlashIndicator.h`),
header-only, templated against `IndependentLedPairDriver*` instead of
`LedPairDriver*`. Implements `BootModeIndicator`:
`show(BootMode::WirelessSetup)` calls `setIdentifying(true)` on every
pair (the same 150ms flash `IndependentLedPairDriver` already provides,
confirmed unchanged from sub-project #2); `show(BootMode::NeedsCommissioning)`
calls `setGreen(false)`/`setRed(false)` on every pair, which — against a
driver already resting in `Mode::Blink` from `begin()`/`clearIndicator()`
— is a no-op, so `NeedsCommissioning` shows the exact same red/green
alternating blink the unconfirmed state already shows (the same "keep the
existing blink visual" decision sub-project #2 made, inherited here rather
than re-decided); `show(BootMode::Normal)` leaves identifying off.
`update()` is a no-op body — `IndependentLedPairFlashIndicator` itself
drives nothing per-tick; the actual flash/blink timing comes from
`IndependentLedPairStation::update()`, called from the `ledStations` loop
in `loop()` (including, after this sub-project's own fix, the one inside
the `BootMode::WirelessSetup` branch — see below).

`src/esp32node4/main.cpp` wiring mirrors the panel's exact two call sites:
a `bootModeLedPairs` array of `IndependentLedPairDriver*` built from
`ledStations[i].driver()`, one `IndependentLedPairFlashIndicator<4>
bootModeIndicator(bootModeLedPairs);` global, `bootModeIndicator.show(bootMode);`
called in `setup()` right after the `station.begin()` loop and before the
`BootMode::WirelessSetup` check, and `bootModeIndicator.update();` as the
very first line of `loop()` — identical placement to `src/esp32/main.cpp`.

**Net effect:** all 4 node-board LED pairs now flash fast (150ms,
green/red alternating) for the duration of a `BootMode::WirelessSetup`
boot, instead of sitting solid red for the whole session — closing the
"Known, accepted gap" sub-project #4 documented about wireless-setup mode
giving no LED feedback. (This WirelessSetup flash required one additional
fix beyond the original wiring: `loop()`'s `WirelessSetup` branch returned
before ever reaching the `ledStations` update loop, so the flash never
actually ticked until that branch gained its own `ledStation.update()`
loop, mirroring `src/esp32/main.cpp`'s identical branch.) `NeedsCommissioning`
and `Normal` are visually unchanged (both already showed the unconfirmed
blink via `begin()`). (In practice, the flash doesn't begin until the
first `loop()` tick — `setup()`'s own `captivePortalServer.begin()` call
can block for several seconds scanning WiFi networks before `loop()` ever
runs, during which the LEDs sit solid green; this is pre-existing panel
behavior too, not something unique to the node board.)

**Test coverage added:** `test/test_independent_led_pair_flash_indicator`
(3 cases, mirroring `test_led_pair_flash_indicator`'s 3 cases with both
GPIOs asserted per pair, same pattern `test_independent_led_pair_driver`
already established over `test_led_pair_driver`) — 51 → 52 suites.

**Non-goals, deferred:** identify-overlay wiring (`IdentifyModeTimer`, an
MQTT `panel/<nodeId>/identify` subscription, a `setIdentifying()` loop
driven by it) — closed by sub-project #9, see the "Identify-overlay
(sub-project #9)" section below. Status/activity LEDs — closed by
sub-project #10, see the "Status/activity LEDs (sub-project #10)"
section below. **The "4-channel commissioning form" item named here was
never actually a separate gap** — `WebFormCommissioningAdapter4`/
`CaptivePortalServer4` were wired in sub-project #1 and `SetupFormRenderer`'s
N=4 rendering has been tested since that same sub-project (`test_setup_form_renderer`'s
"render includes a labeled input for each of the 4 turnout channels on the
node variant" case, plus `test_web_form_commissioning_adapter4`'s own
suite). `CaptivePortalServer4` itself is `#ifdef ARDUINO`-guarded like its
N=12 counterpart and has no native test, the same precedent every other
pure-construction hardware shim in this project follows. This "remains its
own future sub-project" phrasing was stale early-roadmap framing, left
unexamined because no sub-project's scope was ever specifically "verify the
commissioning form" — corrected in place here rather than via a dedicated
sub-project, since there was nothing left to build.

### Loco2MQTT channel-count templating (sub-project #6)

Closes the gap sub-project #1's own forward-looking note flagged:
`Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource`
(`lib/McsEsp32/src/adapters/`) were hard-bound to
`std::array<int, NodeConfig::kChannelCount>` and could not be constructed
with `NodeConfig4`'s 4-element `channelTurnoutAddresses` array. This is
the first of what the roadmap originally called "real WiFi/MQTT/Loco2MQTT
wiring on the node board" — narrowed, with Paige, to templating alone:
isolated, fully testable, no hardware dependency, no `main.cpp` changes.
Wiring these classes into `src/esp32node4/main.cpp` alongside real
`WiFiLink`/`MqttLink`/`BrokerAddressResolver`/`EspMdnsResolver` was done
in sub-project #7 (2026-10-22, in this node-board effort's own
numbering — not the unrelated, older "#7b" presence-detection spec
referenced elsewhere in this file) — see below. **Presence/collision
detection was closed by sub-project #8 — see below.** The identify-overlay
was closed by sub-project #9 — see the "Identify-overlay (sub-project #9)"
section below.

Both classes got the same general shape of `Basic<X><N>` treatment sub-project #1
used for `NvsConfigStore`/`CaptivePortalServer` (the two prior classes
with a `.cpp`): `BasicLoco2MqttTurnoutCommandAdapter<N>` and
`BasicLoco2MqttFeedbackSource<N>`, method bodies as template definitions
in each `.cpp`, explicit instantiations for `N=12`/`N=4` at the bottom of
each `.cpp`, and `using X = Basic...<12>;`/`using X4 = Basic...<4>;`
aliases at the bottom of each header — zero behavior change for the
existing N=12 panel code. Both classes dropped their
`domain/NodeConfig.h` include entirely, since `N` as a template parameter
removed their only dependency on it. The command adapter's range check
became `address > N` (was `address > NodeConfig::kChannelCount`); the
feedback source's constructor loop became `for (int i = 0; i < N; ++i)`
(was `... < NodeConfig::kChannelCount; ...`).

One structural difference from `NvsConfigStore`/`CaptivePortalServer`:
neither Loco2MQTT `.cpp` file is `#ifdef ARDUINO`-guarded (both were
already native-tested translation layers, unlike those two hardware-shim
classes), which is exactly what makes the new N=4 native test suites
possible to link at all.

**Test coverage added:** `test/test_loco2mqtt_turnout_command_adapter4`
(5 cases, mirroring `test_loco2mqtt_turnout_command_adapter`'s 5 cases)
and `test/test_loco2mqtt_feedback_source4` (7 cases, mirroring
`test_loco2mqtt_feedback_source`'s 7 cases) — 52 → 54 suites. No N=4
mirror of `test_loco2mqtt_turnout_wiring` was added: its cases exercise
cross-class logic that doesn't differ between N=12 and N=4, so a copy
would add no coverage.

### Loco2MQTT WiFi/MQTT wiring (sub-project #7)

Closes the roadmap item CLAUDE.md has tracked since sub-project #4 as
"real WiFi/MQTT/Loco2MQTT wiring on the node board" — the exact piece
sub-project #6's design spec deferred, naming this sub-project by
number. `src/esp32node4/main.cpp` now wires a real `WiFiLink`/
`MqttLink`/`BrokerAddressResolver`/`EspMdnsResolver` stack, mirroring
`src/esp32/main.cpp`'s working reference implementation with
presence/collision detection and the identify-overlay both left out —
neither is part of this sub-project. **Presence/collision detection was
closed by sub-project #8 — see below.** The identify-overlay was closed
by sub-project #9 — see the "Identify-overlay (sub-project #9)" section
below.

`NullTurnoutCommandPort` is gone from this file, replaced by
`Loco2MqttTurnoutCommandAdapter4`/`Loco2MqttFeedbackSource4`
(sub-project #6's templated classes) constructed over the real
`MqttLink`. The MQTT client ID and `EspMdnsResolver`'s self-advertised
hostname both use `"maltbee-node4-" + ownMac.lastFourHexDigits()` —
MAC-derived, mirroring the panel's own `"maltbee-esp32-"`/
`"maltbee-panel-"` convention with a node4-specific prefix. The LWT
(will topic/message) uses `PresenceTopics::statusTopic(runningConfig.nodeId)`/
`"offline"`, the same values the panel passes — required unconditionally
by `MqttLink`'s constructor, and was dormant (nothing publishes `"online"`)
at the time. **Closed by sub-project #8 — see below**, which added the
active `MqttPresenceAnnouncer`; it reuses this exact topic rather than
inventing a new one.

`setup()` gates `wifiLink.begin()`/`mqttLink.begin()` on `configValid`,
mirroring the panel exactly but without the panel's two
`mqttLink.subscribe(...)` calls (both presence/identify-only, out of
scope here). `loop()` polls both links when `configValid`, then applies
real feedback (`feedbackSource.poll()` → `station.applyFeedback()`) when
`configValid && mqttLink.connected()`, or calls `station.clearIndicator()`
otherwise — the panel's equivalent gate also ANDs `!collision`, which
had no node4 equivalent at the time (**closed by sub-project #8 — see
below**, which folds `!collision` into this same gate).

**Net effect:** a button press on the node board now genuinely publishes
a Loco2MQTT command once commissioned with real WiFi/broker credentials
and LocoNet turnout addresses, and a real state-topic confirmation now
genuinely updates the turnout and its LED — closing sub-project #4's
"Known, accepted gap: a button press currently produces zero observable
effect" for the first time.

**Bring-up note:** three distinct failure modes look similar or confusing
on first network bring-up. Wrong WiFi credentials and an unreachable
MQTT broker produce visually IDENTICAL symptoms — all 4 LED pairs sit on
the 500ms unconfirmed blink forever, via `clearIndicator()`'s else
branch; the only diagnostic is `WiFiLink`'s one-shot "WiFi connected,
IP: ..." serial line (there's no equivalent serial message for MQTT
connect success or failure). With MQTT connected but no `/state` echo
from Loco2MQTT yet, the board is throw-only and looks dead after the
first button press — correct by design (`ToggleTurnoutControl::update()`
only mutates the turnout's recorded position via `applyFeedback()`,
which only fires on a real state-topic echo), but easily misread as a
firmware bug. Commands sent while MQTT is disconnected are silently
dropped (`mqttLink.publish()` on a dead client returns false with no
indication) — identical to the panel's existing behavior, not a new
defect.

**Test coverage added: none.** `main.cpp` is a composition root verified
by build only (`pio run -e esp32node4`/`esp32dev`/`megaatmega2560`), same
precedent as every prior `main.cpp`-only change in this project. Suite
count stays at 54.

### Presence/collision detection (sub-project #8)

Closes the gap left open by sub-project #7's own non-goals list: the
node board had no way to detect a `nodeId` collision between two
physical boards. `src/esp32node4/main.cpp` now wires the panel's own
`NodeIdentityGuard`/`MqttPresenceAnnouncer` pair, unmodified, over the
real `MqttLink` sub-project #7 wired in. The precondition the panel's
own "Presence + collision detection" section (above) explains is
required — a MAC-derived, not `nodeId`-derived, MQTT client ID, so two
colliding boards stay simultaneously connected long enough to observe
each other — was already satisfied: sub-project #7 used
`"maltbee-node4-" + mac4` for exactly this reason, even though
collision detection itself wasn't wired yet at the time.

`PresenceTopics` uses a single `panel/<nodeId>/...` topic prefix for
both board families — there is no separate node4-specific prefix. As a
new consequence of this sub-project, a panel and a node4 board
accidentally commissioned with the same `nodeId` now mutually detect
and lock each other out too, not just two node4 boards against each
other. This is new behavior as of this commit: previously only the
panel published or subscribed to these topics at all, so no node4-side
detection — cross-family or otherwise — existed. Arguably correct (they
genuinely do conflict over the same `nodeId`), but worth stating
explicitly since nothing described this cross-family case before.

The node board's 4 buttons, previously raw `ArduinoDigitalInput`s fed
directly into `ToggleTurnoutStation`, are now each wrapped in a
`GatedDigitalInput` — mirroring the panel's `gatedButtons[12]` — so a
latched collision can suppress all 4 at once via
`gated.setSuppressed(collision)`. `setup()` subscribes to
`PresenceTopics::macTopic(nodeId)` with a lambda forwarding to
`identityGuard.onMacObserved()`, inside the existing `configValid` gate
that also calls `wifiLink.begin()`/`mqttLink.begin()`. `loop()` computes
`collision` once per tick from `identityGuard.collisionDetected()`,
logs it once via a `static` one-shot guard (reusing the panel's exact
wording, with "panel" swapped for "node"), suppresses all 4
`gatedButtons`, and folds `!collision` into the same feedback-apply gate
`clearIndicator()` already used for `configValid`/MQTT-connected —
identical structure to the panel's own `loop()`.
`presenceAnnouncer.update(mqttLink.connected())` runs unconditionally
every tick, same as the panel, driving the 30-second heartbeat that lets
two simultaneously-connected colliding boards each observe the other's
live MAC publish.

**Net effect:** two node4 boards accidentally commissioned with the
same `nodeId`, once both are configured and connect to MQTT (any boot
mode other than `WirelessSetup`), detect each other within 30 seconds
(the heartbeat interval) and suppress all 4 buttons on both, falling
back to the blink/unconfirmed LED state and logging the collision once
over serial — identical behavior and latency to the panel's existing
mechanism.

**Bring-up note:** a latched collision produces the exact same LED
signature as a WiFi/broker connectivity failure — both fall back to
`clearIndicator()`'s solid unconfirmed blink on all 4 pairs, via the
same gate this sub-project extended with `!collision`. The only
discriminator is the serial collision line, and it fires only once per
boot (the `static collisionLogged` guard) — a technician who attaches a
terminal after boot, or re-attaches later, sees nothing, even though the
collision is still active. `NodeIdentityGuard`'s latch never clears on
its own; after re-commissioning one board's `nodeId` to resolve the
conflict, that board needs a reboot (the serial `reboot` command works)
before it re-evaluates cleanly. Serial commissioning and the BOOT/SETUP
wireless-setup gesture both stay live during a lockout — recovery never
needs broker access, same as the panel.

**Test coverage added: none.** `main.cpp` is a composition root verified
by build only (`pio run -e esp32node4`/`esp32dev`/`megaatmega2560`), same
precedent as every prior `main.cpp`-only change in this project. Suite
count stays at 54.

**Non-goals, deferred:** the identify-overlay (`IdentifyModeTimer`, an
MQTT `panel/<nodeId>/identify` subscription, a `setIdentifying()` loop
driven by it) — a separate choice from this sub-project's scope, closed
by sub-project #9 (see the "Identify-overlay (sub-project #9)" section
below). Status/activity LEDs — closed by sub-project #10 (see the
"Status/activity LEDs (sub-project #10)" section below). The "4-channel
commissioning form" item named in earlier sub-projects' non-goals was
never actually a separate gap — see the correction in the "Boot-mode LED
indicator (sub-project #5)" section's own non-goals above.

### Identify-overlay (sub-project #9)

(2026-10-25, in this node-board effort's own numbering — not the unrelated,
older "#9a"/"#9b"/"#9c" panel-focused sub-project sequence immediately
below, which happens to reuse the same digit.)

Closes the last item on CLAUDE.md's "deferred to whichever future
sub-project adds it" list for the node board's identify mechanism — tracked
since sub-project #5, reaffirmed as still open in #6, #7, and #8.
`src/esp32node4/main.cpp` now wires the panel's own `IdentifyModeTimer`,
unmodified, mirroring `src/esp32/main.cpp`'s existing identify-overlay
exactly: a new `IdentifyModeTimer identifyTimer(systemClock,
IDENTIFY_DURATION_MS);` global (declared right after `presenceAnnouncer`,
`IDENTIFY_DURATION_MS = 10000`, matching the panel's own constant),
`setup()` subscribing to `PresenceTopics::identifyTopic(nodeId)` with a
lambda that ignores the payload and calls `identifyTimer.trigger()`
(added right after the existing `macTopic` subscribe, inside the same
`if (configValid)` block), and a `loop()` block — gated by `if (bootMode
== BootMode::Normal)`, carrying the panel's own ordering comment
verbatim — that computes `identifying` once per tick from
`identifyTimer.isActive()` and calls `ledStation.setIdentifying(identifying)`
on all 4 `IndependentLedPairStation`s, running before the existing
`ledStation.update()` loop for the same reason the panel's does: a fresh
activation must take visible effect the same tick it arrives, not one
tick later.

No new library code: `IdentifyModeTimer`, `PresenceTopics::identifyTopic()`,
and `IndependentLedPairStation::setIdentifying()` (which the node board
already exercises for boot-mode flashing, sub-project #5) are all reused
unmodified. The `bootMode == BootMode::Normal` gate is carried over
verbatim from the panel even though it is currently redundant on both
boards — the MQTT subscribe that is the only way to trigger
`identifyTimer` lives inside the post-`WirelessSetup`-return `configValid`
branch, so by the time this line runs `bootMode` can only be `Normal` or
`NeedsCommissioning`, and `NeedsCommissioning` implies `configValid` was
false, which means the subscribe never happened either. The gate is kept
for literal parity with the panel rather than relying on that chain of
reasoning staying true if either file changes independently later.

**Net effect:** publishing any message (payload ignored) to
`panel/<nodeId>/identify` now makes a node4 board's 4 LED pairs flash
green/red in unison at a fast, fixed 150ms interval for 10 seconds, then
automatically revert — identical behavior to the panel. Re-publishing
before the window expires extends it rather than stacking a second one
(`IdentifyModeTimer`'s own existing behavior, unchanged). Identify has no
button/feedback suppression and deliberately still works during a latched
`nodeId` collision, exactly as on the panel: it locates the board(s)
currently claiming that `nodeId`, which is useful on its own even
mid-collision, but when the colliding pair are both node4 boards (same
family) it flashes both identically and simultaneously, so it cannot tell
which-is-which between them — that still requires a serial terminal on
each candidate board to check its own reported `nodeId`/MAC. The
`BootMode::WirelessSetup` branch
needs no change: its own LED flash (sub-project #5's
`IndependentLedPairFlashIndicator`/`bootModeIndicator`) is a separate
mechanism that the two can never overlap with, since wireless setup mode
never starts MQTT and that branch `return`s before reaching the new
`loop()` block above — exactly as on the panel.

Because `PresenceTopics` hard-codes a single `panel/<nodeId>/...` prefix
shared by both board families (first documented for the collision topic
in sub-project #8's section above), a panel and a node4 board
accidentally commissioned with the same `nodeId` can now also trigger
each other's identify-overlay flash, not just two boards of the same
family — the same kind of cross-family interaction #8 already documented
for collision detection, restated here for the identify topic.

**Test coverage added: none.** `main.cpp` is a composition root verified
by build only (`pio run -e esp32node4`/`esp32dev`/`megaatmega2560`), same
precedent as every prior `main.cpp`-only change in this project. Suite
count stays at 54.

**Non-goals, deferred:** status/activity LEDs (PCB doc §7 item 5) — closed
by sub-project #10, see the "Status/activity LEDs (sub-project #10)"
section below. The "4-channel commissioning form" item named in earlier
sub-projects' non-goals was never actually a separate gap — see the
correction in the "Boot-mode LED indicator (sub-project #5)" section's own
non-goals above. The hardware bring-up checklist (physically verifying
buttons, LEDs, and LocoNet send/receive against real hardware) also
remains outstanding, unrelated to this sub-project's scope.

### Status/activity LEDs (sub-project #10)

(2026-10-25, in this node-board effort's own numbering — not the unrelated,
older "#9a"/"#9b"/"#9c" panel-focused sub-project sequence referenced
elsewhere in this file.)

Closes the last open item in `docs/pcb/turnout-node-4ch-pcb-design.md`
§7 (item 5, "Status and activity LEDs"). Unlike every node4 sub-project
since #9, this one has no panel equivalent to mirror — the panel has no
on-board status/activity LEDs at all, only its 12 turnout LED pairs,
which is exactly the distinction the PCB doc draws ("this is also where
wireless-setup and WiFi/broker state get their own indicator **instead
of borrowing the turnout LEDs**"). Two new classes, both fully
native-tested:

- `StatusLedDriver` (`lib/McsEsp32/src/domain/`) — a new, generic
  single-GPIO LED driver with four states (`Off`/`On`/`SlowBlink`/
  `FastBlink`), mirroring `LedPairDriver`'s existing toggle-timing shape
  (re-stamping the toggle time on a state change so a transition takes
  visible effect immediately) but deliberately simpler: one color, one
  GPIO, and a genuine `Off` — `LedPairDriver`'s "exactly one color is
  always lit" invariant doesn't apply here and would have been the
  wrong class to reuse or extend. Drives the network status LED
  (GPIO 18): `src/esp32node4/main.cpp`'s `loop()` computes the desired
  state every tick — collision (highest priority) → `FastBlink`; no
  WiFi → `Off`; WiFi but no broker → `SlowBlink`; both connected → `On`
  — and calls `setState()`/`update()`.
- `ActivityTrackingTurnoutCommandPort` (`lib/McsEsp32/src/adapters/`,
  header-only, no `.cpp`) — a decorator implementing `TurnoutCommandPort`
  that forwards `send()` to a wrapped inner port while also triggering
  an injected `IdentifyModeTimer&`. Wraps `turnoutCommandPort` before it
  reaches the 4 `ToggleTurnoutStation`s — no change to
  `ToggleTurnoutStation`, `ToggleTurnoutControl`, or
  `Loco2MqttTurnoutCommandAdapter4`. This is the hexagonal-architecture
  way to observe outbound traffic at the composition-root boundary
  without touching any already-tested production class the panel also
  depends on.
- `IdentifyModeTimer` is reused **as-is, unmodified** for the activity
  LED's momentary flash (GPIO 17, 150ms) — it already provides exactly
  "trigger() stamps the clock, isActive() compares against a fixed
  duration, a second trigger() before expiry extends rather than
  stacks," with no identify-specific logic baked into its
  implementation. No new timer class was needed. The activity flash
  fires on every turnout command that reaches the decorator — i.e.
  every button press forwarded to `send()`, not every successful
  publish — or every feedback message received (one added
  `activityFlashTimer.trigger()` line inside the existing feedback-poll
  loop).

`src/esp32node4/main.cpp`'s activity-LED GPIO read is the **very last
statement in `loop()`**, after the `station.update()` loop where an
outbound send actually reaches the decorator — the same ordering
principle sub-project #9's identify wiring documents: a same-tick event
must be visible the same tick, not one tick later.

**`BootMode::WirelessSetup` needs no new branch.** `loop()`'s existing
`WirelessSetup` branch returns before it ever reaches the new network-LED
state computation or the activity-flash read, so neither piece of new
code runs at all during that mode — it is not that the computation runs
and happens to evaluate to `Off`. Both LEDs simply stay off because
`StatusLedDriver`'s initial `state_ = State::Off` default (applied via
`begin()`, which — deliberately — is called before the boot-mode check)
holds, combined with the GPIOs' own off-by-default `begin()`. The
existing 4-turnout-LED flash (sub-project #5) remains the sole
setup-mode indicator, a deliberate scope decision made during
brainstorming rather than giving these two new LEDs a redundant pattern
of their own.

**Net effect:** closes the last open item in the PCB doc's firmware
checklist (§7) — every item is now done.

**Bring-up note:** the activity LED flashes on every attempted send or
every feedback message received, not on confirmed successful MQTT
delivery — don't read a flash as proof the broker actually got the
message. `ActivityTrackingTurnoutCommandPort::send()` triggers the
timer unconditionally after forwarding to the inner port, regardless of
whether anything was actually transmitted. Concretely: a channel with
its turnout address left unconfigured (`0`) causes
`Loco2MqttTurnoutCommandAdapter4::send()` to return early without
publishing anything, yet the activity LED still flashes, since the
decorator can't see that; a button press while MQTT is disconnected
flashes the same way, since `mqttLink.publish()` on a dead client fails
silently. The network status LED's `On`/`SlowBlink`/`Off` states are
unaffected by this gap — those are computed from `wifiLink`/
`mqttLink.connected()` directly, not from traffic — only the activity
LED's "something happened" signal is this loose.

**Test coverage added:** `test/test_status_led_driver` (9 cases) and
`test/test_activity_tracking_turnout_command_port` (3 cases) — 54 → 56
suites. `src/esp32node4/main.cpp` itself has no native test target, same
precedent as every prior `main.cpp`-only change in this project.

### Loco2MQTT turnout bridge (sub-project #9a)

The ESP32 panel's turnout MQTT integration was migrated off JMRI-over-MQTT
(a computer running JMRI, bridging to turnouts over an external broker)
onto Loco2MQTT — a companion ESP32 device that bridges LocoNet to MQTT
directly, running its own on-device broker, eliminating the computer from
the loop. This is a **full replacement**, not a switchable dual-path:
`JmriTurnoutCommandAdapter`/`JmriFeedbackSource` and the
`track/turnout/<jmriName>` topic scheme are gone.
`jmri/panel_mqtt_turnout_bridge.py` was orphaned by this migration and
removed in sub-project #9c.

Each panel channel now maps to a plain LocoNet turnout address
(`NodeConfig::channelTurnoutAddresses`, `int`, 1–2048, `0` = unconfigured)
instead of a JMRI system name string. `TopicScheme::topicFor(int)`/
`stateTopicFor(int)` produce `loconet/turnout/<address>/set` (command) and
`.../state` (feedback) — Loco2MQTT's contract.
`Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource` (renamed from
the JMRI-era classes) are re-keyed accordingly; `PayloadCodec`'s
`CLOSED`/`THROWN` words are unchanged, since Loco2MQTT's payload contract
happens to match exactly. Commissioning's `turnout <n> name <jmriName>`
command became `turnout <n> address <address>` — a bare integer, so
`WebFormCommissioningAdapter` no longer needs a quote-handling bypass for
it (only `wifi` still bypasses `CommandLineParser`, since SSIDs/passwords
can contain spaces).

**Broker discovery:** Loco2MQTT advertises itself via mDNS as
`loco2mqtt.local`. A new `MdnsResolver` port (implemented by
`EspMdnsResolver` on hardware) plus `BrokerAddressResolver` application
class implement "try mDNS first, fall back to the manually-configured
`brokerHost`". `MqttLink::connect()` re-resolves on every connect attempt
(both the initial `begin()` call and every subsequent `poll()`-triggered
retry) rather than once at boot — `MqttLink` holds the
`BrokerAddressResolver&` as a constructor dependency and resolves against
a file-local `kMdnsHostname` (`"loco2mqtt"`) constant in `MqttLink.cpp`
(`main.cpp` passes `runningConfig.brokerHost` as the fallback via
`begin()`, and declares `brokerAddressResolver` before `mqttLink` since
the latter holds it by reference). This makes a Loco2MQTT IP change (e.g.
its own reboot picking up a new DHCP lease) self-heal through the
existing 5s retry-on-timer loop in `poll()`, typically within one
`BROKER_RESOLVE_INTERVAL_MS` window, without a panel power-cycle.

`EspMdnsResolver::resolveHost()` is a blocking call: it loops on `delay()`
for up to 8s waiting for WiFi association, then blocks again on
`MDNS.queryHost()`. That's tolerable once at boot but not on every 5s retry
from inside `loop()` — so `BrokerAddressResolver` throttles actual
`resolveHost()` calls to at most once per `BROKER_RESOLVE_INTERVAL_MS`
(60s, set in `main.cpp`), returning the last successfully resolved host (or
the caller's current fallback, if mDNS has never resolved) on every
throttled call in between. This bounds the loop-freeze risk to roughly once
a minute instead of every retry, while still self-healing within that
window. It does not make the underlying mDNS query itself non-blocking —
a single resolution attempt can still stall `loop()` for up to ~8s in the
worst case (WiFi flapping); moving that to a background task would be a
larger change than this throttle.

**Testability note:** `MqttLink` is `#ifdef ARDUINO`-guarded (depends on
`PubSubClient`/`WiFiClient`) and is never compiled under the `native` test
environment, so it has no native tests — this was already true of its
pre-existing retry-timer logic before this change. The one piece of new
logic this wiring depends on, `BrokerAddressResolver`'s throttling, is
fully covered by `test/test_broker_address_resolver`; the `MqttLink`
change itself is pure delegation to that already-tested class.

A manually-bumped `kFirmwareVersion` constant (`FirmwareVersion.h`) is now
shown on the commissioning web page, so a fleet of panels can be checked
against each other for "did I flash the latest firmware everywhere."

**Upgrade note:** a panel already commissioned against the old JMRI-name
field will boot with every channel showing as unconfigured after this
migration (the NVS keys are unchanged, but `Preferences::getInt()` against
a previously-string-typed key returns its default of `0`) —
`NodeConfig::validate()` still passes (wifi/broker/nodeId are untouched),
so the panel reports "ready (configured)" while every button/LED is dead.
Re-commission every channel with `turnout <n> address <address>` after
flashing.

See `docs/superpowers/specs/2026-09-06-esp32-loco2mqtt-turnout-bridge-design.md`
and `docs/superpowers/plans/2026-09-06-esp32-loco2mqtt-turnout-bridge.md`
for the full design/implementation history, including two in-flight
amendments: the plan's 12 tasks had to execute as 4 dispatches (PlatformIO
compiles `McsEsp32` as one shared library per native test target, so 9 of
the 12 were compile-coupled and had to land as one atomic commit group),
and `NvsConfigStore.cpp`'s NVS persistence layer was missing from the
original plan entirely, caught only during post-hoc verification.

**Include convention:** within a library, includes stay relative
(`../ports/X.h`). A file depending on a header from a *different* library it
needs uses a rooted include (`"ports/X.h"`) instead — 47 such includes exist
today (recounted by full `grep` as of sub-project #4, which added 3 via
`IndependentLedPairStation.h`'s `adapters/ArduinoDigitalOutput.h`,
`ports/Clock.h` and `ports/DigitalOutput.h`; the #3 recount had caught 5 of
pre-existing drift the "35" figure missed, so **re-grep, don't increment** —
this tally has drifted twice before; sub-project #10 added 3 more via its
two new files, `StatusLedDriver.h`'s `ports/Clock.h` and
`ports/DigitalOutput.h`, and `ActivityTrackingTurnoutCommandPort.h`'s
`ports/TurnoutCommandPort.h` — **this is now the third drift; re-grep, don't
increment**),
split between `McsEsp32` (42) and `McsLoconet` (5), all of them referencing
`McsCore` headers: `adapters/ArduinoDigitalOutput.h`, `domain/Turnout.h`,
`domain/Button.h`, `domain/Indicator.h`, `domain/TurnoutIndicator.h`,
`ports/Clock.h`, `ports/DigitalInput.h`, `ports/DigitalOutput.h`,
`ports/TurnoutCommandPort.h`. When recounting, note that a raw `grep` of
`McsEsp32` returns 40, not 39: `EspMdnsResolver.h`'s
`#include "ports/MdnsResolver.h"` is rooted but points at a header in its
*own* library, so it doesn't meet this paragraph's definition and is excluded
(the "36" figure already excluded it too).
Two files rootedly include an *adapter* header rather than a `domain/` or
`ports/` one — `LedPairStation.h` and (as of sub-project #4)
`IndependentLedPairStation.h`, both for
`adapters/ArduinoDigitalOutput.h`. Still safe
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
- ✅ FixedString32 (construction, truncation, equality, default/empty state, `c_str()` exposing the stored value as a null-terminated C string — added for Milestone 13's EEPROM/serial commissioning needs)
- ✅ Turnout (position, address, locking, disable)
- ✅ TurnoutCollection (registry, lookup, capacity limit)
- ✅ TurnoutService (coordination)
- ✅ TurnoutIndicator (display/clear reflecting thrown/closed position)
- ✅ TurnoutControl (button-edge commands, feedback applying to turnout/indicator)
- ✅ NullTurnoutCommandPort (no-op send is safely callable)
- ✅ Route (command sequences, capacity limit)
- ✅ RouteService (execution, capacity limit)
- ✅ RouteExecutionService (executes a route's commands one at a time directly through TurnoutCommandPort, spaced over time to avoid truncating LocoNet pulses; isRunning() reflects progress; starting a new route always replaces an in-progress one)
- ✅ MrrwaLocoNetTurnoutAdapter (command → LocoNet packet translation)
- ✅ PulsingLocoNetTransport (non-blocking solenoid pulse timing)
- ✅ LocoNetFeedbackDecoder (switch-outputs report → TurnoutFeedback translation)
- ✅ CommandLineParser (command-line tokenizing/parsing)
- ✅ CommissioningSession (commissioning command handling)
- ✅ NodeConfig (node configuration construction/validation)
- ✅ SerialCommissioningAdapter (serial line framing → ParsedCommand translation)
- ✅ TopicScheme (LocoNet address → Loco2MQTT topic naming)
- ✅ PayloadCodec (turnout position ↔ MQTT payload encoding)
- ✅ Loco2MqttTurnoutCommandAdapter (channel → Loco2MQTT topic/payload publish translation)
- ✅ Loco2MqttFeedbackSource (MQTT payload → TurnoutFeedback translation)
- ✅ Loco2MQTT command/feedback wiring integration (end-to-end command/state topic separation + real-feedback proof + 30s-heartbeat-repeat idempotence)
- ✅ BrokerAddressResolver (mDNS-first, manual-fallback broker host resolution; throttles repeat mDNS queries to once per configured interval, returning the cached resolved host — or the caller's current fallback if mDNS has never resolved — on throttled calls)
- ✅ MatrixScanner (row-cycling scan, per-row column caching, default-false before first scan)
- ✅ MatrixDigitalInput (forwards to the scanner's cached reading for its own fixed cell)
- ✅ LedPairDriver (steady green/red, blink between last-displayed color and its opposite, `begin()` re-stamping the timer and re-writing the current color)
- ✅ LedPairOutput (forwards one logical side's `set()` to the shared `LedPairDriver`)
- ✅ ToggleTurnoutStation (begin() clears the indicator; a debounced press sends the opposite of the current position; applyFeedback matching/non-matching address; clearIndicator turns both outputs off even after a real applyFeedback lit one)
- ✅ PresenceTopics (status/mac topic string construction for a given node id)
- ✅ NodeIdentityGuard (no collision before any observation; own-mac echoes never false-positive; a foreign mac latches; the latch never clears once tripped)
- ✅ MqttPresenceAnnouncer (publishes online status + own mac on the connect edge and every 30s heartbeat thereafter while connected, not retained; no republish before the interval elapses; a heartbeat publish rearms the interval rather than firing every subsequent tick; re-arms after a disconnect/reconnect cycle)
- ✅ IdentifyModeTimer (inactive before any trigger; active immediately after; expires after the fixed duration; a second trigger before expiry extends the window rather than stacking a separate one)
- ✅ LedPairDriver's identify override (activating shows a color immediately at its own fixed interval, independent of the blink interval; idempotent — a redundant activate call doesn't reset the toggle timer; deactivating restores whatever the pre-existing blink/steady state would show, not a guess)
- ✅ MegaConfig (`validate()`: in-range unique addresses pass; below-minimum, above-maximum, duplicate, and default-constructed all-zero configs fail; the min/max bounds themselves are valid)
- ✅ InMemoryConfigurationRepository (load before any save returns a default-constructed, invalid config; save-then-load round-trips the exact config; a second save overwrites the first)
- ✅ MegaCommissioningSession (falls back to compile-time defaults with nothing saved, uses a valid saved config instead; `address`/`name` stage into the draft without touching the repository, rejecting out-of-range station numbers/addresses and malformed input; `save` persists the draft and reports a repository failure; an unrecognized command is rejected and leaves the draft unchanged)

**Completed milestones:** 1-13 (foundation, ports, Button, Indicator, Turnout domain model, TurnoutIndicator, TurnoutControl, hardware integration programming, LocoNet output, LocoNet feedback, multiple turnouts, routes, persistent configuration). `pio run -e megaatmega2560` compiles successfully (RAM 26.6%, Flash 5.0% with 4 stations + routes + persistent config). Milestones 9-13 are complete on the programming side only — physical wiring and on-hardware verification (button-to-LED and LocoNet send/receive against a real DR5000/DR4018, across all 4 stations, plus the 2 route buttons, plus serial commissioning) are still outstanding — see the "Milestone 8 hardware" note below.

**Next milestones:** none — this was the last item on the Mega panel's original roadmap. Milestone 14 (Production Hardening) exists as a roadmap section but was never part of this project's active milestone sequence tracked here; pick it up as its own future effort if wanted.

### Milestone 8 hardware (not yet done)

The programming portion of Milestone 8 is complete and builds cleanly for the Mega 2560, but the hands-on portion still needs a physically connected board: `src/mega/main.cpp` now defines 4 stations (`STATION_CONFIGS[0..3]`) — pins 22/23/8/9 (throw/close/thrown-LED/closed-LED) for station 1, 24/25/10/11 for station 2, 26/27/12/13 for station 3, 28/29/14/15 for station 4. Wire at least one (ideally all 4, since Milestone 11's completion criteria calls for verifying 4), flash with `pio run -e megaatmega2560 --target upload`, and verify.

`src/mega/main.cpp` now wires the real LocoNet send and receive adapters (Milestones 9-10), not `NullTurnoutCommandPort`, so a button press does reach `LocoNet.requestSwitch()` and `TurnoutControl::applyFeedback()` is driven by real incoming `OPC_SW_REP` messages — but none of that has been exercised against real LocoNet hardware yet. "Verify behavior on hardware" for Milestone 8 realistically still means confirming button reads and LED drive work in isolation; full button-to-LED-via-LocoNet verification is Milestones 9-11's remaining hardware work, not Milestone 8's.

### Milestone 12: Routes (Mega panel)

Closes the roadmap's Milestone 12 on the programming side: the Mega panel
can now trigger a sequence of turnout commands from a single button press,
rather than only ever commanding one turnout at a time.

`RouteExecutionService` (`lib/McsCore/src/application/RouteExecutionService.h/.cpp`)
is a new application class, built fresh for this milestone rather than
reusing the ported `RouteService`/`TurnoutService`/`TurnoutCollection` trio
(from Milestone 5 — see that milestone's own status note and the roadmap
doc's Milestone 12 section): those classes are a different design that
operates against their own turnout registry and never reach
`TurnoutCommandPort`, so they don't satisfy this milestone's "execute a
route through the turnout command port" intent. `RouteExecutionService` is
the class this section originally specified. It takes a `Route` and the
shared `TurnoutCommandPort&`/`Clock&`; `start()` begins sending that route's
commands, and non-blocking `update()` sends the next command once
`ROUTE_COMMAND_SPACING_MS` (300ms) has elapsed since the last one —
required, not optional, because `PulsingLocoNetTransport` can only track one
pending solenoid pulse at a time, so firing a route's commands back-to-back
would truncate every earlier pulse's duration. Calling `start()` again
before a route finishes discards whatever commands hadn't been sent yet and
begins the new route immediately — there is no queueing of multiple
in-flight routes. `Route`/`RouteService`/`TurnoutService`/`TurnoutCollection`
are left exactly as they were — unused, not removed, not modified.

`src/mega/main.cpp` wires 2 dedicated route buttons (GPIO 30/31, same
active-low/internal-pullup convention as the existing station buttons) and
2 placeholder `Route` objects (built via `addTurnout()` calls — data, not
hard-coded conditionals), both driving a single shared
`RouteExecutionService` over the shared `TurnoutCommandPort` — one
instance, not one per route: `route1Button.wasPressed()` calls
`routeExecutionService.start(route1)`, `route2Button.wasPressed()` calls
`routeExecutionService.start(route2)` on that same instance, and `loop()`
calls `update()` on it once every tick alongside the existing 4 stations'.
This single-instance wiring is what makes the "starting a new route while
one is in progress replaces it" interrupt behavior described above work at
all — pressing one route button while the other's route is still sending
interrupts it, exactly like calling `start()` again on the same service.
The 2 routes' turnout addresses are placeholders, not yet matched to a real
layout — see `src/mega/main.cpp` for their exact values.

**Net effect:** pressing either route button now sends its route's full,
correctly-ordered, properly-spaced sequence of turnout commands through the
real LocoNet send path (once Milestone 8's hardware pass is done) — the
same path an individual station's own button already uses. Firmware still
builds cleanly for the Mega 2560 (RAM 18.1%, Flash 3.1% with 4 stations +
routes).

**Test coverage added:** `test/test_route_execution_service` (8 cases —
covering start/update sequencing, command spacing, and the
start-replaces-in-progress behavior) — 56 → 57 suites.

**Non-goals, deferred:** route status indication (a roadmap task for this
milestone, not one of its two completion criteria — left for a future
increment); failure reporting if a command in a route can't be sent; real
route data for an actual layout (today's 2 routes are placeholders); and
persistent configuration for route definitions themselves — **update:**
Milestone 13 (below) shipped persistent configuration for station
addresses/names only; route definitions were never in its scope and stay
compile-time-only, so this remains an open non-goal with no milestone
left to pick it up (Milestone 13 was the last one on the Mega panel's
roadmap).

### Milestone 13: Persistent configuration (Mega panel)

Closes the roadmap's Milestone 13, and with it the last item on the Mega
panel's original roadmap: turnout addresses and names can now be changed
over a serial connection and survive a power cycle, instead of being
fixed at compile time.

`ConfigurationRepository` (`lib/McsCore/src/ports/ConfigurationRepository.h`)
is a new port with `load()`/`save(config)`, deliberately shaped like the
ESP32 side's own `ConfigStore` (a config value in, a config value out)
rather than the roadmap doc's own original bool-returning sketch (§5.7) —
chosen for consistency with the only other persistence port in this
project rather than inventing a second shape. Two implementations exist:
`InMemoryConfigurationRepository` (a real, reusable implementation backed
by a plain in-memory `MegaConfig`, not merely a test double — it's what
every native test and `MegaCommissioningSession`'s default-fallback path
use) and `EepromConfigurationRepository` (`#ifdef ARDUINO`-guarded, same
precedent as `TurnoutStation`/`MrrwaLocoNetSwitchDriver`, so it has no
native test). The EEPROM schema is 139 bytes against the ATmega2560's
4096-byte EEPROM: a 2-byte magic (`0xBEEF`) plus a 1-byte schema version
plus the raw `MegaConfig` struct. `load()` treats a magic or version
mismatch as "no valid config" and returns a default-constructed
(all-zero, invalid) `MegaConfig` rather than whatever garbage bytes are
sitting in EEPROM — `MegaCommissioningSession`'s constructor is what
turns that into the compile-time `STATION_CONFIGS` defaults, via the same
`MegaConfig::validate()` check (in-range, non-duplicate addresses) it
uses for every other staged edit.

`MegaCommissioningSession` (`lib/McsCore/src/application/`) handles 3
commands — `address <station 1-4> <address>`, `name <station 1-4>
<name>`, `save` — against an in-memory draft seeded from
`repository.load()` at construction (falling back to the caller-supplied
defaults if that load fails `validate()`). Every `address`/`name` edit
only touches the draft; `repository.save(draft)` is called once, on the
`save` command alone — deliberately, to avoid EEPROM wear from writing on
every keystroke (the ATmega2560's EEPROM is rated for ~100k write
cycles per cell, not unlimited). This is a smaller, hand-`sscanf`-parsed
command set than the ESP32 side's `CommandLineParser`/`ParsedCommand`/
`CommissioningSession` layering — deliberately: 3 fixed commands don't
need a generic tokenizing pipeline.

**Correction made during plan-writing, not the original design doc, worth
noting the way this project notes discovered spec imprecisions in
place:** the `show` command (dumping all 4 stations' current
address/name) lives directly in `src/mega/main.cpp`'s serial-line
handling, not inside `MegaCommissioningSession` alongside `address`/
`name`/`save`. `MegaCommissioningSession::handleLine()` always returns a
single `FixedString32` — a 32-char buffer — but a 4-station dump needs
several lines' worth of text, which doesn't fit in one. Rather than
changing every command's return type to accommodate the one command that
doesn't fit, `show` reads `commissioningSession.draft()` (a const
accessor) directly and prints each station with `Serial.print()`/
`Serial.println()`, bypassing `handleLine()` entirely. `main.cpp` also
calls `Serial.begin(115200)` in `setup()` — the Mega firmware's first-ever
use of `Serial`; previously it had no serial output or input of any kind.

**Net effect:** a technician can now connect at 115200 baud, type `show`
to see the 4 stations' current configuration, change an address or name,
`save` it, and power-cycle the board — the new values persist and are
what `src/mega/main.cpp` builds its `TurnoutStation`s from at the next
boot (`buildEffectiveStationConfig()` reads
`commissioningSession.draft().stations[i]` rather than
`STATION_CONFIGS[i]` directly for address/name, keeping pin assignments
compile-time-only). Firmware still builds cleanly for the Mega 2560 (RAM
26.6%, Flash 5.0% with 4 stations + routes + persistent config).

**Test coverage added:** `test/test_mega_config` (6 cases — `validate()`'s
in-range/duplicate/bounds/default-invalid behavior),
`test/test_in_memory_configuration_repository` (3 cases — default-invalid
before any save, save-then-load round-trip, a second save overwriting the
first), `test/test_mega_commissioning_session` (11 cases — default
fallback, a valid saved config overriding the defaults, `address`/`name`
staging and their rejection paths, `save` success and repository-failure
reporting, and an unrecognized/malformed command leaving the draft
unchanged), plus one new case in the existing `test_fixed_string32` suite
(`c_str()`, needed by both the EEPROM schema and the serial `show`
command) — 57 → 60 suites.

**Non-goals, deferred:** persisting pin assignments (deliberately
excluded — see the roadmap doc's completion-criteria note); persisting
route definitions (the 2 placeholder routes in `src/mega/main.cpp` stay
compile-time); a CRC/checksum over the stored config (the magic+version
pair is the only integrity check — a bit-flip inside a structurally valid
`MegaConfig` would not be caught); a generic command-parsing pipeline
(deliberately smaller than the ESP32 side's, per the design note above);
a `Logger` class (responses go straight to `Serial.println()`, same
precedent as everywhere else in `src/mega/main.cpp`); and a `reset`-to-
defaults serial command (recovering from a bad saved config today means
re-entering `address`/`name` values by hand, or power-cycling before a
bad `save`).

## Engineering Principles (from the roadmap)

- **TDD**: write a failing native test first, implement the minimum to pass, refactor only while green.
- **Dependency inversion**: domain depends on ports, adapters depend on domain-owned interfaces — never the reverse.
- **Single responsibility**: e.g. a `Button` only reads button state, a `Turnout` only models turnout position — it must not own LEDs or call LocoNet directly.
- **Explicit state**: state changes go through methods that enforce valid transitions, not direct field mutation.
- **No blocking calls** (`delay()`) in domain/application code — `loop()` must stay non-blocking via `update()`/`poll()`.

## Shared libraries (`mcs-core`, `mcs-net`)

Code shared with other MaltBee projects lives in two public PlatformIO libraries, pinned by tag in `platformio.ini`:

- [`mcs-core`](https://github.com/MyTurnyet/mcs-core) — AVR-safe code (currently the `Clock` port).
- [`mcs-net`](https://github.com/MyTurnyet/mcs-net) — `MqttTransport`/`MdnsResolver` ports, `BrokerAddressResolver`, the `MqttLink`/`WiFiLink`/`EspMdnsResolver` adapters, and the test fakes `FakeClock`, `FakeMqttTransport`, `FakeMdnsResolver` (include them as `testing/Fake*.h`). Their tests live in the `mcs-net` repo. ESP32 environments must still list `knolleary/PubSubClient` themselves; the Mega environment does not use `mcs-net`.

Do not re-add these files under `lib/`. Change them in the library repo, tag a new version, then bump the tag here.
