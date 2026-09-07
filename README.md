# MaltBee Control System (MCS)

An embedded railroad control system for model railroad panels. Two hardware targets exist:
the original Arduino Mega 2560, talking directly to a Digitrax LocoNet layout, and an
ESP32-based panel that talks to Loco2MQTT — a companion device that bridges the layout's
LocoNet bus to MQTT via its own on-device broker. Current development is focused on the
ESP32 panel; see `docs/superpowers/specs/` and `docs/superpowers/plans/` for the full design
history of every shipped sub-project.

Built with professional software engineering practices: Test-Driven Development, Hexagonal
Architecture, and Dependency Inversion. The core control logic is testable on a desktop
computer, independent of any specific hardware target — that's also what lets the same
`lib/McsCore` domain/application code be shared across both the Mega and ESP32 environments.

## Current Status

**Mega 2560 panel:** Milestones 1-11 are complete on the programming side (native test
harness, I/O ports, domain layer, hardware integration, LocoNet output, LocoNet feedback,
multiple turnouts). Physical wiring and on-hardware verification (button-to-LED and LocoNet
send/receive against a real DR5000/DR4018, across all 4 stations) are still outstanding —
see `CLAUDE.md`'s "Milestone 8 hardware" section. Routes (Milestone 12) and persistent
configuration (Milestone 13) are not yet started.

**ESP32 panel:** shipped and in active development. 12-turnout button-matrix panels talk
directly to Loco2MQTT — see `CLAUDE.md`'s "Loco2MQTT turnout bridge" section for the full
contract. Also shipped: wireless commissioning via a captive-portal web form, bench-serial
commissioning, multi-panel presence/collision detection (a 30-second MQTT heartbeat), and
MQTT-triggered identify-blink for locating a specific physical panel. Two real boards have
been built and commissioned, though that hardware verification predates the migration to
Loco2MQTT and needs re-running against it — see `docs/HARDWARE_BRINGUP_CHECKLIST.md` Part 2
for current status.

See `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md` for the Mega
panel's original complete development plan, and `CLAUDE.md` for the authoritative,
currently-maintained architecture description of both panels.

## Requirements

- PlatformIO
- Arduino Mega 2560 (for hardware deployment on the `megaatmega2560` environment)
- ESP32-WROOM-32 dev board, e.g. an ELEGOO ESP32 development board (for the `esp32dev`
  environment — see `docs/ESP32_Turnout_Panel_Implementation.md`)
- A Loco2MQTT device reachable on the same WiFi network, for the ESP32 panel to talk to
- C++17 compiler (for native tests)

## Building and Testing

```bash
# Run all native unit tests
pio test -e native

# Run a specific test suite
pio test -e native -f test_turnout

# Build firmware for Arduino Mega
pio run -e megaatmega2560
pio run -e megaatmega2560 --target upload

# Build firmware for the ESP32 panel
pio run -e esp32dev
pio run -e esp32dev --target upload

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
for fast test feedback. This is also what made the ESP32 environment additive rather than a
rewrite: it reuses `lib/McsCore`'s domain and application layers unchanged and only needed
new adapters (button-matrix input, shared-GPIO LED pairs, Wi-Fi/MQTT transport) behind the
existing ports — see `CLAUDE.md` for the full current source layout.

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

- **Mega 2560 panel:** verify the electrical LocoNet interface and confirm DR5000/DR4018
  behavior on hardware across all 4 stations, both send and feedback (finish Milestone 8's
  hardware pass); then routes (Milestone 12) and persistent configuration (Milestone 13).
- **ESP32 panel:** re-verify hardware bring-up against the current Loco2MQTT-based
  procedure — see `docs/HARDWARE_BRINGUP_CHECKLIST.md` Part 2 for outstanding items.
