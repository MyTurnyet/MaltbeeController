# Mega Panel Routes (Milestone 12) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and wire a `RouteExecutionService` that lets a dedicated physical button fire a sequence of turnout commands on the Mega 2560 / LocoNet panel, closing out the project's roadmap Milestone 12.

**Architecture:** A new, fully native-tested application-layer class (`RouteExecutionService`, `lib/McsCore/src/application/`) executes a `Route` directly through the existing `TurnoutCommandPort` as a non-blocking state machine — spacing its sends over time, since `PulsingLocoNetTransport` can only track one pending pulse at once. Wired into `src/mega/main.cpp` with 2 new dedicated route buttons and 2 placeholder routes. The ported `Route`/`RouteService`/`TurnoutService`/`TurnoutCollection` classes (built against a different, currently-unwired model) are left completely untouched.

**Tech Stack:** C++17 (AVR/`megaatmega2560` target has no real libstdc++ — no STL containers/strings/optionals in domain code), PlatformIO, Catch2 (native suite).

## Global Constraints

- `RouteExecutionService`'s constructor: `RouteExecutionService(TurnoutCommandPort& commandPort, Clock& clock, unsigned long commandSpacingMs)`.
- `start(const Route&)` sends the route's first command immediately (same eager-send pattern `PulsingLocoNetTransport` already uses), then schedules the rest spaced by `commandSpacingMs`.
- `update()` sends the next due command only once `commandSpacingMs` has elapsed since the last send.
- `isRunning()` returns `true` while commands remain unsent, `false` once the route completes (or immediately, for an empty route — `start()` on an empty route must do nothing).
- Calling `start()` again — same route or a different one — always replaces whatever was in progress; no queueing, no locking.
- `ROUTE_COMMAND_SPACING_MS = 300` — a single global constant in `src/mega/main.cpp`, not configurable per-route.
- `PulsingLocoNetTransport`, `Route`, `RouteService`, `TurnoutService`, `TurnoutCollection` are never modified by this plan.
- 2 new route-trigger buttons on GPIO pins 30 and 31 (continuing the existing stations' 22-29 sequence), active-low, internal pull-up — matching `TurnoutStation.cpp`'s own button-GPIO convention exactly (`ArduinoDigitalInput(pin, /*activeLow=*/true, /*useInternalPullup=*/true)`), debounced at 30ms (matching `TurnoutStation`'s private `DEBOUNCE_MS`).
- 2 placeholder `Route` objects, clearly commented as examples to replace with real route data, built from the existing 4 station addresses (101-104).
- No route status LED this increment (deferred — a roadmap *task*, not one of its 2 stated *completion criteria*).
- No failure reporting — `TurnoutCommandPort::send()` returns `void`; no such mechanism exists anywhere in this codebase today.

---

### Task 1: `RouteExecutionService` application class

**Files:**
- Create: `lib/McsCore/src/application/RouteExecutionService.h`
- Create: `lib/McsCore/src/application/RouteExecutionService.cpp`
- Create: `test/test_route_execution_service/test_main.cpp`

**Interfaces:**
- Consumes: `TurnoutCommandPort::send(int, TurnoutPosition)` (`lib/McsCore/src/ports/TurnoutCommandPort.h`, unmodified), `Clock::nowMilliseconds()` (`lib/McsCore/src/ports/Clock.h`, unmodified), `Route::getTurnoutCount()`/`commandAt(int)` (`lib/McsCore/src/domain/Route.h`, unmodified — `commandAt(int)` returns `TurnoutCommand{int address; TurnoutPosition position;}`).
- Produces: `RouteExecutionService(TurnoutCommandPort&, Clock&, unsigned long)`, `void start(const Route&)`, `void update()`, `bool isRunning() const` — all consumed by Task 2.

- [ ] **Step 1: Write the failing tests**

Create `test/test_route_execution_service/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/RouteExecutionService.h"
#include "domain/Route.h"
#include "support/FakeClock.h"
#include "support/FakeTurnoutCommandPort.h"

namespace
{
    constexpr unsigned long COMMAND_SPACING_MS = 300;
}

TEST_CASE("start sends the first command immediately")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Test Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    route.addTurnout(102, TurnoutPosition::Closed);

    service.start(route);

    REQUIRE(port.sentCommands.size() == 1);
    REQUIRE(port.sentCommands[0].address == 101);
    REQUIRE(port.sentCommands[0].position == TurnoutPosition::Thrown);
}

TEST_CASE("update does not send the second command before the spacing interval elapses")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Test Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    route.addTurnout(102, TurnoutPosition::Closed);

    service.start(route);
    clock.advanceBy(COMMAND_SPACING_MS - 1);
    service.update();

    REQUIRE(port.sentCommands.size() == 1);
}

TEST_CASE("update sends the second command once the spacing interval elapses, with the correct address and position")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Test Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    route.addTurnout(102, TurnoutPosition::Closed);

    service.start(route);
    clock.advanceBy(COMMAND_SPACING_MS);
    service.update();

    REQUIRE(port.sentCommands.size() == 2);
    REQUIRE(port.sentCommands[1].address == 102);
    REQUIRE(port.sentCommands[1].position == TurnoutPosition::Closed);
}

TEST_CASE("commands are sent in route order across a 3-command route")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Three Command Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    route.addTurnout(102, TurnoutPosition::Closed);
    route.addTurnout(103, TurnoutPosition::Thrown);

    service.start(route);
    clock.advanceBy(COMMAND_SPACING_MS);
    service.update();
    clock.advanceBy(COMMAND_SPACING_MS);
    service.update();

    REQUIRE(port.sentCommands.size() == 3);
    REQUIRE(port.sentCommands[0].address == 101);
    REQUIRE(port.sentCommands[1].address == 102);
    REQUIRE(port.sentCommands[2].address == 103);
}

TEST_CASE("isRunning is true while commands remain and false once the route completes")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Test Route");
    route.addTurnout(101, TurnoutPosition::Thrown);
    route.addTurnout(102, TurnoutPosition::Closed);

    REQUIRE_FALSE(service.isRunning());

    service.start(route);
    REQUIRE(service.isRunning());

    clock.advanceBy(COMMAND_SPACING_MS);
    service.update();
    REQUIRE_FALSE(service.isRunning());
}

TEST_CASE("isRunning is false immediately after a single-command route completes")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route route(1, "Single Command Route");
    route.addTurnout(101, TurnoutPosition::Thrown);

    service.start(route);

    REQUIRE_FALSE(service.isRunning());
    REQUIRE(port.sentCommands.size() == 1);
}

TEST_CASE("starting an empty route does nothing")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route emptyRoute(1, "Empty Route");

    service.start(emptyRoute);

    REQUIRE_FALSE(service.isRunning());
    REQUIRE(port.sentCommands.empty());

    service.update();
    REQUIRE(port.sentCommands.empty());
}

TEST_CASE("starting a new route while one is in progress replaces it")
{
    FakeTurnoutCommandPort port;
    FakeClock clock;
    RouteExecutionService service(port, clock, COMMAND_SPACING_MS);

    Route routeA(1, "Route A");
    routeA.addTurnout(101, TurnoutPosition::Thrown);
    routeA.addTurnout(102, TurnoutPosition::Thrown);

    Route routeB(2, "Route B");
    routeB.addTurnout(201, TurnoutPosition::Closed);

    service.start(routeA);
    REQUIRE(port.sentCommands.size() == 1);
    REQUIRE(port.sentCommands[0].address == 101);

    // Route A still has one command pending (address 102) - interrupt it.
    service.start(routeB);

    REQUIRE(port.sentCommands.size() == 2);
    REQUIRE(port.sentCommands[1].address == 201);
    REQUIRE_FALSE(service.isRunning());

    // Advancing time must not resurrect route A's un-sent command (102).
    clock.advanceBy(COMMAND_SPACING_MS * 2);
    service.update();
    REQUIRE(port.sentCommands.size() == 2);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run (via Bash/Git Bash, not PowerShell): `pio test -e native -f test_route_execution_service`
Expected: FAIL / build error — `RouteExecutionService` does not exist yet.

- [ ] **Step 3: Create the header**

Create `lib/McsCore/src/application/RouteExecutionService.h`:

```cpp
#pragma once

#include "../domain/Route.h"
#include "../ports/Clock.h"
#include "../ports/TurnoutCommandPort.h"

// Executes a Route's turnout commands one at a time, directly through the
// given TurnoutCommandPort - the same path a real button press already
// uses. Sends are spaced out over time (update() must be called on every
// loop iteration) rather than fired back-to-back, because
// PulsingLocoNetTransport can only track one pending pulse at a time;
// firing commands faster than that would truncate every earlier pulse.
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

- [ ] **Step 4: Create the implementation**

Create `lib/McsCore/src/application/RouteExecutionService.cpp`:

```cpp
#include "RouteExecutionService.h"

RouteExecutionService::RouteExecutionService(TurnoutCommandPort& commandPort, Clock& clock,
                                               const unsigned long commandSpacingMs)
    : commandPort_(commandPort), clock_(clock), commandSpacingMs_(commandSpacingMs)
{
}

void RouteExecutionService::start(const Route& route)
{
    if (route.getTurnoutCount() == 0)
    {
        activeRoute_ = nullptr;
        return;
    }

    activeRoute_ = &route;
    nextCommandIndex_ = 0;

    const TurnoutCommand firstCommand = activeRoute_->commandAt(nextCommandIndex_);
    commandPort_.send(firstCommand.address, firstCommand.position);
    ++nextCommandIndex_;

    if (nextCommandIndex_ >= activeRoute_->getTurnoutCount())
    {
        activeRoute_ = nullptr;
        return;
    }

    nextSendAtMs_ = clock_.nowMilliseconds() + commandSpacingMs_;
}

void RouteExecutionService::update()
{
    if (activeRoute_ == nullptr)
    {
        return;
    }

    if (nextCommandIndex_ >= activeRoute_->getTurnoutCount())
    {
        activeRoute_ = nullptr;
        return;
    }

    if (clock_.nowMilliseconds() < nextSendAtMs_)
    {
        return;
    }

    const TurnoutCommand command = activeRoute_->commandAt(nextCommandIndex_);
    commandPort_.send(command.address, command.position);
    ++nextCommandIndex_;

    if (nextCommandIndex_ >= activeRoute_->getTurnoutCount())
    {
        activeRoute_ = nullptr;
        return;
    }

    nextSendAtMs_ = clock_.nowMilliseconds() + commandSpacingMs_;
}

bool RouteExecutionService::isRunning() const
{
    return activeRoute_ != nullptr;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_route_execution_service`
Expected: PASS, all 8 test cases.

- [ ] **Step 6: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: 57 suites passing (56 existing + this new one), zero failures.

- [ ] **Step 7: Commit**

```bash
git add lib/McsCore/src/application/RouteExecutionService.h lib/McsCore/src/application/RouteExecutionService.cpp test/test_route_execution_service/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add RouteExecutionService for Mega panel routes

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Wire routes into `src/mega/main.cpp`

**Files:**
- Modify: `src/mega/main.cpp`

**Interfaces:**
- Consumes: `RouteExecutionService` (Task 1), `Button` (`lib/McsCore/src/domain/Button.h`, unmodified — constructor `Button(DigitalInput&, Clock&, unsigned long debounceMilliseconds)`, method `wasPressed()`, requires `update()` called every tick), `ArduinoDigitalInput` (`lib/McsCore/src/adapters/ArduinoDigitalInput.h`, unmodified — constructor `ArduinoDigitalInput(int pin, bool activeLow, bool useInternalPullup)`), `Route` (`lib/McsCore/src/domain/Route.h`, unmodified — constructor `Route(int id, const char* name)`, method `addTurnout(int address, TurnoutPosition position)`).
- Produces: nothing consumed by a later task — Task 3 only documents this task's result.

The file currently reads in full (verified immediately before writing this plan, last touched by commit `17d4396`, a mechanical file-move predating any content change — no intervening commits):

```cpp
#include <Arduino.h>
#include <LocoNet.h>

#include "adapters/ArduinoClock.h"
#include "adapters/LocoNetFeedbackDecoder.h"
#include "adapters/MrrwaLocoNetFeedbackSource.h"
#include "adapters/MrrwaLocoNetSwitchDriver.h"
#include "adapters/MrrwaLocoNetTurnoutAdapter.h"
#include "adapters/PulsingLocoNetTransport.h"
#include "adapters/TurnoutStation.h"

namespace
{
    constexpr int STATION_COUNT = 4;
    constexpr TurnoutConfig STATION_CONFIGS[STATION_COUNT] = {
        {101, "Turnout 1", 22, 23, 8, 9},
        {102, "Turnout 2", 24, 25, 10, 11},
        {103, "Turnout 3", 26, 27, 12, 13},
        {104, "Turnout 4", 28, 29, 14, 15},
    };
    constexpr unsigned long LOCONET_PULSE_DURATION_MS = 250;
}

ArduinoClock clock;

MrrwaLocoNetSwitchDriver locoNetSwitchDriver;
PulsingLocoNetTransport locoNetTransport(locoNetSwitchDriver, clock, LOCONET_PULSE_DURATION_MS);
MrrwaLocoNetTurnoutAdapter turnoutCommandPort(locoNetTransport);

MrrwaLocoNetFeedbackSource locoNetFeedbackSource;
LocoNetFeedbackDecoder locoNetFeedbackDecoder;

TurnoutStation stations[STATION_COUNT] = {
    TurnoutStation(STATION_CONFIGS[0], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[1], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[2], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[3], clock, turnoutCommandPort),
};

void setup()
{
    for (auto& station : stations)
    {
        station.begin();
    }

    LocoNet.init();
}

void loop()
{
    for (auto& station : stations)
    {
        station.update();
    }

    locoNetTransport.update();

    SwitchOutputsReport report;
    if (locoNetFeedbackSource.poll(report))
    {
        const TurnoutFeedbackLookup lookup = locoNetFeedbackDecoder.decode(report);
        if (lookup.found)
        {
            for (auto& station : stations)
            {
                station.applyFeedback(lookup.feedback);
            }
        }
    }
}
```

- [ ] **Step 1: Add the new includes**

```cpp
#include <Arduino.h>
#include <LocoNet.h>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/LocoNetFeedbackDecoder.h"
#include "adapters/MrrwaLocoNetFeedbackSource.h"
#include "adapters/MrrwaLocoNetSwitchDriver.h"
#include "adapters/MrrwaLocoNetTurnoutAdapter.h"
#include "adapters/PulsingLocoNetTransport.h"
#include "adapters/TurnoutStation.h"
#include "application/RouteExecutionService.h"
#include "domain/Button.h"
#include "domain/Route.h"
```

- [ ] **Step 2: Add the new constants**

```cpp
namespace
{
    constexpr int STATION_COUNT = 4;
    constexpr TurnoutConfig STATION_CONFIGS[STATION_COUNT] = {
        {101, "Turnout 1", 22, 23, 8, 9},
        {102, "Turnout 2", 24, 25, 10, 11},
        {103, "Turnout 3", 26, 27, 12, 13},
        {104, "Turnout 4", 28, 29, 14, 15},
    };
    constexpr unsigned long LOCONET_PULSE_DURATION_MS = 250;
    constexpr unsigned long ROUTE_COMMAND_SPACING_MS = 300;
    constexpr unsigned long ROUTE_BUTTON_DEBOUNCE_MS = 30;
}
```

- [ ] **Step 3: Add the route button, route, and service globals**

Immediately after the `TurnoutStation stations[...]` array:

```cpp
TurnoutStation stations[STATION_COUNT] = {
    TurnoutStation(STATION_CONFIGS[0], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[1], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[2], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[3], clock, turnoutCommandPort),
};

ArduinoDigitalInput route1ButtonInput(30, /*activeLow=*/true, /*useInternalPullup=*/true);
ArduinoDigitalInput route2ButtonInput(31, /*activeLow=*/true, /*useInternalPullup=*/true);
Button route1Button(route1ButtonInput, clock, ROUTE_BUTTON_DEBOUNCE_MS);
Button route2Button(route2ButtonInput, clock, ROUTE_BUTTON_DEBOUNCE_MS);

// Placeholder routes - replace with real route definitions for your layout.
// Populated in setup() via addTurnout() calls below.
Route route1(1, "Example Route 1");
Route route2(2, "Example Route 2");

RouteExecutionService routeExecutionService(turnoutCommandPort, clock, ROUTE_COMMAND_SPACING_MS);
```

- [ ] **Step 4: Populate the placeholder routes and begin the new GPIOs in `setup()`**

```cpp
void setup()
{
    route1.addTurnout(101, TurnoutPosition::Thrown);
    route1.addTurnout(102, TurnoutPosition::Closed);
    route2.addTurnout(103, TurnoutPosition::Closed);
    route2.addTurnout(104, TurnoutPosition::Closed);

    route1ButtonInput.begin();
    route2ButtonInput.begin();

    for (auto& station : stations)
    {
        station.begin();
    }

    LocoNet.init();
}
```

- [ ] **Step 5: Drive the route buttons and service in `loop()`**

```cpp
void loop()
{
    for (auto& station : stations)
    {
        station.update();
    }

    locoNetTransport.update();

    SwitchOutputsReport report;
    if (locoNetFeedbackSource.poll(report))
    {
        const TurnoutFeedbackLookup lookup = locoNetFeedbackDecoder.decode(report);
        if (lookup.found)
        {
            for (auto& station : stations)
            {
                station.applyFeedback(lookup.feedback);
            }
        }
    }

    route1Button.update();
    route2Button.update();

    if (route1Button.wasPressed())
    {
        routeExecutionService.start(route1);
    }
    if (route2Button.wasPressed())
    {
        routeExecutionService.start(route2);
    }
    routeExecutionService.update();
}
```

- [ ] **Step 6: Build the Mega environment**

```bash
pio run -e megaatmega2560
```

Expected: builds clean (`SUCCESS`). Note the reported RAM/Flash percentages — Task 3 needs the exact post-change figures, not the pre-existing "RAM 9.9%, Flash 2.7%" figures CLAUDE.md currently cites (those predate this task).

- [ ] **Step 7: Build the other 2 hardware environments as a regression check**

```bash
pio run -e esp32node4
pio run -e esp32dev
```

Expected: both `SUCCESS`, byte-for-byte unaffected — this task touches no shared `lib/` code, only `src/mega/main.cpp`.

- [ ] **Step 8: Run the native test suite**

```bash
pio test -e native
```

Expected: unaffected by this task specifically (57 suites passing, same as after Task 1 — `main.cpp` has no native test target).

**Do NOT launch this command with a backgrounding mechanism and then wait across turns for it.** Run it and wait for it to finish in the same turn.

- [ ] **Step 9: Commit**

```bash
git add src/mega/main.cpp
git commit -m "$(cat <<'EOF'
@ F Wire routes into Mega panel main.cpp

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(`@` risk symbol: `main.cpp` composition-root change with no possible native test, same classification used throughout this project for every `main.cpp`-only commit on both the Mega and ESP32 sides.)

---

### Task 3: Document Milestone 12 and run the staleness sweep

**Files:**
- Modify: `CLAUDE.md`
- Modify: `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`
- Modify: `README.md`
- Modify: `docs/HARDWARE_BRINGUP_CHECKLIST.md`

**Interfaces:**
- Consumes: Task 2's completed wiring and its reported RAM/Flash build figures (this task describes what Tasks 1-2 built; it does not touch code).
- Produces: nothing consumed by a later task — this is the last task in this plan.

- [ ] **Step 1: Update the roadmap doc's Milestone 12 section**

In `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`, find the current `## Milestone 12: Add Routes` section (its `### Tasks` and `### Completion Criteria` sub-sections). Replace it with:

```markdown
## ✅ Milestone 12: Add Routes
**Status:** COMPLETE (route status indication deferred — see note below)

### Tasks

- [x] TDD `Route` — already done; see Milestone 5's own status note above:
  `Route` was ported wholesale from an earlier, separate project
  ("MaltBee-Control-System", 2026-07-13), not built fresh for this
  milestone.
- [x] TDD `RouteExecutionService` — built fresh for this milestone, since
  the ported `RouteService` (also from Milestone 5) is a different
  design that operates against a separate, currently-unwired
  `TurnoutService`/`TurnoutCollection` registry and never reaches
  `TurnoutCommandPort` — it doesn't satisfy this section's original
  "execute a route through the turnout command port" intent (§6.7).
  `RouteExecutionService` is the class this section originally
  specified; `RouteService`/`TurnoutService`/`TurnoutCollection` are
  left exactly as they were — unused, not removed.
- [x] Create route buttons — 2 dedicated buttons, GPIO 30/31 on the Mega.
- [x] Execute a sequence of turnout commands —
  `RouteExecutionService::start()`/`update()`, directly through the
  existing `TurnoutCommandPort`.
- [x] Add optional command spacing if the layout requires it — not
  optional in practice: `PulsingLocoNetTransport` can only track one
  pending pulse at a time, so firing a route's commands back-to-back
  would truncate every earlier pulse's duration. `RouteExecutionService`
  spaces sends by `ROUTE_COMMAND_SPACING_MS` (300ms), folded into the
  base design rather than an add-on.
- [ ] Add route status indication — deferred. This was a task, not one
  of the two stated completion criteria below, and is left for a future
  increment.

### Completion Criteria

- [x] A route button sends all required turnout commands.
- [x] Routes are defined as data rather than hard-coded conditionals —
  2 placeholder `Route` objects in `src/mega/main.cpp`, built via
  `addTurnout()` calls, not hard-coded if/else logic.
```

- [ ] **Step 2: Update CLAUDE.md's milestone summary lines**

Find the current lines (near CLAUDE.md's Mega/`McsCore` test-coverage summary):

```
**Completed milestones:** 1-11 (foundation, ports, Button, Indicator, Turnout domain model, TurnoutIndicator, TurnoutControl, hardware integration programming, LocoNet output, LocoNet feedback, multiple turnouts). `pio run -e megaatmega2560` compiles successfully (RAM 9.9%, Flash 2.7% with 4 stations). Milestones 9-11 are complete on the programming side only — physical wiring and on-hardware verification (button-to-LED and LocoNet send/receive against a real DR5000/DR4018, across all 4 stations) are still outstanding — see the "Milestone 8 hardware" note below.

**Next milestones:** routes (12), persistent configuration (13)
```

Replace with (filling in the exact RAM/Flash percentages from Task 2's `pio run -e megaatmega2560` report — do not guess; use the real reported figures):

```
**Completed milestones:** 1-12 (foundation, ports, Button, Indicator, Turnout domain model, TurnoutIndicator, TurnoutControl, hardware integration programming, LocoNet output, LocoNet feedback, multiple turnouts, routes). `pio run -e megaatmega2560` compiles successfully (RAM <TASK2_RAM_PCT>, Flash <TASK2_FLASH_PCT> with 4 stations + routes). Milestones 9-12 are complete on the programming side only — physical wiring and on-hardware verification (button-to-LED and LocoNet send/receive against a real DR5000/DR4018, across all 4 stations, plus the 2 new route buttons) are still outstanding — see the "Milestone 8 hardware" note below.

**Next milestones:** persistent configuration (13)
```

- [ ] **Step 3: Add a new class entry and a Routes section to CLAUDE.md's architecture description**

In CLAUDE.md's description of `lib/McsCore/src/application/` classes (the paragraph listing `TurnoutControl` and similar application-layer classes), add `RouteExecutionService` with a short description: "RouteExecutionService (Milestone 12 — executes a `Route`'s turnout commands one at a time directly through `TurnoutCommandPort`, spaced over time via its own `update()` since `PulsingLocoNetTransport` can only track one pending pulse at a time; built fresh rather than reusing the ported `RouteService`, which operates against a separate, unwired `TurnoutService`/`TurnoutCollection` registry and never reaches real hardware)."

Also add a short new section documenting Milestone 12's wiring, following this project's established per-milestone/per-sub-project documentation pattern (see any "### <Name> (sub-project #N)" section in the ESP32 node-board part of this file for the expected level of detail — net effect, test coverage added, non-goals). Cover: the `RouteExecutionService` class and its non-blocking spaced-send design; the 2 new route buttons (GPIO 30/31) and 2 placeholder routes wired into `src/mega/main.cpp`; the explicit decision to leave `Route`/`RouteService`/`TurnoutService`/`TurnoutCollection` untouched; test coverage added (`test/test_route_execution_service`, 8 cases, 56 → 57 suites); non-goals (no status LED, no failure reporting, no real route data for an actual layout yet, no persistent config).

- [ ] **Step 4: Update README.md's "Next Steps" Mega bullet**

Find the current bullet:

```
- **Mega 2560 panel:** verify the electrical LocoNet interface and confirm DR5000/DR4018
  behavior on hardware across all 4 stations, both send and feedback (finish Milestone 8's
  hardware pass); then routes (Milestone 12) and persistent configuration (Milestone 13).
```

Replace with:

```
- **Mega 2560 panel:** verify the electrical LocoNet interface and confirm DR5000/DR4018
  behavior on hardware across all 4 stations, both send and feedback (finish Milestone 8's
  hardware pass), plus the 2 new route buttons (Milestone 12, done in firmware — see
  `CLAUDE.md`'s Milestone 12 section); then persistent configuration (Milestone 13).
```

Also check README.md's `## Current Status` section's Mega paragraph (the one starting "**Mega 2560 panel:** Milestones 1-11 are complete...") — update "1-11" to "1-12" and its sentence about "Routes (Milestone 12) ... are not yet started" to reflect that Milestone 12 is now done (programming side), re-reading the live paragraph first since its exact current wording may differ slightly from what's quoted here.

- [ ] **Step 5: Add a route-buttons bring-up subsection to the hardware checklist**

In `docs/HARDWARE_BRINGUP_CHECKLIST.md`, immediately after the existing `### 1.4 Done when` subsection (Part 1, Mega panel) and before the `---` separator preceding `## Part 2`, add:

```markdown
### 1.5 Route buttons (Milestone 12)

**Reference:** `src/mega/main.cpp` for the 2 route buttons' exact pins
(GPIO 30/31) and the 2 placeholder routes' turnout addresses/positions.

Verify after all 4 turnout stations (1.1-1.4) are already working —
routes just fire the same `TurnoutCommandPort` sends a station's own
button would, so there's nothing new to verify about the LocoNet send
path itself, only the route-triggering mechanism and command spacing.

1. Wire the 2 route buttons (GPIO 30/31, active-low, internal pull-up —
   no external parts needed, same as the existing station buttons).
2. Press the first route button. Using a LocoNet monitor (or watching
   the DR4018's own output LEDs if configured for the placeholder
   routes' addresses), confirm every command in that route fires, in
   order, with a visible gap between each (`ROUTE_COMMAND_SPACING_MS =
   300` in `src/mega/main.cpp` — if a DR4018 doesn't reliably latch
   from back-to-back route commands, this is the first thing to
   adjust, same as `LOCONET_PULSE_DURATION_MS` for individual turnouts).
3. Repeat for the second route button.
4. Press a route button, then immediately press the other one before
   the first route finishes — confirm the second route's commands
   fire and the first route's remaining (un-sent) commands never do
   (`RouteExecutionService`'s interrupt-replaces-in-progress behavior).

**Done when:** both route buttons fire their full, correctly-ordered,
properly-spaced command sequence, and starting one route mid-sequence
of another cleanly replaces it rather than firing both.
```

- [ ] **Step 6: Run the staleness sweep**

```bash
grep -ni "milestone 12\|routes (12)\|TDD \`RouteExecutionService\`\|TDD \`Route\`\|not yet started" CLAUDE.md internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md README.md docs/HARDWARE_BRINGUP_CHECKLIST.md
```

Read every hit's surrounding context (not just the matched line) and confirm every claim this milestone makes false is now corrected by Steps 1-5 above. If the sweep surfaces a location not already covered, annotate it in place (revise the text to state current reality, don't delete it outright) rather than ignoring it.

**Known, deliberately out-of-scope finding:** `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md` has a large master checklist section further down the file (searchable by `- [ ] TDD` patterns in a block containing "Create Arduino digital output adapter", "Add `Signal` later", etc.) that still shows items like "TDD `Indicator`" and "TDD `Button`" as unchecked `[ ]`, even though those have been done and marked complete elsewhere in the same document for a long time — this drift predates Routes entirely and spans the whole document, not something this milestone's own work makes newly false. Do not attempt to reconcile that entire master checklist as part of this task; it's a separate, pre-existing, document-wide inconsistency. Leave it alone.

- [ ] **Step 7: Verify the sweep is clean**

Re-run the grep from Step 6. Confirm every hit that was made false by this milestone is now annotated, and that no hit describing already-correct, still-true behavior was altered.

- [ ] **Step 8: Commit**

```bash
git add CLAUDE.md internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md README.md docs/HARDWARE_BRINGUP_CHECKLIST.md
git commit -m "$(cat <<'EOF'
. d Document Milestone 12 (Mega panel routes)

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
