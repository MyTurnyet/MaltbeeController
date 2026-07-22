# MaltBee Control System (MCS)
## Architecture, TDD, and Development Roadmap

---

## Project Merge (2026-07-13)

This project is the result of merging two MaltBee projects:
- **MaltbeeController** (architectural foundation, ports/adapters, Button/Indicator)
- **MaltBee-Control-System** (domain logic: Turnout, Route, services)

The merge used MaltbeeController as the base and incrementally ported domain classes
from MaltBee-Control-System. All code now uses Catch2 for testing and targets the
Arduino Mega 2560 via PlatformIO.

**Current Status:** Milestones 1-8 complete. Milestone 9 (LocoNet Output) in progress: translation and pulse-timing adapters implemented and natively tested, wired into `main.cpp`, firmware builds for the Mega 2560. Electrical LocoNet interface verification and on-hardware DR5000/DR4018 confirmation still outstanding.

---

## Architecture Decision Records (from original project)

### Turnout Addressing
- Valid range: 1-9999
- Uniqueness is expected but not enforced at domain level
- Enforcement will be added at the repository/configuration level later

### Route Validation
- Routes must not be empty
- No duplicate turnouts within a route
- All referenced addresses must exist in the collection

### Hardware Abstraction
- Domain logic must not call Arduino APIs directly
- All hardware interactions go through port interfaces
- Hardware drivers implement ports

---

## 1. Project Purpose

The MaltBee Control System is a reusable Arduino/PlatformIO-based control platform for model railroad panels.

The first practical goal is to replace and extend the functionality previously provided by Team Digital SRC8 boards:

- Read panel pushbuttons
- Drive panel LEDs
- Maintain turnout state
- Send turnout commands over LocoNet
- Receive turnout state changes over LocoNet
- Support routes
- Support multiple panels and multiple turnouts
- Remain testable without requiring the physical Mega 2560

The project should be designed as a maintainable embedded software system rather than as a single large Arduino sketch.

---

## 2. Engineering Principles

The project should follow these principles:

1. **Test-Driven Development**
   - Write a failing test first.
   - Implement the smallest amount of production code needed to pass.
   - Refactor only while tests are green.
   - Keep hardware-dependent code outside the core domain wherever possible.

2. **Dependency Inversion**
   - Domain classes depend on abstractions, not Arduino APIs.
   - Hardware adapters implement interfaces owned by the application or domain.
   - The core logic should not directly call `digitalRead`, `digitalWrite`, or LocoNet library functions.

3. **Hexagonal Architecture**
   - Domain logic lives in the center.
   - Ports define what the domain needs from the outside world.
   - Adapters connect those ports to Arduino GPIO, LocoNet, serial logging, EEPROM, and future hardware.

4. **Single Responsibility**
   - A button reads button state.
   - A turnout models turnout behavior.
   - A route coordinates multiple turnout commands.
   - A LocoNet adapter translates domain commands into LocoNet messages.

5. **Explicit State**
   - The software should model turnout, signal, button, and route state directly.
   - State changes should occur through methods that enforce valid transitions.

6. **Event-Driven Behavior**
   - Inputs produce events.
   - Application services interpret events.
   - Outputs react to application decisions.
   - The main loop should coordinate components rather than contain business logic.

---

## 3. Architectural Overview

```text
+--------------------------------------------------+
|                  Hardware Layer                  |
|                                                  |
| Arduino GPIO   LocoNet Bus   EEPROM   Serial     |
+------------------------+-------------------------+
                         |
                         v
+--------------------------------------------------+
|                    Adapters                      |
|                                                  |
| ArduinoDigitalInput                              |
| ArduinoDigitalOutput                             |
| MrrwaLocoNetAdapter                              |
| ArduinoClock                                     |
| SerialLogger                                     |
| EepromConfigurationRepository                    |
+------------------------+-------------------------+
                         |
                         v
+--------------------------------------------------+
|                 Application Layer                |
|                                                  |
| PanelController                                  |
| TurnoutCommandService                            |
| RouteExecutionService                            |
| InputPollingService                              |
| LayoutStateCoordinator                           |
+------------------------+-------------------------+
                         |
                         v
+--------------------------------------------------+
|                   Domain Layer                   |
|                                                  |
| Button                                           |
| Indicator                                        |
| Turnout                                          |
| Route                                            |
| Signal                                           |
| Panel                                            |
| Domain Events                                    |
+--------------------------------------------------+
```

