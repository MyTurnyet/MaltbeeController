# MaltBee Control System (MCS)

An embedded railroad control system for model railroad panels. Three hardware targets exist:
the original Arduino Mega 2560, talking directly to a Digitrax LocoNet layout; a 12-turnout
ESP32 panel that talks to Loco2MQTT — a companion device that bridges the layout's LocoNet
bus to MQTT via its own on-device broker; and a newer 4-turnout ESP32 "node" board, a smaller
per-cluster alternative to the 12-turnout panel — fully wired in firmware as of sub-project #10
(2026-10-25): turnout stations, LEDs, buttons, wireless commissioning, real WiFi/MQTT/Loco2MQTT
send-and-receive, presence/collision detection, MQTT-triggered identify-blink, and status/activity
LEDs are all in place, with the only remaining work being
physical hardware bring-up (no hardware board exists yet; see
`docs/pcb/turnout-node-4ch-pcb-design.md` for the hardware spec). Current development is
focused on the node board; see `docs/superpowers/specs/` and `docs/superpowers/plans/` for
the full design history of every shipped sub-project.

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

**ESP32 node board (new):** started as sub-project #1's minimal placeholder — see
`CLAUDE.md`'s "4-channel node board: channel-count templating" section — but sub-projects
#2 through #10 (2026-10-21 through 2026-10-25) have since fully wired it up, and it is **no
longer a placeholder in any sense**: the 12-channel panel's commissioning classes were
templated on channel count (sub-project #1) so the `[env:esp32node4]` environment/
`src/esp32node4/main.cpp` composition root could share the same logic; turnout stations,
independent-GPIO LED pairs, and direct-GPIO buttons are wired (sub-projects #2 and #4); a
composite BOOT/SETUP-button gesture makes wireless (captive-portal) commissioning genuinely
reachable at runtime rather than merely wired-but-dormant (sub-project #3); boot-mode LED
feedback is wired (sub-project #5); real WiFi/MQTT/Loco2MQTT send-and-receive wiring is in
place, so a button press genuinely publishes a command and a real state-topic echo updates
the turnout and its LED (sub-projects #6 and #7); multi-board presence/collision detection
is wired (sub-project #8); MQTT-triggered identify-blink is wired (sub-project #9); and a
network status LED plus an activity LED are wired (sub-project #10), closing the PCB doc's
§7 firmware checklist entirely. The 4-channel commissioning web form (`WebFormCommissioningAdapter4`/
`CaptivePortalServer4`, wired and tested since sub-project #1) needs no further work either.
What remains is physical hardware bring-up — no hardware exists for this board yet (see
`docs/pcb/turnout-node-4ch-pcb-design.md` §11 for open items).

See `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md` for the Mega
panel's original complete development plan, and `CLAUDE.md` for the authoritative,
currently-maintained architecture description of all three targets.

## Requirements

- PlatformIO
- Arduino Mega 2560 (for hardware deployment on the `megaatmega2560` environment)
- ESP32-WROOM-32 dev board, e.g. an ELEGOO ESP32 development board (for the `esp32dev`
  12-channel panel environment — see `docs/ESP32_Turnout_Panel_Implementation.md` — or the
  `esp32node4` 4-channel node environment, same dev board; no hardware build exists for the
  node board yet, so `esp32node4` is native/build-only for now)
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

# Build firmware for the ESP32 node board (no hardware yet - build-only)
pio run -e esp32node4

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
- **ESP32 node board:** sub-projects #2 through #10 (independent-output turnout indicator,
  direct-GPIO buttons, composite setup trigger, turnout-station wiring, boot-mode LED
  feedback, Loco2MQTT channel-count templating, real WiFi/MQTT/Loco2MQTT wiring,
  presence/collision detection, the identify-overlay, and status/activity LEDs) have all
  landed — see `CLAUDE.md`'s per-sub-project sections for each. The 4-channel commissioning
  web form has needed no further work since sub-project #1 (an earlier "remains its own
  future sub-project" note here was stale — the form was already wired and tested). What
  remains is physical hardware bring-up once a board exists (see
  `docs/pcb/turnout-node-4ch-pcb-design.md` §11).
