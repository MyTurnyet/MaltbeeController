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
  - `domain/`: CommandLineParser, NodeConfig, ParsedCommand, TopicScheme (JMRI/MQTT topic naming), PayloadCodec (turnout position ↔ MQTT payload encoding), MatrixScanner (cycles the 3 row outputs one at a time, caching each column's reading per row for the 3x4 button matrix), LedPairDriver (drives a shared-GPIO red/green LED pair: steady color or non-blocking blink between last-displayed color and its opposite), BootMode (enum: Normal/NeedsCommissioning/WirelessSetup — transient boot-time state, not part of persisted `NodeConfig`), BootModeSelector (picks `BootMode` from `NodeConfig::validate()` plus a pending wireless-setup request, request always wins), MacAddress (6-byte value type, `lastFourHexDigits()`), SetupApName (builds a WiFi AP name, `"MaltBee-Setup-" + lastFourHexDigits()`, from a `MacAddress`), WebFormSubmission (plain data struct: wifi/broker/id/12 channel JMRI names, as submitted by the wireless-commissioning web form), SetupFormRenderer (pure HTML generation/escaping for that form, served by `CaptivePortalServer`; the WiFi-password field always renders blank — never the real stored password — and a blank submission keeps the current password, the opposite blank-semantics from turnout channel names, which genuinely clear on a blank submission)
  - `ports/`: ConfigStore, UartPort, MqttTransport, SetupModeRequestStore (a one-shot "enter wireless setup on next boot" flag, separate from `ConfigStore` since it's boot-intent, not configuration)
  - `application/`: CommissioningSession (commissioning command handling, wires a `ConfigStore` port; gained a read-only `draft()` accessor in sub-project #2c-b1 so `WebFormCommissioningAdapter` can pre-fill the web form)
  - `adapters/`: SerialCommissioningAdapter, JmriTurnoutCommandAdapter, JmriFeedbackSource, WebFormCommissioningAdapter (translates a `WebFormSubmission` into `CommissioningSession` commands — deliberately bypasses `CommandLineParser` for the `wifi`/`turnout N name X` commands by constructing `ParsedCommand` directly, since `CommandLineParser` has no quote-handling and those two commands' string values could contain spaces) (native-tested translation layers, no Arduino dependency); EspUartPort, NvsConfigStore, WiFiLink, MqttLink, NvsSetupModeRequestStore, EspDeviceIdentity (reads the real ESP32 MAC via `esp_efuse_mac_get_default()`), CaptivePortalServer (`#ifdef ARDUINO`-guarded hardware shims; opens a WiFi AP + `DNSServer` (all queries redirected to the AP's own IP) + `WebServer` serving `SetupFormRenderer`'s page; `begin(apName, passphrase)` requires a WPA2 passphrase to join, wired in `src/esp32/main.cpp` (sub-project #2c-b2) to a fixed passphrase documented in `docs/ESP32_Turnout_Panel_Implementation.md`; `NvsSetupModeRequestStore` persists its flag in its own NVS namespace `"mcs-boot"`, distinct from `NvsConfigStore`'s `"mcsnode"`, and its `requestOnNextBoot()`/`consumeRequest()` both return `bool` and check their own NVS calls, so a persistence failure doesn't get silently swallowed or misreported); MatrixDigitalInput (implements `DigitalInput` for one fixed matrix cell, forwarding to `MatrixScanner`'s cached reading); LedPairOutput (implements `DigitalOutput` for one logical side, green or red, of a shared LED pair, forwarding to `LedPairDriver`); LedPairStation (`#ifdef ARDUINO`-guarded, config-driven per-turnout composition helper — owns one turnout's real LED-pair GPIO plus its `LedPairDriver`/two `LedPairOutput`s, mirroring `TurnoutStation`); ToggleTurnoutStation (portable, no `#ifdef ARDUINO` guard — config-driven per-turnout composition helper for the ESP32's single-button panel, owning one turnout's `Button`/`Turnout`/`TurnoutIndicator`/`ToggleTurnoutControl` stack behind ports only, mirroring `TurnoutStation`/`LedPairStation` but without any raw-GPIO construction of its own); GatedDigitalInput (implements `DigitalInput` by wrapping another and forcing `isActive()` false while `setSuppressed(true)`, otherwise forwarding — general-purpose, no knowledge of its caller); ComboSetupModeTrigger (detects two `DigitalInput`s held simultaneously for a minimum duration then either releasing, edge-triggered like `Button::wasPressed()`; wired in `src/esp32/main.cpp` against the raw T1/T2 `MatrixDigitalInput`s in sub-project #2c-b2 — a documented, deliberately-accepted gap remains where a human's staggered two-finger press can let the earlier-pressed button fire one ordinary toggle command before suppression engages, see the design spec's "Known gap" section)
- `src/mega/main.cpp` and `src/esp32/main.cpp` — the two composition roots. `src/mega/main.cpp` builds a `TurnoutConfig[4]` table and 4 `TurnoutStation`s from it (range-`for` over `stations[]` in `setup()`/`loop()`, no per-turnout duplication), plus the shared real LocoNet send/receive adapter chain (`MrrwaLocoNetSwitchDriver` → `PulsingLocoNetTransport` → `MrrwaLocoNetTurnoutAdapter` for sending; `MrrwaLocoNetFeedbackSource` → `LocoNetFeedbackDecoder` → broadcast `TurnoutStation::applyFeedback()` for receiving, each station's own `TurnoutControl` self-filtering by address). `src/esp32/main.cpp` builds a `TurnoutPanelConfig[12]` table and 12 `ToggleTurnoutStation`s from it, plus a shared `MatrixScanner` (3 row/4 column GPIOs) feeding 12 `MatrixDigitalInput`s (each wrapped in a `GatedDigitalInput` before being handed to its station, so a held T1+T2 combo can suppress those two buttons' normal toggle command), 12 `LedPairStation`s (one real LED-pair GPIO each), the shared JMRI/MQTT command+feedback adapters (`JmriTurnoutCommandAdapter`/`JmriFeedbackSource` over a `WiFiLink`+`MqttLink` pair), bench-serial commissioning (`EspUartPort` → `SerialCommissioningAdapter` → `CommissioningSession` → `NvsConfigStore`), and (sub-project #2c-b2) wireless commissioning (`WebFormCommissioningAdapter` wrapping that same `CommissioningSession`, served by a `CaptivePortalServer`). Boot computes a `BootMode` via `BootModeSelector::select()` — from `NodeConfig::validate()` plus a `NvsSetupModeRequestStore::consumeRequest()` one-shot flag, read exactly once at global-init time. In `BootMode::WirelessSetup`, `setup()`/`loop()` open the captive portal (`EspDeviceIdentity::mac()` → `SetupApName::from()` builds the AP name, joined with a fixed WPA2 passphrase) and skip the WiFi-station/MQTT/JMRI/matrix/trigger/LED/station machinery entirely for that boot — every one of those objects still exists as an unconditionally-constructed global (no lazy/heap construction; only `begin()`/`poll()`/`update()` calls are gated, since C++ global constructors can't be skipped based on a runtime decision anyway). `BootMode::Normal` and `BootMode::NeedsCommissioning` share this same non-WirelessSetup code path, still distinguished only by the pre-existing `configValid` gate. In that path, a `ComboSetupModeTrigger` reads the *raw* T1/T2 `MatrixDigitalInput`s every `loop()` tick (never the gated wrappers — wiring it to gated inputs would deadlock the gesture) and, on a 3-second hold-then-release, calls `NvsSetupModeRequestStore::requestOnNextBoot()` and `ESP.restart()` — but only if that call returned `true`; a persistence failure logs a message and leaves the panel running normally instead of rebooting into a mode it might immediately revert out of. Boot explicitly gates Wi-Fi/MQTT connection attempts on `NodeConfig::validate().empty()`; `loop()` reverts every station's LED to blink/unconfirmed via `clearIndicator()` whenever the config is invalid or MQTT is disconnected. An `NvsBootstrap` global (the very first global constructed in the file) calls `nvs_flash_init()` before anything else touches NVS — ESP-IDF runs C++ global constructors before `app_main()`/`initArduino()`, so without this, `NvsConfigStore::load()` (and now `NvsSetupModeRequestStore::consumeRequest()`, declared right after it) in a later global's initializer would silently see an uninitialized NVS partition and always return factory-default/no-request state.
- `test/test_<name>/test_main.cpp` — Catch2 test binaries (36 test suites)
- `test/support/` — test doubles (FakeDigitalInput, FakeClock, FakeDigitalOutput, FakeTurnoutCommandPort, FakeLocoNetTransport, FakeLocoNetSwitchDriver, FakeSetupModeRequestStore)

### Wireless setup: fully wired (sub-project #2c-b2)

Sub-project #2c-b2 wired every class from #2c-a (`BootMode`,
`BootModeSelector`, `SetupModeRequestStore`/`NvsSetupModeRequestStore`,
`GatedDigitalInput`, `ComboSetupModeTrigger`) and #2c-b1 (`MacAddress`,
`SetupApName`, `EspDeviceIdentity`, `WebFormSubmission`,
`WebFormCommissioningAdapter`, `SetupFormRenderer`, `CaptivePortalServer`)
into `src/esp32/main.cpp` — see the `src/esp32/main.cpp` bullet above for
how. It also resolved both open design gaps #2c-b1's final review had
deferred here:
- The web form's blank-channel copy and behavior now agree — a blank
  channel field genuinely clears that channel's stored name
  (`WebFormCommissioningAdapter::submit()` no longer skips it).
- `CaptivePortalServer`'s AP now requires a WPA2 passphrase to join
  (documented in `docs/ESP32_Turnout_Panel_Implementation.md`), and the web
  form never renders the real stored WiFi password back into its HTML — a
  blank password field on submission keeps the current password instead of
  clearing it (the opposite blank-semantics from channel names, deliberately).

One gap remains open by explicit, documented decision rather than
oversight: `docs/superpowers/specs/2026-08-29-esp32-wireless-setup-trigger-design.md`'s
staggered-press gap — a human's two-finger T1+T2 press can let the
earlier-pressed button fire one ordinary toggle command before the combo's
suppression engages. #2c-b2 chose to accept this (self-correcting, rare,
avoids adding latency/complexity to every other button's normal press)
rather than close it with a hold-off timer or a command-on-release change.

**Include convention:** within a library, includes stay relative
(`../ports/X.h`). A file depending on a header from a *different* library it
needs uses a rooted include (`"ports/X.h"`) instead — 34 such includes exist
today (reconciled via a full `grep` recount as of sub-project #2c-a; the
previous "19" figure had drifted uncorrected across sub-projects #7a and
#7b, which each added rooted-including files — `ToggleTurnoutControl.h`,
`ToggleTurnoutStation.h` — without this paragraph's tally being updated),
split between `McsEsp32` (29) and `McsLoconet` (5), all of them referencing
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
