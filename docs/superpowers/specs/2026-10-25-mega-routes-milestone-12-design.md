# Mega Panel Routes (Milestone 12) — Design

**Status:** Approved

## Problem

The project's roadmap (`internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`)
lists Milestone 12 ("Add Routes") as the next unbuilt feature for the Mega
2560 / LocoNet panel, following completed Milestones 1-11 (domain layer,
`TurnoutControl`, hardware integration, LocoNet send/receive, multiple
turnouts — all done and native-tested). Its stated completion criteria:

- A route button sends all required turnout commands.
- Routes are defined as data rather than hard-coded conditionals.

## Investigation: the ported `Route`/`RouteService` don't fit

Before designing anything new, the existing `Route`/`RouteService`/
`TurnoutService`/`TurnoutCollection` classes were examined, since CLAUDE.md's
test-coverage checklist already lists `Route` and `RouteService` as
"✅ complete." Reading them revealed they don't satisfy Milestone 12 as-is:

- The roadmap's own §6.7 specifies a class called `RouteExecutionService` —
  "execute a route through the turnout command port," with a single
  `execute(const Route&)` method.
- What actually exists is a different class, `RouteService`, built against a
  different design. Its own `activateRoute()` calls
  `turnoutService.throwStraight()`/`throwDiverging()`, which only mutates an
  in-memory `Turnout` inside a `TurnoutCollection` — it never touches
  `TurnoutCommandPort` and so never sends a real LocoNet command. The
  roadmap's own Milestone 5 status note explains why: `Route`, `RouteService`,
  `TurnoutService`, and `TurnoutCollection` were **ported wholesale from an
  earlier, separate project** ("MaltBee-Control-System", 2026-07-13), not
  built against this project's own hardware-integration path.
- Confirmed by reading `src/mega/main.cpp`: it never constructs a
  `TurnoutCollection` or `TurnoutService` at all. The real turnout state for
  each of the 4 wired stations lives inside `TurnoutStation`'s own private
  `Turnout turnout_` member, driven by `TurnoutControl` straight to
  `TurnoutCommandPort`. These are two separate, currently-disconnected models
  of "what a turnout is" — `RouteService`'s model was never wired to
  production hardware.
- Confirmed by reading `TurnoutControl.cpp`: the real button-driven path
  never checks a turnout's `locked()`/`disabled()` state either, so
  `RouteService`'s lock/disable safety checks aren't an active production
  feature today regardless of what Routes does.

**Decision (confirmed with Paige):** build the roadmap's originally-intended
`RouteExecutionService`, operating directly through `TurnoutCommandPort` —
the same path every existing button press already uses. `Route`,
`RouteService`, `TurnoutService`, and `TurnoutCollection` are left exactly as
they are: unused library code, not touched, not removed. Whether to later
deprecate or repurpose them is a separate housekeeping question, out of
scope here.

## Investigation: `PulsingLocoNetTransport` can't handle a burst of sends

`PulsingLocoNetTransport` (the class that actually pulses the LocoNet relay
for a configured duration, `LOCONET_PULSE_DURATION_MS = 250` in
`src/mega/main.cpp`) was read in full. It tracks exactly **one** pending
pulse at a time:

```cpp
void PulsingLocoNetTransport::sendPacket(const LocoNetPacket& packet)
{
    if (releasePending_)
    {
        releasePendingSwitch();
    }
    driver_.requestSwitch(packet.address, packet.position, true);
    releasePending_ = true;
    // ... schedules release at now + pulseDurationMs_
}
```

If `sendPacket()` is called again before the previous pulse's full duration
has elapsed, it immediately force-releases the previous pending switch early
to start the new one. A route that fired several turnout commands back-to-back
in the same tick would truncate every earlier pulse down to however many
milliseconds passed before the next call — likely insufficient to reliably
throw a solenoid.

**Decision:** `RouteExecutionService` cannot be a simple loop calling `send()`
N times, as the roadmap's original 2-line sketch implied. It must be a small
non-blocking state machine with its own `update()`, holding the active
route's command list and advancing to the next command only after a spacing
interval has elapsed since the last one — the same pattern
`PulsingLocoNetTransport` itself already uses. This directly satisfies the
roadmap's "Add optional command spacing if the layout requires it" task; it
isn't optional once the pulse-truncation hazard is understood, so it folds
into the base design rather than being a later add-on.
`PulsingLocoNetTransport` itself is not modified — it's already native-tested,
already hardware-verified (Milestone 9), and the fix belongs on the caller
side.

## Decision

### `RouteExecutionService` (new, `lib/McsCore/src/application/`)

```cpp
#pragma once

#include "../domain/Route.h"
#include "../ports/Clock.h"
#include "../ports/TurnoutCommandPort.h"

class RouteExecutionService
{
public:
    RouteExecutionService(TurnoutCommandPort& commandPort, Clock& clock, unsigned long commandSpacingMs);

    void start(const Route& route);
    void update();
    [[nodiscard]] bool isRunning() const;

private:
    TurnoutCommandPort& commandPort_;
    Clock& clock_;
    unsigned long commandSpacingMs_;

    const Route* activeRoute_ = nullptr;
    int nextCommandIndex_ = 0;
    unsigned long nextSendAtMs_ = 0;
};
```

Behavior:

