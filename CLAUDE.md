# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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

- `lib/McsCore/src/domain/` — domain classes
  - Button, Indicator (I/O primitives)
  - FixedString32 (fixed 32-char buffer, replaces std::string for AVR compatibility)
  - Turnout, TurnoutCollection, TurnoutService (turnout model and coordination)
  - TurnoutIndicator (displays turnout position via thrown/closed indicators)
  - Route, RouteService (route sequences and execution)
- `lib/McsCore/src/application/` — application/use-case classes
  - TurnoutControl (wires buttons, turnout, indicator, and TurnoutCommandPort together)
- `lib/McsCore/src/ports/` — port interfaces (DigitalInput, DigitalOutput, Clock, TurnoutCommandPort, LocoNetTransport, LocoNetSwitchDriver, LocoNetFeedbackSource)
- `lib/McsCore/src/adapters/` — generic Arduino GPIO adapters (ArduinoDigitalInput/Output, ArduinoClock — guarded with `#ifdef ARDUINO` so they don't break the native build) and LocoNet adapters: `MrrwaLocoNetTurnoutAdapter` and `LocoNetFeedbackDecoder` (native-tested translation layers, no Arduino dependency) plus `MrrwaLocoNetSwitchDriver`, `PulsingLocoNetTransport`, and `MrrwaLocoNetFeedbackSource` (the send/receive hardware shims and pulse-timing logic); `NullTurnoutCommandPort` remains as a no-op fallback
- `src/main.cpp` — composition root: wires one throw/close button, one thrown/closed indicator, one Turnout, one TurnoutControl, and the real LocoNet send/receive adapter chain (`MrrwaLocoNetSwitchDriver` → `PulsingLocoNetTransport` → `MrrwaLocoNetTurnoutAdapter` for sending; `MrrwaLocoNetFeedbackSource` → `LocoNetFeedbackDecoder` → `TurnoutControl::applyFeedback()` for receiving)
- `test/test_<name>/test_main.cpp` — Catch2 test binaries (14 test suites)
- `test/support/` — test doubles (FakeDigitalInput, FakeClock, FakeDigitalOutput, FakeTurnoutCommandPort, FakeLocoNetTransport, FakeLocoNetSwitchDriver)

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

**Completed milestones:** 1-10 (foundation, ports, Button, Indicator, Turnout domain model, TurnoutIndicator, TurnoutControl, hardware integration programming, LocoNet output, LocoNet feedback). `pio run -e megaatmega2560` compiles successfully. Milestones 9-10 are complete on the programming side only — physical wiring and on-hardware verification (both button-to-LED and LocoNet send/receive against a real DR5000/DR4018) are still outstanding — see the "Milestone 8 hardware" note below.

**Next milestones:** multiple turnouts (11), routes (12), persistent configuration (13)

### Milestone 8 hardware (not yet done)

The programming portion of Milestone 8 is complete and builds cleanly for the Mega 2560, but the hands-on portion still needs a physically connected board: wire one throw button (pin 22), one close button (pin 23), one thrown-position LED (pin 8), one closed-position LED (pin 9), flash with `pio run -e megaatmega2560 --target upload`, and verify.

`main.cpp` now wires the real LocoNet send and receive adapters (Milestones 9-10), not `NullTurnoutCommandPort`, so a button press does reach `LocoNet.requestSwitch()` and `TurnoutControl::applyFeedback()` is driven by real incoming `OPC_SW_REP` messages — but none of that has been exercised against real LocoNet hardware yet. "Verify behavior on hardware" for Milestone 8 realistically still means confirming button reads and LED drive work in isolation; full button-to-LED-via-LocoNet verification is Milestones 9-10's remaining hardware work, not Milestone 8's.

## Engineering Principles (from the roadmap)

- **TDD**: write a failing native test first, implement the minimum to pass, refactor only while green.
- **Dependency inversion**: domain depends on ports, adapters depend on domain-owned interfaces — never the reverse.
- **Single responsibility**: e.g. a `Button` only reads button state, a `Turnout` only models turnout position — it must not own LEDs or call LocoNet directly.
- **Explicit state**: state changes go through methods that enforce valid transitions, not direct field mutation.
- **No blocking calls** (`delay()`) in domain/application code — `loop()` must stay non-blocking via `update()`/`poll()`.