The domain and application layers should compile and run in native unit tests without the Arduino framework.

---

## 4. Recommended Project Structure

```text
MaltBeeController/
├── platformio.ini
├── README.md
├── docs/
│   ├── architecture.md
│   ├── wiring.md
│   ├── development-roadmap.md
│   └── adr/
│       ├── 0001-use-platformio.md
│       ├── 0002-use-hexagonal-architecture.md
│       └── 0003-separate-domain-from-hardware.md
├── include/
│   ├── domain/
│   ├── application/
│   └── ports/
├── src/
│   ├── domain/
│   ├── application/
│   ├── adapters/
│   │   ├── arduino/
│   │   ├── loconet/
│   │   ├── storage/
│   │   └── logging/
│   └── main.cpp
├── test/
│   ├── test_button/
│   ├── test_turnout/
│   ├── test_route/
│   ├── test_panel_controller/
│   └── support/
└── lib/
```

Suggested naming rule:

- `domain/` contains pure C++ only.
- `application/` contains use cases and coordination logic.
- `ports/` contains interfaces.
- `adapters/` contains Arduino-, LocoNet-, storage-, and logging-specific code.
- `main.cpp` wires the system together.

---

## 5. Core Ports

Ports are interfaces that isolate the core software from hardware and external systems.

### 5.1 DigitalInput

Represents a source that can be read as active or inactive.

```cpp
class DigitalInput
{
public:
    virtual ~DigitalInput() = default;
    virtual bool isActive() const = 0;
};
```

Possible implementations:

- `ArduinoDigitalInput`
- `FakeDigitalInput`
- `DebouncedDigitalInput`

### 5.2 DigitalOutput

Represents an output that can be switched on or off.

```cpp
class DigitalOutput
{
public:
    virtual ~DigitalOutput() = default;
    virtual void set(bool active) = 0;
    virtual bool isSet() const = 0;
};
```

Possible implementations:

- `ArduinoDigitalOutput`
- `FakeDigitalOutput`

### 5.3 Clock

Provides time without forcing domain code to call `millis()`.

```cpp
class Clock
{
public:
    virtual ~Clock() = default;
    virtual unsigned long nowMilliseconds() const = 0;
};
```

Possible implementations:

- `ArduinoClock`
- `FakeClock`

### 5.4 TurnoutCommandPort

Sends turnout commands to the layout network.

```cpp
enum class TurnoutPosition
{
    Closed,
    Thrown
};

class TurnoutCommandPort
{
public:
    virtual ~TurnoutCommandPort() = default;

    virtual void send(
        int address,
        TurnoutPosition position
    ) = 0;
};
```

Possible implementations:

- `MrrwaLocoNetTurnoutAdapter`
- `FakeTurnoutCommandPort`

### 5.5 TurnoutFeedbackPort

Receives turnout state changes from outside the application.

This may eventually be represented as callbacks, polling, or queued domain events.

```cpp
struct TurnoutFeedback
{
    int address;
    TurnoutPosition position;
};
```

### 5.6 Logger

```cpp
class Logger
{
public:
    virtual ~Logger() = default;
    virtual void info(const char* message) = 0;
    virtual void error(const char* message) = 0;
};
```

Possible implementations:

- `SerialLogger`
- `NullLogger`
- `FakeLogger`

### 5.7 ConfigurationRepository

```cpp
class ConfigurationRepository
{
public:
    virtual ~ConfigurationRepository() = default;
    virtual bool load() = 0;
    virtual bool save() = 0;
};
```

Possible implementations:

- `EepromConfigurationRepository`
- `InMemoryConfigurationRepository`
- future JSON or SD-card implementation

---

