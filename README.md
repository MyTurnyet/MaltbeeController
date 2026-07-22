# MaltBee Control System (MCS)

An embedded railroad control system for model railroad panels. The original target is an
Arduino Mega 2560 talking to a Digitrax LocoNet layout; a second panel type, an ESP32-based
panel that talks to JMRI over Wi-Fi, is in planning (see `docs/`).

Built with professional software engineering practices: Test-Driven Development, Hexagonal
Architecture, and Dependency Inversion. The core control logic is testable on a desktop
computer, independent of any specific hardware target — that's also what lets the same
`lib/McsCore` domain/application code be shared across the Mega and (soon) ESP32 environments.

## Current Status

The project has completed Milestones 1-8, and Milestones 9-11 (LocoNet output, LocoNet
feedback, and multiple turnouts) are complete on the programming side:

- ✅ Native test environment with Catch2
- ✅ Digital I/O ports (DigitalInput, DigitalOutput, Clock)
- ✅ Domain classes: Button, Indicator, Turnout, TurnoutCollection, TurnoutIndicator, Route, RouteService
- ✅ Service classes: TurnoutService, RouteService
- ✅ Application layer: TurnoutControl (Milestone 7)
- ✅ Hardware integration: Arduino GPIO adapters and composition root (Milestone 8; physical wiring/on-hardware verification still outstanding)
- ✅ Test doubles for all ports (FakeDigitalInput, FakeDigitalOutput, FakeClock, FakeTurnoutCommandPort, FakeLocoNetTransport, FakeLocoNetSwitchDriver)
- 🚧 LocoNet output (Milestone 9): translation and pulse-timing adapters implemented and natively tested, wired into `main.cpp`, firmware builds for the Mega 2560. Electrical LocoNet interface verification and on-hardware DR5000/DR4018 confirmation still outstanding.
- 🚧 LocoNet feedback (Milestone 10): decode adapter and receive-side port implemented and natively tested, wired into `main.cpp`, firmware builds for the Mega 2560. Blocked on the same on-hardware verification as Milestone 9.
- 🚧 Multiple turnouts (Milestone 11): `TurnoutConfig` + `TurnoutStation` replace the single hand-declared turnout with a 4-entry, config-table-driven composition root. Blocked on the same on-hardware verification as Milestones 9-10.

**Next:** verify the electrical LocoNet interface and confirm DR5000/DR4018 behavior on
hardware across all 4 stations (send and feedback), then routes (Milestone 12).
In parallel, an ESP32 panel (Wi-Fi to JMRI, 12 turnouts per board) is being planned as a
second PlatformIO environment — see `docs/ESP32_Turnout_Panel_Implementation.md` and
`docs/Refactoring_Recommendations_Multi_Hardware.md`.

See `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md` for
the complete development plan.

## Requirements

- PlatformIO
- Arduino Mega 2560 (for hardware deployment on the existing `megaatmega2560` environment)
- ESP32-WROOM-32 dev board (planned `esp32dev` environment — not yet added, see
  `docs/ESP32_Turnout_Panel_Implementation.md`)
- C++17 compiler (for native tests)

## Building and Testing

```bash
# Run all native unit tests
pio test -e native

# Run a specific test suite
pio test -e native -f test_turnout

# Build firmware for Arduino Mega
pio run -e megaatmega2560

# Upload to Arduino Mega
pio run -e megaatmega2560 --target upload

# Serial monitor
pio device monitor
```

## Architecture

This project uses **Hexagonal Architecture** (Ports & Adapters):

- **Domain Layer:** Pure C++ business logic (Turnout, Button, Route, etc.)
- **Ports:** Interfaces defining what the domain needs (DigitalInput, Clock, etc.)
- **Adapters:** Hardware implementations (ArduinoDigitalInput, MrrwaLocoNetAdapter, etc.)
- **Application Layer:** Use cases and coordination (TurnoutControl, RouteExecutionService)

The Arduino is treated as an implementation detail behind the domain layer, not the center
of the design. Domain and application layers compile and run natively (no Arduino required)
for fast test feedback. This is also what makes a second hardware target additive rather
than a rewrite: the planned ESP32 environment reuses `lib/McsCore`'s domain and application
layers unchanged and only needs new adapters (button-matrix input, shared-GPIO LED pairs,
Wi-Fi/JMRI transport) behind the existing ports.

## Project History

This project merged two prior efforts on 2026-07-13:
- **MaltbeeController** — architectural foundation with ports/adapters
- **MaltBee-Control-System** — domain logic for turnouts and routes

See `internal_documents/archive/original-overview.md` for historical context.

## Development Principles

- **Test-Driven Development**: Write tests first, implement to pass
- **Dependency Inversion**: Domain depends on ports, not concrete hardware
- **Single Responsibility**: Each class has one clear purpose
- **Explicit State**: State changes through methods, not direct mutation
- **No blocking calls**: `loop()` must stay non-blocking via `update()`/`poll()`

## Next Steps

- Verify the electrical LocoNet interface and confirm DR5000/DR4018 behavior on hardware across all 4 stations, both send and feedback (finish Milestones 9-11)
- Add routes (Milestone 12)
- Add the `esp32dev` PlatformIO environment and ESP32 panel support — see
  `docs/ESP32_Turnout_Panel_Implementation.md` for the milestone plan and
  `docs/Refactoring_Recommendations_Multi_Hardware.md` for prerequisite refactoring
