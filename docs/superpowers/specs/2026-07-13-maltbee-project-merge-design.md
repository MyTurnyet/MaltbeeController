# MaltBee Project Merge Design
**Date:** 2026-07-13  
**Author:** Claude Code  
**Status:** Approved

## Executive Summary

This document describes the plan to merge two MaltBee railroad control system projects into a single unified codebase called **MaltbeeControllerSystem**. The merge uses **MaltbeeController** as the architectural foundation (PlatformIO, hexagonal architecture, ports/adapters) and incrementally ports the domain logic (Turnout, Route classes) from **MaltBee-Control-System**.

The result will be a complete, testable embedded control system targeting the Arduino Mega 2560, with comprehensive domain logic, clean separation of concerns, and a clear path to hardware integration.

---

## Background

### The Two Projects

**MaltBee-Control-System** (older project):
- CMake-based C++ project
- Well-tested domain logic using Catch2
- Classes: `Turnout`, `TurnoutCollection`, `TurnoutService`, `Route`, `RouteService`
- Pure domain code with no hardware abstraction
- No PlatformIO or Arduino integration
- 16 test files with comprehensive coverage

**MaltbeeController** (newer project):
- PlatformIO-based Arduino Mega 2560 project
- Hexagonal architecture with ports/adapters pattern
- Classes: `Button`, `Indicator` (I/O primitives)
- Ports: `DigitalInput`, `DigitalOutput`, `Clock`
- Test doubles: `FakeDigitalInput`, `FakeDigitalOutput`, `FakeClock`
- Unity test framework for native tests
- Comprehensive 1200+ line roadmap document
- Minimal code but correct architectural foundation

### Why Merge?

Both projects are building toward the same goal: a professional, testable embedded control system for model railroad panels. They contain complementary pieces:

- MaltbeeController has the **architectural foundation** (ports, hardware abstraction, PlatformIO setup)
- MaltBee-Control-System has the **domain logic** (turnout/route modeling and coordination)

Merging them creates a complete system that is both architecturally sound and functionally rich.

---

## Project Goals

The merged project will:

1. **Target Arduino Mega 2560** exclusively (not hardware-agnostic)
2. **Use PlatformIO** as the build system
3. **Use Catch2** for all automated testing (native tests only, manual hardware verification)
4. **Follow hexagonal architecture** with ports/adapters separation
5. **Support Test-Driven Development** with fast native test feedback
6. **Preserve git history** from MaltbeeController going forward
7. **Include both I/O primitives and domain logic** (Button, Indicator, Turnout, Route, services)

---

## High-Level Approach

**Strategy: Incremental Port**

Use MaltbeeController as the base, port domain classes from MaltBee-Control-System one at a time, committing and testing after each addition.

**Why incremental?**
- Safest approach - test at each step
- Clear commit history
- Easy to debug issues as they arise
- Can pause/resume between classes
- Lower risk than bulk merge

---

## Architecture

### Directory Structure (Target State)