## 6. Domain Classes

## 6.1 Button

### Responsibility

Represent the logical behavior of a momentary input.

### Parameters

- `DigitalInput& input`
- `Clock& clock`
- debounce duration

### Methods

- `update()`
- `isPressed()`
- `wasPressed()`
- `wasReleased()`

### State

- last raw reading
- stable reading
- previous stable reading
- time of last raw transition

### Tests

- Button begins released
- Button reports pressed after a stable active input
- Button ignores input changes shorter than the debounce period
- Button reports `wasPressed()` once per press
- Button does not repeatedly report `wasPressed()` while held
- Button reports `wasReleased()` once per release
- Button works with active-low hardware through the adapter

---

## 6.2 Indicator

### Responsibility

Represent a logical indicator without knowing how the physical output is implemented.

### Parameters

- `DigitalOutput& output`

### Methods

- `on()`
- `off()`
- `set(bool active)`
- `isOn()`

### Tests

- Indicator begins off
- Calling `on()` activates the output
- Calling `off()` deactivates the output
- Calling `set(true)` activates the output
- Calling `set(false)` deactivates the output
- Repeated calls are idempotent

---

## 6.3 Turnout

### Responsibility

Represent a turnout and its logical position.

### Parameters

- turnout address
- initial position

### Methods

- `throwTurnout()`
- `closeTurnout()`
- `setPosition(TurnoutPosition position)`
- `position()`
- `isThrown()`
- `isClosed()`
- `address()`

### State

- address
- current position

### Tests

- Turnout stores its address
- Turnout begins in the configured initial position
- `throwTurnout()` changes the position to thrown
- `closeTurnout()` changes the position to closed
- Repeating the same command does not create a false state transition
- Turnout reports whether it is thrown
- Turnout reports whether it is closed

Important design rule:

The domain `Turnout` should not directly own Arduino LED objects and should not directly call LocoNet. It should model turnout state only.

---

## 6.4 TurnoutIndicator

### Responsibility

Display the current turnout position using two logical indicators.

### Parameters

- thrown indicator
- closed indicator

### Methods

- `display(TurnoutPosition position)`
- `clear()`

### Tests

- Displaying thrown turns on the thrown indicator
- Displaying thrown turns off the closed indicator
- Displaying closed turns on the closed indicator
- Displaying closed turns off the thrown indicator
- Clear turns both indicators off

---

## 6.5 TurnoutControl

### Responsibility

Associate turnout command buttons, turnout state, indicators, and the command port.

This is an application-level class or use case rather than a pure domain entity.

### Parameters

- `Button& throwButton`
- `Button& closeButton`
- `Turnout& turnout`
- `TurnoutIndicator& indicator`
- `TurnoutCommandPort& turnoutCommandPort`

### Methods

- `update()`
- `applyFeedback(TurnoutFeedback feedback)`

### Behavior

- A throw-button press sends a thrown command.
- A close-button press sends a closed command.
- Indicators should normally reflect confirmed layout state.
- Local optimistic updates may be supported later as a configuration choice.

### Tests

- Throw-button press sends one thrown command
- Holding throw button does not send repeated commands
- Close-button press sends one closed command
- Feedback updates turnout state
- Feedback updates indicators
- Feedback for another address is ignored
- Repeated feedback for the current state causes no harmful behavior

---

## 6.6 Route

### Responsibility

Represent a named collection of desired turnout positions.

### Parameters

- route identifier
- route name
- list of turnout commands

### Methods

- `id()`
- `name()`
- `commands()`
- `containsTurnout(address)`

### Supporting Value Object

```cpp
struct TurnoutCommand
{
    int address;
    TurnoutPosition position;
};
```

### Tests

- Route stores its identifier
- Route stores its name
- Route returns its turnout commands in order
- Route reports whether it contains a turnout
- Route may contain both thrown and closed commands
- Empty routes are rejected or explicitly supported

---

## 6.7 RouteExecutionService

### Responsibility

Execute a route through the turnout command port.

### Parameters

- `TurnoutCommandPort&`

### Methods