- **`start(route)`:** if `route.getTurnoutCount() == 0`, does nothing —
  `activeRoute_` stays `nullptr`, satisfying "does nothing harmful for an
  empty route." Otherwise, stores `&route` (by pointer — `Route` is a
  ~550-600 byte fixed-size struct with a 64-command array; routes live as
  long-lived globals in `main.cpp`, so copying would waste RAM for no
  benefit), sends `route.commandAt(0)` immediately via `commandPort_.send()`,
  sets `nextCommandIndex_ = 1`, and — if more commands remain — schedules
  `nextSendAtMs_ = clock_.nowMilliseconds() + commandSpacingMs_`. If the route
  has exactly 1 command, `activeRoute_` is cleared immediately after sending
  it (nothing left to schedule).
- **`update()`:** no-op unless `activeRoute_ != nullptr` and
  `nextCommandIndex_ < activeRoute_->getTurnoutCount()` and
  `clock_.nowMilliseconds() >= nextSendAtMs_`. When all three hold, sends
  `activeRoute_->commandAt(nextCommandIndex_)`, increments
  `nextCommandIndex_`, and either reschedules `nextSendAtMs_` (more commands
  remain) or clears `activeRoute_` (route complete).
- **`isRunning()`:** `activeRoute_ != nullptr` — lets a caller (or a future
  status LED, deliberately deferred — see Non-goals) observe progress.
- **Interrupt semantics:** calling `start()` again — the same route or a
  different one — always replaces whatever was in progress mid-sequence.
  There's no queueing or locking. This matches how a fresh button press
  already overrides in-flight state everywhere else in this panel (e.g.
  `TurnoutControl` has no concept of "busy").
- **No failure reporting:** `TurnoutCommandPort::send()` returns `void` — no
  failure signal exists anywhere in this codebase today, including every
  existing button-press path. `RouteExecutionService` doesn't invent one.

### `src/mega/main.cpp` wiring

New constant alongside `LOCONET_PULSE_DURATION_MS`:

```cpp
constexpr unsigned long ROUTE_COMMAND_SPACING_MS = 300;
```

(a 50ms margin over the existing 250ms pulse duration — adjustable on the
bench without any code restructuring if real hardware needs more margin).

2 new route-trigger buttons, wired directly with the existing `Button`/
`ArduinoDigitalInput` pair (no new composition-helper class — there's no LED
to bundle alongside them this time, unlike `TurnoutStation`), on GPIO pins 30
and 31 (continuing the existing stations' 22-29 sequence):

```cpp
ArduinoDigitalInput route1ButtonInput(30, /*activeLow=*/true);
ArduinoDigitalInput route2ButtonInput(31, /*activeLow=*/true);
Button route1Button(route1ButtonInput, clock);
Button route2Button(route2ButtonInput, clock);
```

2 placeholder routes, clearly commented as examples using the existing 4
station addresses (101-104), to be replaced with real route data once known
(and, eventually, loaded from Milestone 13's persistent config instead of
hard-coded):

```cpp
// Placeholder routes — replace with real route definitions for your layout.
Route route1(1, "Example Route 1");
Route route2(2, "Example Route 2");

// (populated in setup() via route1.addTurnout(...)/route2.addTurnout(...),
// e.g. route1: 101 -> Thrown, 102 -> Closed; route2: 103 -> Closed, 104 -> Closed)

RouteExecutionService routeExecutionService(turnoutCommandPort, clock, ROUTE_COMMAND_SPACING_MS);
```

`loop()` gains, alongside the existing station-update loop:

```cpp
if (route1Button.wasPressed())
{
    routeExecutionService.start(route1);
}
if (route2Button.wasPressed())
{
    routeExecutionService.start(route2);
}
routeExecutionService.update();
```

### Testing

New native suite `test/test_route_execution_service/test_main.cpp`, using
`FakeClock` and `FakeTurnoutCommandPort` (both already exist). Covers the
roadmap's §6.7 test list exactly where applicable:

- `start()` sends the first command immediately.
- `update()` does not send the second command before the spacing interval
  elapses.
- `update()` sends the second command once the spacing interval elapses,
  with the correct address and position.
- Commands are sent in route order (verified across a 3+ command route).
- `isRunning()` is `true` while commands remain, `false` once the route
  completes.
- Starting an empty route does nothing (`isRunning()` stays `false`, no
  `send()` calls).
- Starting a new route while one is in progress replaces it — the
  in-progress route's remaining commands are never sent.

No new test for `src/mega/main.cpp` itself — composition-root-only, no
native test target, consistent with every prior `main.cpp`-only change
across this entire project (both the Mega and ESP32 sides).

## Non-goals

- `Route`, `RouteService`, `TurnoutService`, `TurnoutCollection` — left
  completely unmodified. Not wired into `main.cpp`, not deprecated, not
  removed. A separate future decision if ever revisited.
- `PulsingLocoNetTransport` — left completely unmodified.
- No route status LED this increment (roadmap *task*, not one of its 2
  stated *completion criteria* — deferred, matching this project's
  established scope-narrowing pattern).
- No failure reporting/detection for a send that doesn't reach the LocoNet
  bus — no such mechanism exists anywhere in this codebase today.
- No persistent route configuration (EEPROM, etc.) — that's Milestone 13.
- No per-route configurable command spacing — a single global constant for
  all routes, per the "single global constant" decision above.
- No real route data for the user's actual layout — 2 clearly-marked
  placeholder routes, to be replaced once real route definitions are known.