```
MaltbeeControllerSystem/
├── platformio.ini                      # PlatformIO config (native + megaatmega2560 envs)
├── README.md                           # Project overview, build instructions
├── CLAUDE.md                           # Claude Code guidance
├── .gitignore
├── internal_documents/
│   ├── MaltBee_Control_System_Architecture_and_Roadmap.md  # Main roadmap
│   └── archive/
│       └── original-overview.md        # Historical context from old project
├── lib/
│   └── McsCore/
│       └── src/
│           ├── domain/
│           │   ├── Button.h/.cpp       # [EXISTS] Debounced button
│           │   ├── Indicator.h/.cpp    # [EXISTS] LED indicator
│           │   ├── Turnout.h/.cpp      # [TO PORT] Turnout model
│           │   ├── TurnoutCollection.h/.cpp  # [TO PORT] Turnout registry
│           │   ├── TurnoutService.h/.cpp     # [TO PORT] Turnout coordination
│           │   ├── Route.h/.cpp        # [TO PORT] Route model
│           │   └── RouteService.h/.cpp # [TO PORT] Route execution
│           └── ports/
│               ├── Clock.h             # [EXISTS] Time abstraction
│               ├── DigitalInput.h      # [EXISTS] Input port
│               └── DigitalOutput.h/.cpp # [EXISTS] Output port
├── src/
│   └── main.cpp                        # Arduino entry point (composition root)
├── test/
│   ├── test_button/
│   │   └── test_main.cpp               # [EXISTS, TO CONVERT] Button tests
│   ├── test_indicator/
│   │   └── test_main.cpp               # [EXISTS, TO CONVERT] Indicator tests
│   ├── test_turnout/
│   │   └── test_main.cpp               # [TO CREATE] Turnout tests
│   ├── test_turnout_collection/
│   │   └── test_main.cpp               # [TO CREATE] Collection tests
│   ├── test_turnout_service/
│   │   └── test_main.cpp               # [TO CREATE] Service tests
│   ├── test_route/
│   │   └── test_main.cpp               # [TO CREATE] Route tests
│   ├── test_route_service/
│   │   └── test_main.cpp               # [TO CREATE] RouteService tests
│   └── support/
│       ├── FakeClock.h                 # [EXISTS] Test double
│       ├── FakeDigitalInput.h          # [EXISTS] Test double
│       └── FakeDigitalOutput.h         # [EXISTS] Test double
```

### Technology Stack

- **Language:** C++17
- **Build System:** PlatformIO
- **Target Hardware:** Arduino Mega 2560 (ATmega2560)
- **Test Framework:** Catch2 (native tests only)
- **Architecture:** Hexagonal (Ports & Adapters)
- **Testing Strategy:** TDD with native tests, manual hardware verification

### Testing Strategy

**Automated Testing (Catch2):**
- All domain and application logic tested via native PlatformIO environment
- Tests run on development machine (Mac/Linux/Windows)
- Fast feedback loop (no hardware needed)
- Use test doubles (Fake*) to simulate hardware

**Hardware Verification (Manual):**
- Simple test sketches for hardware adapters
- Manual verification that buttons read correctly, LEDs light up
- No automated on-hardware test suite

**Why this split?**
- 95%+ of code is domain/application logic that doesn't need hardware
- Hardware adapters are thin wrappers that are easier to verify manually
- Keeps test suite fast and maintainable

---

## Detailed Design

### Phase 1: Preparation & Rename

**Objective:** Set up the foundation without breaking anything.

**Actions:**
1. Rename `MaltbeeController/` → `MaltbeeControllerSystem/`
2. Update any internal references to old name (check docs, comments)
3. Verify build still works: `pio test -e native`
4. Verify Mega build: `pio run -e megaatmega2560`

**Git commit:**
```
[r] Rename project to MaltbeeControllerSystem
```

**Verification:**
- Directory renamed
- Project still builds
- Existing tests pass

---

### Phase 2: Set Up Catch2 Testing

**Objective:** Replace Unity with Catch2 for all native tests.

**Current State:**
- MaltbeeController uses Unity (`test_framework = unity` in platformio.ini)
- Two test files use Unity syntax:
  - `test/test_button/test_main.cpp`
  - `test/test_indicator/test_main.cpp`

**Changes:**

**1. Update `platformio.ini`:**

Change the `[env:native]` section from:
```ini
[env:native]
platform = native
test_framework = unity
test_build_src = false
build_flags = -std=c++17
```

To:
```ini
[env:native]
platform = native
test_framework = custom
test_build_src = false
build_flags = -std=c++17
lib_deps =
    catchorg/Catch2@^3.7.1
```

**2. Convert Button tests (Unity → Catch2):**

Unity syntax:
```cpp
void test_button_begins_released(void) {
    TEST_ASSERT_FALSE(button.isPressed());
}
```

Becomes Catch2 syntax:
```cpp
TEST_CASE("Button begins released") {
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, 30);
    
    REQUIRE_FALSE(button.isPressed());
}
```