- `execute(const Route& route)`

### Tests

- Executes every route command
- Executes commands in route order
- Sends the correct address and position
- Does nothing harmful for an empty route
- Reports or logs transmission failure when failure handling is added

---

## 6.8 Panel

### Responsibility

Represent a collection of controls and indicators associated with a physical panel.

### Parameters

- panel name
- collection of turnout controls
- collection of route controls

### Methods

- `begin()`
- `update()`
- `applyFeedback()`

### Tests

- Panel updates all controls
- Panel routes feedback to the correct turnout control
- Panel ignores unrelated feedback
- Panel can contain multiple turnout controls
- Panel can contain multiple route controls

---

## 6.9 Signal

This should be added after turnout control and LocoNet communication are stable.

### Possible States

- Stop
- Approach
- Clear
- Dark

### Methods

- `setAspect()`
- `aspect()`

### Tests

- Signal begins in the configured initial aspect
- Signal changes aspect
- Invalid aspect transitions are rejected if transition rules are added

---

## 7. Hardware Adapters

## 7.1 ArduinoDigitalInput

### Responsibility

Adapt an Arduino GPIO pin to the `DigitalInput` port.

### Parameters

- pin number
- active level
- pin mode

### Methods

- `begin()`
- `isActive()`

### Tests

Host-based unit tests are optional because this class is a thin hardware adapter.

Recommended verification:

- Small hardware integration test
- Button press reads active
- Button release reads inactive
- Active-low behavior is correct

---

## 7.2 ArduinoDigitalOutput

### Responsibility

Adapt an Arduino GPIO pin to the `DigitalOutput` port.

### Parameters

- pin number
- active level

### Methods

- `begin()`
- `set(bool active)`
- `isSet()`

### Verification

- Setting true turns the output on
- Setting false turns the output off
- Active-low output behavior works if supported

---

## 7.3 ArduinoClock

### Responsibility

Adapt `millis()` to the `Clock` port.

### Method

- `nowMilliseconds()`

---

## 7.4 MrrwaLocoNetTurnoutAdapter

### Responsibility

Translate application turnout commands into MRRWA LocoNet library calls.

### Parameters

- LocoNet library dependency or wrapper

### Methods

- `begin()`
- `send(address, position)`
- `poll()`
- `setFeedbackHandler(...)` or return queued messages

### Tests

Prefer testing the translation logic through a lower-level LocoNet transport abstraction.

Suggested internal port:

```cpp
class LocoNetTransport
{
public:
    virtual ~LocoNetTransport() = default;
    virtual void sendPacket(const LocoNetPacket& packet) = 0;
};
```

### Tests

- Closed command creates the expected LocoNet message
- Thrown command creates the expected LocoNet message
- Received LocoNet turnout feedback maps to the correct domain address
- Irrelevant packet types are ignored
- Invalid packets do not crash the controller

---

## 8. Test Doubles

Create reusable fakes under `test/support/`.

### FakeDigitalInput

```cpp
class FakeDigitalInput : public DigitalInput
{
public:
    bool active = false;

    bool isActive() const override
    {
        return active;
    }
};
```

### FakeDigitalOutput

Tracks the last requested state.

### FakeClock

Allows tests to advance time manually.

### FakeTurnoutCommandPort

Stores sent commands in a vector.

### FakeLogger

Stores log messages for assertions.

### InMemoryConfigurationRepository

Allows configuration tests without EEPROM.

---

## 9. PlatformIO Test Environments

Use separate environments for the Mega and native host tests.

Example `platformio.ini`:

```ini
[platformio]
default_envs = megaatmega2560

[env:megaatmega2560]
platform = atmelavr
board = megaatmega2560
framework = arduino
monitor_speed = 115200
test_framework = unity
test_build_src = true

[env:native]
platform = native
test_framework = unity
test_build_src = true
build_flags =
    -std=c++17
```

Important:

Pure domain and application code should compile in the native environment.

Arduino-specific code should be isolated so native tests do not need `Arduino.h`.

Typical commands:

```bash
pio test -e native
pio run -e megaatmega2560
pio run -e megaatmega2560 --target upload
pio device monitor
```

---

## 10. Development Milestones

### ✅ Milestone 1: Establish the Testing Foundation
**Status:** COMPLETE

### ✅ Milestone 2: Build Digital Input and Output Ports
**Status:** COMPLETE

### ✅ Milestone 3: TDD the Indicator
**Status:** COMPLETE

### ✅ Milestone 4: TDD the Debounced Button
**Status:** COMPLETE

### ✅ Milestone 5: TDD the Turnout Domain Model
**Status:** COMPLETE (ported from MaltBee-Control-System 2026-07-13)

**Additional domain classes ported:**
- ✅ TurnoutCollection (registry/lookup)
- ✅ TurnoutService (coordination)
- ✅ Route (command sequences)
- ✅ RouteService (execution)

### ✅ Milestone 6: TDD Turnout Indicators
**Status:** COMPLETE

### ✅ Milestone 7: TDD Turnout Control Use Case
**Status:** COMPLETE

### ✅ Milestone 8: Integrate Physical Buttons and LEDs
**Status:** COMPLETE (programming portion). Physical wiring and on-hardware verification still outstanding.

### → Milestone 9: Add LocoNet Output
**Status:** IN PROGRESS - `MrrwaLocoNetTurnoutAdapter`, `PulsingLocoNetTransport`, and `MrrwaLocoNetSwitchDriver` implemented and wired into `main.cpp`; firmware builds for the Mega 2560. Electrical interface verification and on-hardware DR5000/DR4018 confirmation remain.

## Milestone 6: TDD Turnout Indicators

### Test Order

1. Closed position lights the closed indicator only.
2. Thrown position lights the thrown indicator only.
3. Clear turns both off.

### Completion Criteria

- Indicator mapping is isolated from turnout logic.
- Physical LED colors are not part of the domain model.

---

## Milestone 7: TDD Turnout Control Use Case

### Test Order

1. Throw-button edge sends one thrown command.
2. Close-button edge sends one closed command.
3. Holding either button sends no repeat commands.
4. Feedback updates turnout state.
5. Feedback updates indicators.
6. Feedback for another turnout is ignored.

### Completion Criteria

- The use case is testable without Arduino or LocoNet hardware.
- Turnout command transmission is represented by a port.

---

## Milestone 8: Integrate Physical Buttons and LEDs

### Tasks

- Wire one throw button.
- Wire one close button.
- Wire one thrown indicator.
- Wire one closed indicator.
- Build adapters in `main.cpp`.
- Compose one `TurnoutControl`.
- Verify behavior on hardware.

### Completion Criteria

- Physical controls operate through the same application classes used in native tests.
- `main.cpp` contains object wiring, not business logic.

---

## Milestone 9: Add LocoNet Output

**Status:** IN PROGRESS

### Tasks

