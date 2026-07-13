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
- **`main.cpp` is the composition root only** — it defines pin assignments, constructs adapters/domain objects/application services, calls `begin()` in `setup()`, and calls non-blocking `update()`/`poll()` methods in `loop()`. It must not contain business logic. (Note: the current `main.cpp` is a placeholder blink sketch and has not yet been rebuilt on this architecture — see the roadmap's Milestone 1.)

### Current source layout

- `lib/McsCore/src/domain/` — domain classes
  - Button, Indicator (I/O primitives)
  - Turnout, TurnoutCollection, TurnoutService (turnout model and coordination)
  - Route, RouteService (route sequences and execution)
- `lib/McsCore/src/ports/` — port interfaces (DigitalInput, DigitalOutput, Clock)
- `src/main.cpp` — composition root (Arduino entry point, currently placeholder)
- `test/test_<name>/test_main.cpp` — Catch2 test binaries (7 test suites)
- `test/support/` — test doubles (FakeDigitalInput, FakeClock, FakeDigitalOutput)

**Test coverage (all Catch2, all passing):**
- ✅ Button (debouncing, edge detection)
- ✅ Indicator (on/off control)
- ✅ Turnout (position, address, locking, disable)
- ✅ TurnoutCollection (registry, lookup)
- ✅ TurnoutService (coordination)
- ✅ Route (command sequences)
- ✅ RouteService (execution)

**Completed milestones:** 1-5 (foundation, ports, Button, Indicator, Turnout domain model)

**Next milestones:** TurnoutIndicator (6), TurnoutControl (7), hardware integration (8), LocoNet (9+)

## Engineering Principles (from the roadmap)

- **TDD**: write a failing native test first, implement the minimum to pass, refactor only while green.
- **Dependency inversion**: domain depends on ports, adapters depend on domain-owned interfaces — never the reverse.
- **Single responsibility**: e.g. a `Button` only reads button state, a `Turnout` only models turnout position — it must not own LEDs or call LocoNet directly.
- **Explicit state**: state changes go through methods that enforce valid transitions, not direct field mutation.
- **No blocking calls** (`delay()`) in domain/application code — `loop()` must stay non-blocking via `update()`/`poll()`.