**3. Convert Indicator tests similarly**

**4. Each test file needs Catch2 main:**
```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
```

**Git commit:**
```
[t] Convert native tests from Unity to Catch2
```

**Verification:**
- `pio test -e native` passes
- All existing Button and Indicator tests still pass
- Test output shows Catch2 format

---

### Phase 3: Port Turnout Domain Class

**Objective:** Bring over the core Turnout class and its tests.

**Source files (from MaltBee-Control-System):**
- `src/domain/Turnout.h`
- `src/domain/Turnout.cpp`
- `test/domain/TurnoutTests.cpp`

**Target location (MaltbeeControllerSystem):**
- `lib/McsCore/src/domain/Turnout.h`
- `lib/McsCore/src/domain/Turnout.cpp`
- `test/test_turnout/test_main.cpp`

**Analysis of Turnout class:**

Looking at the source, the Turnout class:
- Stores turnout address (int, range 1-9999)
- Stores current position (enum: Straight/Diverging or Closed/Thrown)
- Methods: `throwStraight()`, `throwDiverging()`, getters
- Pure domain object, no external dependencies

**Enum alignment:**
- **Old code** uses: `TurnoutPosition::Straight`, `TurnoutPosition::Diverging`
- **Roadmap** suggests: `TurnoutPosition::Closed`, `TurnoutPosition::Thrown`

**Decision:** Use the roadmap's terminology (`Closed`/`Thrown`) to match railroad industry standards and future LocoNet integration. Update ported code if needed.

**Port steps:**
1. Copy `Turnout.h` and `Turnout.cpp` to `lib/McsCore/src/domain/`
2. Update enum values if needed (Straight→Closed, Diverging→Thrown)
3. Create `test/test_turnout/` directory
4. Port tests to `test/test_turnout/test_main.cpp` with Catch2 syntax
5. Update test assertions and structure
6. Run tests: `pio test -e native -f test_turnout`
7. Fix any compilation issues

**Test coverage to port:**
- Turnout stores its address
- Turnout begins in configured initial position
- `throwTurnout()` changes position to thrown
- `closeTurnout()` changes position to closed
- Repeated commands are safe (idempotent)
- Position queries work correctly

**Git commit:**
```
[F] Port Turnout domain class from MaltBee-Control-System

Ports the Turnout class and its test suite. Updates enum values to use
Closed/Thrown terminology to match railroad industry standards.
```

**Verification:**
- `pio test -e native -f test_turnout` passes
- All Turnout tests green

---

### Phase 4: Port TurnoutCollection

**Objective:** Add the collection/registry for managing multiple turnouts.

**Source files:**
- `src/domain/TurnoutCollection.h`
- `src/domain/TurnoutCollection.cpp`
- `test/domain/TurnoutCollectionTests.cpp`

**Target location:**
- `lib/McsCore/src/domain/TurnoutCollection.h`
- `lib/McsCore/src/domain/TurnoutCollection.cpp`
- `test/test_turnout_collection/test_main.cpp`

**Analysis:**

TurnoutCollection provides:
- Add turnouts to the collection
- Lookup turnout by address
- Iterate over all turnouts
- Likely uses `std::map` or `std::vector` internally

**Port steps:**
1. Copy source files to `lib/McsCore/src/domain/`
2. Create test directory and port tests with Catch2 syntax
3. Ensure `#include "Turnout.h"` paths are correct
4. Run tests: `pio test -e native -f test_turnout_collection`

**Git commit:**
```
[F] Port TurnoutCollection domain class
```

**Verification:**
- Collection tests pass
- Can add and retrieve turnouts by address

---

### Phase 5: Port TurnoutService

**Objective:** Add the service layer for turnout operations.

**Source files:**
- `src/domain/TurnoutService.h`
- `src/domain/TurnoutService.cpp`
- `test/domain/TurnoutServiceTests.cpp`

