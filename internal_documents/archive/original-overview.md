> **ARCHIVED** — This document is preserved for historical context.
> The MaltBee-Control-System project was merged into MaltbeeControllerSystem
> on 2026-07-13. See the main roadmap document for current project status.

---

# MaltBee Control System (MCS)

## Project Vision

MaltBee Control System (MCS) is an embedded railroad control system for the Echo Lake & Maltby Railroad.

The goal is to build the system using professional software engineering practices:

- Test Driven Development
- Hexagonal Architecture
- Dependency Inversion
- Separation of domain logic from hardware
- Clean, maintainable C++ design

The Arduino hardware is considered an implementation detail. The core MCS logic should be testable on a desktop computer.

---

# Development Environment

## Primary Tools

- CLion
- CMake
- Catch2
- Git
- Later: PlatformIO + Arduino Framework

---

# Initial Project Structure

```text
MaltBee-Control-System
│
├── CMakeLists.txt
├── platformio.ini          (later, for hardware builds)
│
├── src
│   ├── domain              <-- pure C++ logic (testable)
│   │   ├── Turnout.cpp
│   │   └── Turnout.h
│   │
│   └── hardware            <-- Arduino-specific code later
│       └── MegaOutput.cpp
│
├── test
│   └── domain
│       └── TurnoutTests.cpp
│
└── external
    └── catch2
```

---

# Architecture Goal

```text
                 JMRI
                  |
               LocoNet
                  |
          +---------------+
          | MCS Controller |
          +---------------+
                  |
        +---------+---------+
        |                   |
  Turnout Service     Signal Service
        |
    Domain Logic
        |
  ----------------
  |              |
Tests        Arduino
             Hardware
```

The Arduino is only responsible for interacting with hardware.

The domain model should not know it is running on an Arduino.

---

# Initial CLion Setup

Create a new project:

```text
File
 └── New Project
```

Choose:

```text
C++ Executable
```

Settings:

```text
Language: C++
Standard: C++20
Build System: CMake
```

Project name:

```text
MaltBee-Control-System
```

---

# Create Directories

Create:

```text
src
src/domain
src/hardware
test
test/domain
external
```

---

# CMake Setup

Initial CMake responsibilities:

- Build domain library
- Download Catch2
- Build tests
- Integrate tests with CLion

The project uses FetchContent to retrieve Catch2 automatically.

---

# First Domain Object: Turnout

The first goal is modeling a turnout without hardware.

Example:

```cpp
enum class TurnoutPosition
{
    Straight,
    Diverging
};
```

A turnout should support:

- Throw straight
- Throw diverging
- Report current position

---

# First Tests

## New turnout starts straight

Expected behavior:

```text
Turnout created
        |
        v
Position = Straight
```

## Turnout can throw diverging

Expected behavior:

```text
Turnout
   |
throwDiverging()
   |
Position = Diverging
```

---

# Development Roadmap

## MCS-001 — Foundation

Goals:

- Project created
- CMake configured
- Catch2 installed
- First domain model created
- First tests passing

---

## MCS-002 — Turnout Addresses

Add identification:

Example:

```cpp
Turnout turnout(101);
```

Goals:

- Unique turnout IDs
- Lookup by address
- Validation

---

## MCS-003 — Routes

Model railroad routes:

Examples:

```text
Main Line → Yard
Main Line → Siding
```

Concepts:

- Route
- Multiple turnout changes
- Route validation

---

## MCS-004 — Hardware Abstraction

Introduce interfaces.

Example:

```cpp
class TurnoutDriver
{
public:
    virtual void throwStraight() = 0;
    virtual void throwDiverging() = 0;
};
```

The domain knows what it wants.

The hardware layer knows how to do it.

---

## MCS-005 — Arduino Implementation

Add hardware drivers:

Examples:

- Servo turnout driver
- Relay turnout driver
- LED indicator driver

---

## MCS-006 — LocoNet Integration

Add:

- LocoNet communication
- JMRI commands
- Accessory decoder behavior

---

# Software Practices

## Use TDD

The cycle:

```text
Write failing test
        |
        v
Write minimum code
        |
        v
Refactor
        |
        v
Repeat
```

---

# Keep Domain Separate

Avoid:

```cpp
digitalWrite();
```

inside railroad logic.

Prefer:

```text
Turnout
   |
TurnoutService
   |
TurnoutDriver
   |
Arduino Hardware
```

---

# Future MCS Modules

Potential modules:

- Turnout control
- Signal control
- Occupancy detection
- Sensor nodes
- LocoNet interface
- Dispatcher integration
- Web interface
- Configuration storage

---

# Project Philosophy

MaltBee Control System is not just an Arduino project.

It is a software system built for a model railroad.

The goal is to apply professional software craftsmanship practices to embedded development.

---

# Development Log

## 2026-07-10

Project started.

Initial goals:

- Learn embedded development using Arduino Mega 2560
- Apply professional software practices to embedded programming
- Build a real control system for the Echo Lake & Maltby Railroad

Initial technology choices:

- C++
- CMake
- CLion
- Catch2
- Arduino Framework
- PlatformIO (future)

---

# Future Notes

Use this document as a living development journal.

Record:

- Architecture decisions
- Experiments
- Hardware discoveries
- Lessons learned
- Completed milestones
- Future ideas

The goal is not only to build a railroad controller, but to document the process of applying software craftsmanship principles to embedded systems.

---

# Architecture Decisions

## 2026-07-10 — Turnout Addressing, Routes, Hardware Abstraction

**Turnout addresses**

- Valid range is 1–9,999.
- Uniqueness of addresses is an expectation on how addresses are assigned,
  not enforced by code yet. There is no repository/registry preventing
  duplicates. Revisit later to add enforcement.
- Turnouts will be held in a collection that supports lookup of a
  `Turnout` by its address.

**Routes**

- A `Route` is a list of 1 to n turnouts, each paired with the position
  it should be thrown to when the route is activated. Activating a route
  applies all of its turnout changes as one action.
- Route validation checks:
  - the route is not empty
  - no duplicate turnouts within the same route
  - all referenced turnout addresses exist

**Hardware abstraction**

- `TurnoutDriver` will start as a pure virtual class with no
  implementation. Revisit later to confirm the domain (`TurnoutService` or
  equivalent) is actually wired up to call through a `TurnoutDriver`
  instance rather than talking to hardware directly.
- Of the planned Arduino drivers, the LED indicator driver is needed now.
  Servo and relay turnout drivers are deferred until actually needed.

**LocoNet integration**

- LocoNet communication, JMRI commands, and accessory decoder behavior
  are independent pieces of work. None depends on the others being
  implemented first, even though they interact once the system is
  running end to end.

See [`roadmap-checklist.md`](roadmap-checklist.md) for the checklist
tracking this work.