- ✅ Install the MRRWA LocoNet library (pinned to `mrrwa/LocoNet` GitHub tag `1.1.13`, not the PlatformIO registry's `1.1.6`, which fails to build for Mega 2560 — see note below).
- ⬜ Verify the electrical LocoNet interface.
- ⬜ Create a minimal LocoNet send experiment.
- ✅ Implement `MrrwaLocoNetTurnoutAdapter`.
- ⬜ Send a turnout command to the DR5000.
- ⬜ Confirm the DR4018 responds.

### Completion Criteria

- Button press sends a LocoNet turnout command.
- The correct DR4018 output operates.
- The application depends only on `TurnoutCommandPort`.

### Implementation Notes (2026-07-21)

The LocoNet output path ended up as three classes instead of one, to keep the pulse-timing logic natively testable without Arduino or the MRRWA library:

- `MrrwaLocoNetTurnoutAdapter` — translates `TurnoutCommandPort::send()` into a `LocoNetPacket` on the `LocoNetTransport` port (native-tested).
- `PulsingLocoNetTransport` — implements `LocoNetTransport`. The DR4018 expects a solenoid-style pulse, so this sends an immediate ON `requestSwitch`, then a non-blocking timed OFF release via `update()` (called from `loop()`), driven through a new `LocoNetSwitchDriver` port. Fully native-tested (7 test cases) using a fake driver and `FakeClock`, including the case where a new command arrives while a release is still pending.
- `MrrwaLocoNetSwitchDriver` — implements `LocoNetSwitchDriver` against the real `LocoNet.requestSwitch()` call, `#ifdef ARDUINO`-guarded like the other hardware adapters and not natively tested (thin hardware shim, same pattern as `ArduinoDigitalOutput`/`ArduinoClock`).

`main.cpp` wires `MrrwaLocoNetSwitchDriver` → `PulsingLocoNetTransport` → `MrrwaLocoNetTurnoutAdapter` in place of `NullTurnoutCommandPort`, calls `LocoNet.init()` in `setup()`, and `locoNetTransport.update()` in `loop()`. `LOCONET_PULSE_DURATION_MS` (currently 250ms) is a placeholder pending on-hardware tuning against the real DR4018.

Remaining work is entirely hands-on: verify the electrical LocoNet interface, flash the board, confirm a button press reaches the DR5000, and confirm the DR4018 output responds.

---

## Milestone 10: Add LocoNet Feedback

### Tasks

- Poll LocoNet messages.
- Decode turnout state messages.
- Convert messages into `TurnoutFeedback`.
- Route feedback to the appropriate control.
- Update panel indicators from confirmed state.

### Completion Criteria

- LED state changes when a turnout command is sent from another LocoNet device.
- The panel remains synchronized with JMRI, throttles, and other controllers.

---

## Milestone 11: Add Multiple Turnouts

### Tasks

- Replace individually named controls with a fixed collection.
- Add configuration objects for pin assignments and turnout addresses.
- Route feedback by turnout address.
- Verify at least four turnout controls.

### Completion Criteria

- New turnout controls can be added primarily through configuration.
- No duplicated control logic exists.

---

## Milestone 12: Add Routes

### Tasks

- TDD `Route`.
- TDD `RouteExecutionService`.
- Create route buttons.
- Execute a sequence of turnout commands.
- Add optional command spacing if the layout requires it.
- Add route status indication.

### Completion Criteria

- A route button sends all required turnout commands.
- Routes are defined as data rather than hard-coded conditionals.

---

## Milestone 13: Add Persistent Configuration

### Tasks

- Define configuration data structures.
- Add `ConfigurationRepository`.
- Create an in-memory repository first.
- Add EEPROM persistence.
- Add schema versioning.
- Add safe defaults when configuration is invalid.

### Completion Criteria

- Turnout addresses and pin assignments can be loaded from storage.
- Invalid configuration does not leave the system in an unsafe state.

---

## Milestone 14: Production Hardening

### Tasks

- Remove all blocking `delay()` calls.
- Add watchdog planning.
- Add startup self-test.
- Add error logging.
- Add LocoNet connection-health indication.
- Add brownout and restart behavior documentation.
- Add input and output wiring protection.
- Document current limits.
- Move permanent hardware from breadboard to a reliable mounting solution.
- Add version information to startup logs.
- Add release tags in Git.

### Completion Criteria

- Controller can run unattended.
- Restart behavior is deterministic.
- Hardware wiring is documented.
- A failed peripheral does not crash the entire loop.

---

## 11. Composition Root

`main.cpp` should be the composition root.

Its job is to:

- Define pin assignments
- Construct Arduino adapters
- Construct domain objects
- Construct application services
- Call `begin()` in `setup()`
- Call non-blocking `update()` and `poll()` methods in `loop()`

Example shape:

```cpp
#include <Arduino.h>

#include "adapters/arduino/ArduinoClock.h"
#include "adapters/arduino/ArduinoDigitalInput.h"
#include "adapters/arduino/ArduinoDigitalOutput.h"
#include "application/TurnoutControl.h"
#include "domain/Button.h"
#include "domain/Indicator.h"
#include "domain/Turnout.h"

ArduinoClock clock;

ArduinoDigitalInput throwInput(22, true);
ArduinoDigitalInput closeInput(23, true);

ArduinoDigitalOutput thrownOutput(8, true);
ArduinoDigitalOutput closedOutput(9, true);

Button throwButton(throwInput, clock, 30);
Button closeButton(closeInput, clock, 30);

Indicator thrownIndicator(thrownOutput);
Indicator closedIndicator(closedOutput);

Turnout turnout(101, TurnoutPosition::Closed);

TurnoutIndicator turnoutIndicator(
    thrownIndicator,
    closedIndicator
);

// Later:
// MrrwaLocoNetTurnoutAdapter turnoutCommands(...);
// TurnoutControl control(...);

void setup()
{
    throwInput.begin();
    closeInput.begin();

    thrownOutput.begin();
    closedOutput.begin();
}

void loop()
{
    throwButton.update();
    closeButton.update();

    // control.update();
    // loconet.poll();
}
```

`main.cpp` should remain small even as the system grows.

---

## 12. TDD Workflow

For each behavior:

1. Name the behavior in domain language.
2. Write the smallest failing test.
3. Confirm the failure is meaningful.
4. Add the smallest implementation.
5. Run all native tests.
6. Refactor.
7. Build the Mega target.
8. Upload only when hardware integration is needed.
9. Commit the completed behavior.

Example commit sequence:

```text
test: button reports one pressed edge
feat: implement button pressed edge
refactor: extract debounce state transition
```

---

## 13. Definition of Done

A task is complete when:

- The behavior is described by automated tests.
- Native tests pass.
- The Mega build passes.
- Hardware behavior is verified when applicable.
- No Arduino dependency has leaked into the domain layer.
- Public interfaces use domain terminology.
- Error and restart behavior are considered.
- Documentation is updated.
- Code is committed to Git.

---

## 14. Initial Backlog

Use this as the first working backlog.

### Project Setup

- [ ] Create Git repository
- [ ] Add `.gitignore`
- [ ] Add `README.md`
- [ ] Add native PlatformIO environment
- [ ] Add Unity test framework
- [ ] Create architecture directories
- [ ] Add one passing native test
- [ ] Add continuous integration later

### Ports and Adapters

- [ ] Create `DigitalInput`
- [ ] Create `DigitalOutput`
- [ ] Create `Clock`
- [ ] Create `Logger`
- [ ] Create `TurnoutCommandPort`
- [ ] Create Arduino digital input adapter
- [ ] Create Arduino digital output adapter
- [ ] Create Arduino clock adapter
- [ ] Create fake implementations for tests

### Domain

- [ ] TDD `Indicator`
- [ ] TDD `Button`
- [ ] TDD `Turnout`
- [ ] TDD `TurnoutIndicator`
- [ ] TDD `Route`
- [ ] Add `Signal` later

### Application

- [ ] TDD `TurnoutControl`
- [ ] TDD `RouteExecutionService`
- [ ] Add feedback routing
- [ ] Add `Panel`
- [ ] Add layout-state coordination later

### Hardware Integration

- [ ] Integrate one physical button
- [ ] Integrate one physical LED
- [ ] Integrate two-button turnout panel
- [ ] Select LocoNet interface circuit or module
- [ ] Install MRRWA LocoNet library
- [ ] Send first turnout command
- [ ] Receive first turnout feedback message
- [ ] Synchronize LEDs with external commands

### Expansion

- [ ] Support multiple turnouts
- [ ] Support route buttons
- [ ] Add EEPROM configuration
- [ ] Add startup self-test
- [ ] Add panel health LED
- [ ] Design permanent terminal-block wiring
- [ ] Design enclosure or mounting system

---

## 15. Immediate Next Task

The next task should be:

> Add a native PlatformIO test environment and create the first test-driven `Indicator` class.

Do not begin with LocoNet.

First prove that:

- the project can run native tests,
- the core code does not depend on Arduino,
- test doubles can replace physical inputs and outputs,
- the same production classes can later be wired to the Mega.

That foundation will make every later feature easier to add, test, and maintain.