**Target location:**
- `lib/McsCore/src/domain/TurnoutService.h` (or possibly `src/application/`)
- `lib/McsCore/src/domain/TurnoutService.cpp`
- `test/test_turnout_service/test_main.cpp`

**Analysis:**

TurnoutService likely coordinates:
- Operations on turnout collections
- Possibly business rules (validation, state transitions)

**Potential architecture decision:**
- If TurnoutService needs to send commands to hardware (LocoNet), it should depend on `TurnoutCommandPort` (from roadmap)
- This port doesn't exist yet, but we can add it as a pure interface
- For now, TurnoutService might just coordinate domain objects without external I/O

**Port steps:**
1. Copy source files
2. Review for external dependencies
3. If it needs to send commands, create `TurnoutCommandPort` interface in `lib/McsCore/src/ports/`
4. Add port parameter to constructor if needed
5. Create test directory, port tests
6. Create `FakeTurnoutCommandPort` test double if needed
7. Run tests: `pio test -e native -f test_turnout_service`

**Git commit:**
```
[F] Port TurnoutService

Coordinates turnout operations across the collection.
[Adds TurnoutCommandPort interface if needed for external commands.]
```

**Verification:**
- TurnoutService tests pass
- Service correctly coordinates turnout operations

---

### Phase 6: Port Route and RouteService

**Objective:** Add route support (sequences of turnout commands).

**6.1: Port Route class**

**Source files:**
- `src/domain/Route.h`
- `src/domain/Route.cpp`
- `test/domain/RouteTests.cpp`

**Target:**
- `lib/McsCore/src/domain/Route.h`
- `lib/McsCore/src/domain/Route.cpp`
- `test/test_route/test_main.cpp`

**Analysis:**

A Route represents:
- Route ID/name
- List of turnout commands (address + desired position)
- Query methods

**Port steps:**
1. Copy source files to domain
2. Port tests with Catch2 syntax
3. Update any enum references (Straight/Diverging → Closed/Thrown)
4. Run tests: `pio test -e native -f test_route`

**Git commit:**
```
[F] Port Route domain class
```

**6.2: Port RouteService**

**Source files:**
- `src/domain/RouteService.h`
- `src/domain/RouteService.cpp`
- `test/domain/RouteServiceTests.cpp`

**Target:**
- `lib/McsCore/src/domain/RouteService.h` (or `src/application/`)
- `lib/McsCore/src/domain/RouteService.cpp`
- `test/test_route_service/test_main.cpp`

**Analysis:**

RouteService:
- Executes a route (sends multiple turnout commands)
- Likely depends on `TurnoutCommandPort` to send commands
- May coordinate with TurnoutService or TurnoutCollection

**Port steps:**
1. Copy source files
2. Wire up to `TurnoutCommandPort` if needed
3. Port tests with fake port
4. Run tests: `pio test -e native -f test_route_service`

**Git commit:**
```
[F] Port RouteService

Executes routes by sending sequences of turnout commands through
the TurnoutCommandPort.
```

**Verification:**
- Route and RouteService tests pass
- Full test suite passes: `pio test -e native`

---

### Phase 7: Documentation Merge

**Objective:** Consolidate and update all documentation to reflect merged state.

**Actions:**

**1. Archive historical documentation:**
- Create `internal_documents/archive/` directory
- Copy `MaltBee-Control-System/internal-documents/overview.md` → `internal_documents/archive/original-overview.md`
- Add note at top: "This document is archived for historical context. The project was merged into MaltbeeControllerSystem on 2026-07-13."

**2. Extract and merge architecture decisions:**

From `overview.md`, extract these key decisions:
- Turnout addressing (1-9999 range, uniqueness expectations)
- Route validation rules (no duplicates, all addresses must exist)
- Hardware abstraction principles
- TDD workflow

Add them to the roadmap document under a new section:
```markdown
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
```

**3. Update roadmap status:**

Mark completed milestones in `MaltBee_Control_System_Architecture_and_Roadmap.md`:

```markdown
## Development Milestones (UPDATED 2026-07-13)

### ✅ Milestone 1: Establish the Testing Foundation
**Status:** COMPLETE

### ✅ Milestone 2: Build Digital Input and Output Ports
**Status:** COMPLETE

### ✅ Milestone 3: TDD the Indicator
**Status:** COMPLETE

### ✅ Milestone 4: TDD the Debounced Button
**Status:** COMPLETE

### ✅ Milestone 5: TDD the Turnout Domain Model
**Status:** COMPLETE (ported from MaltBee-Control-System)

### → Milestone 6: TDD Turnout Indicators
**Status:** NEXT - Ready to implement

[Continue with remaining milestones...]
```

Add a new section at the top:
```markdown
## Project Merge (2026-07-13)

This project is the result of merging two MaltBee projects:
- **MaltbeeController** (architectural foundation, ports/adapters, Button/Indicator)
- **MaltBee-Control-System** (domain logic: Turnout, Route, services)

The merge used MaltbeeController as the base and incrementally ported domain classes
from MaltBee-Control-System. All code now uses Catch2 for testing and targets the
Arduino Mega 2560 via PlatformIO.

Milestones 1-5 are complete. Next: TurnoutIndicator (Milestone 6).
```

**4. Update README.md:**

Update the main README to reflect:
- New project name: MaltbeeControllerSystem
- Current status (what's implemented)
- Build and test commands
- What's next

Example:
```markdown
# MaltBee Control System (MCS)

An embedded railroad control system for model railroad panels, targeting the Arduino Mega 2560.

Built with professional software engineering practices: Test-Driven Development, Hexagonal
Architecture, and Dependency Inversion.

## Current Status

The project has completed Milestones 1-5:
- ✅ Native test environment with Catch2
- ✅ Digital I/O ports (DigitalInput, DigitalOutput, Clock)
- ✅ Domain classes: Button, Indicator, Turnout, TurnoutCollection, Route
- ✅ Service classes: TurnoutService, RouteService
- ✅ Test doubles for all ports

**Next:** TurnoutIndicator (connect turnout state to panel LEDs)

See `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md` for
the complete development plan.

## Building and Testing

```bash
# Run native unit tests
pio test -e native

# Run a specific test file
pio test -e native -f test_turnout

# Build firmware for Arduino Mega
pio run -e megaatmega2560

# Upload to Mega
pio run -e megaatmega2560 --target upload

# Serial monitor
pio device monitor
```

## Architecture

This project uses Hexagonal Architecture with Ports & Adapters:

- **Domain Layer:** Pure C++ business logic (Turnout, Button, Route, etc.)
- **Ports:** Interfaces that define what the domain needs (DigitalInput, Clock, etc.)
- **Adapters:** Implementations that connect ports to hardware (ArduinoDigitalInput, etc.)
- **Application Layer:** Use cases and coordination (TurnoutControl, RouteExecutionService)

The domain and application layers compile and run natively (no Arduino required)
for fast test feedback.
```

**5. Update CLAUDE.md:**

Update the project guidance for Claude Code:
- Note that Turnout/Route classes are now present
- Update the "Current source layout" section
- Mark milestones 1-5 as complete
- Update test commands

**Git commit:**
```
[C] Merge documentation and update project status

Archives original MaltBee-Control-System overview, merges architectural
decisions into roadmap, updates README and CLAUDE.md to reflect
merged state and completed milestones 1-5.
```

**Verification:**
- All documentation is accurate and up-to-date
- Historical context preserved
- Clear picture of what's done and what's next

---

### Phase 8: Update Build Configuration

**Objective:** Ensure build configuration is complete and correct.

**Actions:**

**1. Review `platformio.ini`:**

Verify it has:
```ini
[platformio]
default_envs = megaatmega2560

[env:megaatmega2560]
platform = atmelavr
board = megaatmega2560
framework = arduino
monitor_speed = 115200
lib_ldf_mode = deep+

[env:native]
platform = native
test_framework = custom
test_build_src = false
build_flags = -std=c++17
lib_deps =
    catchorg/Catch2@^3.7.1
```

**2. Verify library dependencies are ready for future:**
- MRRWA LocoNet library (not needed yet, will be added in Milestone 9)
- No other external dependencies needed for current milestones

**3. Confirm CLAUDE.md is accurate:**

Update CLAUDE.md to show current state:
```markdown
## Current source layout

- `lib/McsCore/src/domain/` — domain classes (Button, Indicator, Turnout, TurnoutCollection, Route)
- `lib/McsCore/src/ports/` — port interfaces (DigitalInput, DigitalOutput, Clock)
- `src/main.cpp` — composition root (Arduino entry point)
- `test/test_<name>/test_main.cpp` — Catch2 test binaries
- `test/support/` — test doubles (FakeDigitalInput, FakeClock, FakeDigitalOutput)

Test coverage:
- ✅ Button (debouncing, edge detection)
- ✅ Indicator (on/off control)
- ✅ Turnout (position, address)
- ✅ TurnoutCollection (registry, lookup)
- ✅ TurnoutService (coordination)
- ✅ Route (command sequences)
- ✅ RouteService (execution)

Next milestones: TurnoutIndicator, TurnoutControl, hardware integration.
```

**Git commit:**
```
[C] Update build configuration and CLAUDE.md

Confirms platformio.ini is correctly configured for Catch2 native tests.
Updates CLAUDE.md to reflect merged codebase structure.
```

**Verification:**
- Build configuration is correct
- No unused dependencies
- Documentation matches reality

---

### Phase 9: Full Test Suite Verification

**Objective:** Verify everything works together.

**Actions:**

1. **Run complete native test suite:**
   ```bash
   pio test -e native
   ```

2. **Verify Mega build:**
   ```bash
   pio run -e megaatmega2560
   ```

3. **Check for issues:**
   - All tests pass
   - No compilation errors
   - No warnings (or document acceptable warnings)

4. **Document test results:**
   - Number of test cases
   - Number of test files
   - All passing

**If issues found:**
- Fix compilation errors
- Fix failing tests
- Commit: `[t] Fix integration issues in merged test suite`

**If no issues:**
- No commit needed, proceed to next phase

**Verification:**
- ✅ All native tests pass
- ✅ Mega firmware builds successfully
- ✅ No blocking issues

---

### Phase 10: Cleanup

**Objective:** Archive the old project directory.

**Actions:**

**1. Add README to old project:**

Create or update `MaltBee-Control-System/README.md`:
```markdown
# MaltBee Control System (ARCHIVED)

⚠️ **This project has been merged into MaltbeeControllerSystem**

This repository contained the original domain logic (Turnout, Route classes)
that has been ported into the unified MaltbeeControllerSystem project.

See: `../MaltbeeControllerSystem/`

Archived: 2026-07-13

---

[Original README content preserved below]
[...]
```

**2. Optionally rename directory:**
```bash
mv MaltBee-Control-System MaltBee-Control-System-ARCHIVED
```

**3. Update CLion workspace:**
- Remove old project from workspace if it's still there
- Ensure new MaltbeeControllerSystem project is open

**No git commit** - this is workspace/filesystem organization.

**Verification:**
- Old project clearly marked as archived
- No confusion about which project to use
- CLion/IDE shows correct project

---

## Implementation Checklist

- [x] **Phase 1:** Rename MaltbeeController → MaltbeeControllerSystem
- [x] **Phase 2:** Convert existing tests from Unity to Catch2
- [x] **Phase 3:** Port Turnout class and tests
- [x] **Phase 4:** Port TurnoutCollection class and tests
- [x] **Phase 5:** Port TurnoutService class and tests
- [x] **Phase 6:** Port Route and RouteService classes and tests
- [x] **Phase 7:** Merge and update documentation
- [x] **Phase 8:** Update build configuration
- [x] **Phase 9:** Verify full test suite passes
- [x] **Phase 10:** Archive old MaltBee-Control-System project

**Status (2026-07-16):** All 10 merge phases complete. The merge itself is done; the project has since moved on to Milestones 6-8 (TurnoutIndicator, TurnoutControl, hardware-integration programming) beyond the scope of this design doc. Remaining work is tracked in `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`, not here — see Milestone 8's outstanding hardware wiring/verification step and Milestone 9+ (LocoNet).

---

## Success Criteria

The merge is complete and successful when:

✅ **Directory renamed:** `MaltbeeControllerSystem` exists and builds  
✅ **Testing:** All native tests use Catch2 and pass  
✅ **Domain classes ported:** Turnout, TurnoutCollection, TurnoutService, Route, RouteService exist with full test coverage  
✅ **Documentation updated:** README, CLAUDE.md, roadmap reflect merged state  
✅ **Build verified:** `pio test -e native` passes, `pio run -e megaatmega2560` builds  
✅ **Git history clean:** Each phase has clear, descriptive commit  
✅ **Old project archived:** MaltBee-Control-System clearly marked as archived  
✅ **Ready for next milestone:** Milestone 6 (TurnoutIndicator) can begin immediately

---

## Risk Assessment & Mitigation

### Risk: Enum value mismatch
**Description:** Old code uses Straight/Diverging, roadmap uses Closed/Thrown  
**Impact:** Medium - compilation errors or semantic confusion  
**Mitigation:** Align to Closed/Thrown during Phase 3, update all references consistently  
**Owner:** Implementation phase 3

### Risk: Test conversion errors
**Description:** Unity→Catch2 conversion might miss edge cases  
**Impact:** Low - tests might not catch bugs they used to  
**Mitigation:** Review converted tests carefully, run full suite, compare test count before/after  
**Owner:** Implementation phase 2

### Risk: Dependency issues between classes
**Description:** Ported classes might have circular dependencies or missing includes  
**Impact:** Medium - compilation failures  
**Mitigation:** Port in dependency order (Turnout before TurnoutCollection), test after each port  
**Owner:** Implementation phases 3-6

### Risk: TurnoutCommandPort not yet defined
**Description:** TurnoutService or RouteService might need this port but it doesn't exist  
**Impact:** Low - easy to add  
**Mitigation:** Create minimal port interface as needed, implement fully later  
**Owner:** Implementation phase 5-6

---

## Future Work (Post-Merge)

After the merge is complete, the project will be ready for:

**Milestone 6: TDD Turnout Indicators**
- Create `TurnoutIndicator` class
- Connect turnout state to two LEDs (thrown/closed)
- Test with fake outputs

**Milestone 7: TDD Turnout Control Use Case**
- Create `TurnoutControl` application class
- Wire buttons → turnout commands → indicators
- Test full control loop with fakes

**Milestone 8: Hardware Integration**
- Wire physical buttons and LEDs to Mega
- Create Arduino adapters
- Compose in `main.cpp`
- Manual verification on hardware

**Milestone 9+: LocoNet, Routes, Config, Hardening**
- Add MRRWA LocoNet library
- Implement LocoNet send/receive
- Multi-turnout support
- Route buttons
- EEPROM configuration
- Production hardening

---

## Appendix A: Key Architectural Decisions

### ADR-1: Use MaltbeeController as Foundation
**Context:** Two projects with different foundations (CMake vs PlatformIO)  
**Decision:** Use MaltbeeController as base because it has correct architecture (ports/adapters) and PlatformIO setup  
**Consequences:** Must port domain logic incrementally; CMake setup discarded  
**Status:** Accepted

### ADR-2: Use Catch2 for All Native Tests
**Context:** MaltbeeController uses Unity, MaltBee-Control-System uses Catch2  
**Decision:** Standardize on Catch2 for all native tests, no automated on-hardware tests  
**Consequences:** More expressive tests, must convert existing Unity tests  
**Status:** Accepted

### ADR-3: Closed/Thrown Terminology
**Context:** Old code uses Straight/Diverging, roadmap suggests Closed/Thrown  
**Decision:** Use Closed/Thrown to match railroad industry standards and LocoNet protocol  
**Consequences:** Must update enum values during port  
**Status:** Accepted

### ADR-4: No Automated On-Hardware Tests
**Context:** Could use Unity for on-hardware adapter tests  
**Decision:** Manual verification only, no automated on-hardware test suite  
**Consequences:** Faster test suite, simpler setup; manual steps needed for hardware  
**Status:** Accepted

### ADR-5: Incremental Port Strategy
**Context:** Could port all at once (bulk) or one class at a time (incremental)  
**Decision:** Port incrementally with test-after-each-class approach  
**Consequences:** More commits, safer, easier to debug  
**Status:** Accepted

---

## Appendix B: File Mapping

| Source (MaltBee-Control-System) | Destination (MaltbeeControllerSystem) | Phase |
|--------------------------------|---------------------------------------|-------|
| `src/domain/Turnout.h` | `lib/McsCore/src/domain/Turnout.h` | 3 |
| `src/domain/Turnout.cpp` | `lib/McsCore/src/domain/Turnout.cpp` | 3 |
| `test/domain/TurnoutTests.cpp` | `test/test_turnout/test_main.cpp` | 3 |
| `src/domain/TurnoutCollection.h` | `lib/McsCore/src/domain/TurnoutCollection.h` | 4 |
| `src/domain/TurnoutCollection.cpp` | `lib/McsCore/src/domain/TurnoutCollection.cpp` | 4 |
| `test/domain/TurnoutCollectionTests.cpp` | `test/test_turnout_collection/test_main.cpp` | 4 |
| `src/domain/TurnoutService.h` | `lib/McsCore/src/domain/TurnoutService.h` | 5 |
| `src/domain/TurnoutService.cpp` | `lib/McsCore/src/domain/TurnoutService.cpp` | 5 |
| `test/domain/TurnoutServiceTests.cpp` | `test/test_turnout_service/test_main.cpp` | 5 |
| `src/domain/Route.h` | `lib/McsCore/src/domain/Route.h` | 6 |
| `src/domain/Route.cpp` | `lib/McsCore/src/domain/Route.cpp` | 6 |
| `test/domain/RouteTests.cpp` | `test/test_route/test_main.cpp` | 6 |
| `src/domain/RouteService.h` | `lib/McsCore/src/domain/RouteService.h` | 6 |
| `src/domain/RouteService.cpp` | `lib/McsCore/src/domain/RouteService.cpp` | 6 |
| `test/domain/RouteServiceTests.cpp` | `test/test_route_service/test_main.cpp` | 6 |
| `internal-documents/overview.md` | `internal_documents/archive/original-overview.md` | 7 |

---

## Appendix C: Test Count Before/After

**MaltBee-Control-System (before merge):**
- 16 test files
- Catch2 framework
- Focus: Turnout, TurnoutCollection, TurnoutService, Route, RouteService

**MaltbeeController (before merge):**
- 2 test files (Button, Indicator)
- Unity framework

**MaltbeeControllerSystem (after merge):**
- 7 test files expected:
  - test_button
  - test_indicator
  - test_turnout
  - test_turnout_collection
  - test_turnout_service
  - test_route
  - test_route_service
- All Catch2 framework
- Full coverage of all domain classes

---

## Conclusion

This incremental merge strategy provides a safe, testable path to combine the best of both MaltBee projects. By using MaltbeeController's architectural foundation and porting MaltBee-Control-System's domain logic one class at a time, we create a unified, maintainable codebase that is ready for hardware integration and future feature development.

The resulting **MaltbeeControllerSystem** project will be professionally architected, comprehensively tested, and positioned to complete the full roadmap from panel I/O through LocoNet integration to production deployment.
