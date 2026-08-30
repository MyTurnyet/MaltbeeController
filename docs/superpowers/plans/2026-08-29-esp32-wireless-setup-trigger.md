# ESP32 Wireless Setup Boot Mode & Trigger (Sub-project #2c-a) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the five small domain/port/adapter classes that let an ESP32 turnout panel decide which boot mode to enter (Normal / NeedsCommissioning / WirelessSetup) and detect a two-button-hold gesture to request wireless setup on the next reboot, with normal turnout-button handling suppressed for the two combo buttons while the gesture is in progress.

**Architecture:** Pure domain/adapter classes depending only on existing ports (`DigitalInput`, `Clock`) and `NodeConfig` — no `main.cpp` wiring, no changes to `Button`/`MatrixScanner`/`ToggleTurnoutControl`/`ToggleTurnoutStation`. Only one file (`NvsSetupModeRequestStore`) is a real hardware shim.

**Tech Stack:** C++17, PlatformIO (`native` for tests, `esp32dev` build-check for the one hardware shim), Catch2.

## Global Constraints

- No `Arduino.h` dependency in domain/port/adapter code except the one `#ifdef ARDUINO`-guarded hardware shim (`NvsSetupModeRequestStore`).
- `BootMode`/`BootModeSelector`/`GatedDigitalInput`/`ComboSetupModeTrigger` must be fully native-testable using existing test doubles (`FakeDigitalInput`, `FakeClock` in `test/support/`) plus one new test double this plan adds (`FakeSetupModeRequestStore`).
- `NvsSetupModeRequestStore` persists its flag in NVS namespace `"mcs-boot"`, key `"wsetup"` — a separate namespace from `NvsConfigStore`'s configuration namespace, since this is a transient boot-intent flag, not configuration. `consumeRequest()` clears the flag as part of reading it (read-and-clear).
- `ComboSetupModeTrigger`'s hold timer starts from the *later* of the two button presses (when both first become simultaneously active), not the first one alone. Releasing *either* button ends the joint hold and is evaluated for `requested()` the same way, regardless of which one released first.
- This plan does not touch `src/esp32/main.cpp`, `Button`, `MatrixScanner`, `MatrixDigitalInput`, `ToggleTurnoutControl`, or `ToggleTurnoutStation` — wiring these five classes into the composition root, choosing the real T1/T2 `MatrixDigitalInput`s, and calling `ESP.restart()` on a satisfied request are all sub-project #2c-b's job.
- All four tasks below are mutually independent (none depends on another's output) — ordered for reviewability, not dependency.

---

### Task 1: `BootMode` and `BootModeSelector`

**Files:**
- Create: `lib/McsEsp32/src/domain/BootMode.h`
- Create: `lib/McsEsp32/src/domain/BootModeSelector.h`
- Test: `test/test_boot_mode_selector/test_main.cpp`

**Interfaces:**
- Consumes: `NodeConfig` (`lib/McsEsp32/src/domain/NodeConfig.h`) — `static NodeConfig factoryDefault()`, `NodeConfig withNodeId(int) const`, `NodeConfig withWifi(std::string, std::string) const`, `NodeConfig withBroker(std::string, int) const`, `std::vector<std::string> validate() const` (empty = valid).
- Produces (consumed by sub-project #2c-b, not by any task in this plan):
  ```cpp
  enum class BootMode { Normal, NeedsCommissioning, WirelessSetup };

  class BootModeSelector
  {
  public:
      static BootMode select(const NodeConfig& config, bool wirelessSetupRequested);
  };
  ```

- [ ] **Step 1: Write the failing test file**

Create `test/test_boot_mode_selector/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/BootModeSelector.h"
#include "domain/NodeConfig.h"

namespace
{
    NodeConfig validConfig()
    {
        return NodeConfig::factoryDefault()
            .withNodeId(1)
            .withWifi("MyLayoutWifi", "hunter2")
            .withBroker("192.168.1.50", 1883);
    }
}

TEST_CASE("a pending wireless setup request selects WirelessSetup even with an invalid config")
{
    const NodeConfig config = NodeConfig::factoryDefault();

    REQUIRE(BootModeSelector::select(config, true) == BootMode::WirelessSetup);
}

TEST_CASE("a pending wireless setup request selects WirelessSetup even with a valid config")
{
    REQUIRE(BootModeSelector::select(validConfig(), true) == BootMode::WirelessSetup);
}

TEST_CASE("no request and a valid config selects Normal")
{
    REQUIRE(BootModeSelector::select(validConfig(), false) == BootMode::Normal);
}

TEST_CASE("no request and an invalid config selects NeedsCommissioning")
{
    const NodeConfig config = NodeConfig::factoryDefault();

    REQUIRE(BootModeSelector::select(config, false) == BootMode::NeedsCommissioning);
}
```

- [ ] **Step 2: Run the test to verify it fails to compile**

Run: `pio test -e native -f test_boot_mode_selector`
Expected: FAIL — `BootModeSelector.h` does not exist yet.

- [ ] **Step 3: Write `BootMode.h`**

Create `lib/McsEsp32/src/domain/BootMode.h`:

```cpp
#pragma once

enum class BootMode
{
    Normal,
    NeedsCommissioning,
    WirelessSetup
};
```

- [ ] **Step 4: Write `BootModeSelector.h`**

Create `lib/McsEsp32/src/domain/BootModeSelector.h`:

```cpp
#pragma once

#include "BootMode.h"
#include "NodeConfig.h"

class BootModeSelector
{
public:
    static BootMode select(const NodeConfig& config, const bool wirelessSetupRequested)
    {
        if (wirelessSetupRequested)
        {
            return BootMode::WirelessSetup;
        }

        return config.validate().empty() ? BootMode::Normal : BootMode::NeedsCommissioning;
    }
};
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_boot_mode_selector`
Expected: PASS — 4 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites pass (30/30 including the new one).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/domain/BootMode.h lib/McsEsp32/src/domain/BootModeSelector.h test/test_boot_mode_selector/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add BootMode and BootModeSelector
EOF
)"
```

---

### Task 2: `SetupModeRequestStore` port, `NvsSetupModeRequestStore` adapter, `FakeSetupModeRequestStore`

**Files:**
- Create: `lib/McsEsp32/src/ports/SetupModeRequestStore.h`
- Create: `lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.h`
- Create: `lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.cpp`
- Create: `test/support/FakeSetupModeRequestStore.h`

**Interfaces:**
- Consumes: nothing from other tasks in this plan.
- Produces (consumed by sub-project #2c-b, not by any task in this plan):
  ```cpp
  class SetupModeRequestStore
  {
  public:
      virtual ~SetupModeRequestStore() = default;
      virtual void requestOnNextBoot() = 0;
      virtual bool consumeRequest() = 0;  // read-and-clear
  };
  ```
  `NvsSetupModeRequestStore` and `FakeSetupModeRequestStore` both implement this port.

This task has no dedicated native test — `NvsSetupModeRequestStore` is a real hardware shim (same convention as `NvsConfigStore`, verified only by a build-check), and `FakeSetupModeRequestStore` is a test double, not itself under test (matches this project's convention: `FakeConfigStore` has no dedicated test file either).

- [ ] **Step 1: Write the port**

Create `lib/McsEsp32/src/ports/SetupModeRequestStore.h`:

```cpp
#pragma once

class SetupModeRequestStore
{
public:
    virtual ~SetupModeRequestStore() = default;

    virtual void requestOnNextBoot() = 0;
    virtual bool consumeRequest() = 0;
};
```

- [ ] **Step 2: Write the NVS adapter header**

Create `lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include "../ports/SetupModeRequestStore.h"

class NvsSetupModeRequestStore final : public SetupModeRequestStore
{
public:
    void requestOnNextBoot() override;
    bool consumeRequest() override;
};

#endif
```

- [ ] **Step 3: Write the NVS adapter implementation**

Create `lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.cpp`:

```cpp
#ifdef ARDUINO

#include "NvsSetupModeRequestStore.h"

#include <Preferences.h>

namespace
{
    constexpr const char* kNamespace = "mcs-boot";
    constexpr const char* kKey = "wsetup";
}

void NvsSetupModeRequestStore::requestOnNextBoot()
{
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.putBool(kKey, true);
    prefs.end();
}

bool NvsSetupModeRequestStore::consumeRequest()
{
    Preferences prefs;
    prefs.begin(kNamespace, false);
    const bool pending = prefs.getBool(kKey, false);
    if (pending)
    {
        prefs.putBool(kKey, false);
    }
    prefs.end();
    return pending;
}

#endif
```

- [ ] **Step 4: Write the fake test double**

Create `test/support/FakeSetupModeRequestStore.h`:

```cpp
#pragma once

#include "ports/SetupModeRequestStore.h"

class FakeSetupModeRequestStore final : public SetupModeRequestStore
{
public:
    int requestOnNextBootCallCount = 0;

    void requestOnNextBoot() override
    {
        requested_ = true;
        requestOnNextBootCallCount++;
    }

    bool consumeRequest() override
    {
        const bool pending = requested_;
        requested_ = false;
        return pending;
    }

private:
    bool requested_ = false;
};
```

- [ ] **Step 5: Build-check the ESP32 target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — `esp32dev`'s `lib_deps` already includes `McsEsp32` as a full library dependency (see `platformio.ini`), so this new `.cpp` is compiled automatically without needing to be referenced from `src/esp32/main.cpp`.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites still pass (30/30, unchanged from Task 1 — this task adds no native test of its own).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/ports/SetupModeRequestStore.h lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.h lib/McsEsp32/src/adapters/NvsSetupModeRequestStore.cpp test/support/FakeSetupModeRequestStore.h
git commit -m "$(cat <<'EOF'
^ F Add SetupModeRequestStore port and NVS-backed adapter
EOF
)"
```

---

### Task 3: `GatedDigitalInput`

**Files:**
- Create: `lib/McsEsp32/src/adapters/GatedDigitalInput.h`
- Create: `lib/McsEsp32/src/adapters/GatedDigitalInput.cpp`
- Test: `test/test_gated_digital_input/test_main.cpp`

**Interfaces:**
- Consumes: `DigitalInput` (`lib/McsCore/src/ports/DigitalInput.h`) — `virtual bool isActive() const = 0;`. `FakeDigitalInput` (`test/support/FakeDigitalInput.h`) — public `bool active` field.
- Produces (consumed by sub-project #2c-b, not by any task in this plan):
  ```cpp
  class GatedDigitalInput final : public DigitalInput
  {
  public:
      explicit GatedDigitalInput(DigitalInput& inner);
      void setSuppressed(bool suppressed);
      bool isActive() const override;
  };
  ```

- [ ] **Step 1: Write the failing test file**

Create `test/test_gated_digital_input/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/GatedDigitalInput.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("forwards the inner reading when not suppressed")
{
    FakeDigitalInput inner;
    GatedDigitalInput gated(inner);

    inner.active = true;
    REQUIRE(gated.isActive());

    inner.active = false;
    REQUIRE_FALSE(gated.isActive());
}

TEST_CASE("returns false when suppressed regardless of the inner reading")
{
    FakeDigitalInput inner;
    GatedDigitalInput gated(inner);

    inner.active = true;
    gated.setSuppressed(true);

    REQUIRE_FALSE(gated.isActive());
}

TEST_CASE("forwarding resumes after setSuppressed(false)")
{
    FakeDigitalInput inner;
    GatedDigitalInput gated(inner);

    inner.active = true;
    gated.setSuppressed(true);
    REQUIRE_FALSE(gated.isActive());

    gated.setSuppressed(false);
    REQUIRE(gated.isActive());
}
```

- [ ] **Step 2: Run the test to verify it fails to compile**

Run: `pio test -e native -f test_gated_digital_input`
Expected: FAIL — `GatedDigitalInput.h` does not exist yet.

- [ ] **Step 3: Write the header**

Create `lib/McsEsp32/src/adapters/GatedDigitalInput.h`:

```cpp
#pragma once

#include "ports/DigitalInput.h"

class GatedDigitalInput final : public DigitalInput
{
public:
    explicit GatedDigitalInput(DigitalInput& inner);

    void setSuppressed(bool suppressed);
    [[nodiscard]] bool isActive() const override;

private:
    DigitalInput& inner_;
    bool suppressed_ = false;
};
```

- [ ] **Step 4: Write the implementation**

Create `lib/McsEsp32/src/adapters/GatedDigitalInput.cpp`:

```cpp
#include "GatedDigitalInput.h"

GatedDigitalInput::GatedDigitalInput(DigitalInput& inner) : inner_(inner)
{
}

void GatedDigitalInput::setSuppressed(const bool suppressed)
{
    suppressed_ = suppressed;
}

bool GatedDigitalInput::isActive() const
{
    return !suppressed_ && inner_.isActive();
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_gated_digital_input`
Expected: PASS — 3 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites pass (31/31 including this task's new suite — Task 2 added no native test).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/adapters/GatedDigitalInput.h lib/McsEsp32/src/adapters/GatedDigitalInput.cpp test/test_gated_digital_input/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add GatedDigitalInput
EOF
)"
```

---

### Task 4: `ComboSetupModeTrigger`

**Files:**
- Create: `lib/McsEsp32/src/adapters/ComboSetupModeTrigger.h`
- Create: `lib/McsEsp32/src/adapters/ComboSetupModeTrigger.cpp`
- Test: `test/test_combo_setup_mode_trigger/test_main.cpp`

**Interfaces:**
- Consumes: `DigitalInput` (`lib/McsCore/src/ports/DigitalInput.h`), `Clock` (`lib/McsCore/src/ports/Clock.h`) — `virtual unsigned long nowMilliseconds() const = 0;`. `FakeDigitalInput`/`FakeClock` (`test/support/`) — `FakeClock` has `void advanceBy(unsigned long)`.
- Produces (consumed by sub-project #2c-b, not by any task in this plan):
  ```cpp
  class ComboSetupModeTrigger
  {
  public:
      ComboSetupModeTrigger(DigitalInput& buttonA, DigitalInput& buttonB, Clock& clock,
                             unsigned long minHoldMs);
      void update();
      bool isHolding() const;
      bool requested() const;
  };
  ```

- [ ] **Step 1: Write the failing test file**

Create `test/test_combo_setup_mode_trigger/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/ComboSetupModeTrigger.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"

namespace
{
    constexpr unsigned long MIN_HOLD_MS = 3000;
}

TEST_CASE("isHolding is false initially and while only one button is active")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    trigger.update();
    REQUIRE_FALSE(trigger.isHolding());

    buttonA.active = true;
    trigger.update();
    REQUIRE_FALSE(trigger.isHolding());
}

TEST_CASE("isHolding becomes true once both buttons are simultaneously active")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();

    REQUIRE(trigger.isHolding());
}

TEST_CASE("requested stays false if released before minHoldMs elapses")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS - 1);
    buttonA.active = false;
    trigger.update();

    REQUIRE_FALSE(trigger.requested());
    REQUIRE_FALSE(trigger.isHolding());
}

TEST_CASE("requested fires exactly once on the release tick after meeting minHoldMs, then resets")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS);
    buttonA.active = false;
    buttonB.active = false;
    trigger.update();

    REQUIRE(trigger.requested());
    REQUIRE_FALSE(trigger.isHolding());

    trigger.update();
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("releasing only one of the two buttons still ends the joint hold and can fire requested")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS);
    buttonA.active = false;
    // buttonB remains active - only one released
    trigger.update();

    REQUIRE(trigger.requested());
    REQUIRE_FALSE(trigger.isHolding());
}

TEST_CASE("a fresh press-hold-release cycle after a full release can trigger again")
{
    FakeDigitalInput buttonA;
    FakeDigitalInput buttonB;
    FakeClock clock;
    ComboSetupModeTrigger trigger(buttonA, buttonB, clock, MIN_HOLD_MS);

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();
    clock.advanceBy(MIN_HOLD_MS);
    buttonA.active = false;
    buttonB.active = false;
    trigger.update();
    REQUIRE(trigger.requested());

    trigger.update();
    REQUIRE_FALSE(trigger.requested());

    buttonA.active = true;
    buttonB.active = true;
    trigger.update();
    clock.advanceBy(MIN_HOLD_MS);
    buttonA.active = false;
    buttonB.active = false;
    trigger.update();

    REQUIRE(trigger.requested());
}
```

- [ ] **Step 2: Run the test to verify it fails to compile**

Run: `pio test -e native -f test_combo_setup_mode_trigger`
Expected: FAIL — `ComboSetupModeTrigger.h` does not exist yet.

- [ ] **Step 3: Write the header**

Create `lib/McsEsp32/src/adapters/ComboSetupModeTrigger.h`:

```cpp
#pragma once

#include "ports/Clock.h"
#include "ports/DigitalInput.h"

class ComboSetupModeTrigger
{
public:
    ComboSetupModeTrigger(DigitalInput& buttonA, DigitalInput& buttonB, Clock& clock,
                           unsigned long minHoldMs);

    void update();
    [[nodiscard]] bool isHolding() const;
    [[nodiscard]] bool requested() const;

private:
    DigitalInput& buttonA_;
    DigitalInput& buttonB_;
    Clock& clock_;
    unsigned long minHoldMs_;
    bool holding_ = false;
    unsigned long holdStartMs_ = 0;
    bool requestedThisTick_ = false;
};
```

- [ ] **Step 4: Write the implementation**

Create `lib/McsEsp32/src/adapters/ComboSetupModeTrigger.cpp`:

```cpp
#include "ComboSetupModeTrigger.h"

ComboSetupModeTrigger::ComboSetupModeTrigger(DigitalInput& buttonA, DigitalInput& buttonB, Clock& clock,
                                              const unsigned long minHoldMs)
    : buttonA_(buttonA), buttonB_(buttonB), clock_(clock), minHoldMs_(minHoldMs)
{
}

void ComboSetupModeTrigger::update()
{
    requestedThisTick_ = false;
    const bool bothActive = buttonA_.isActive() && buttonB_.isActive();

    if (bothActive && !holding_)
    {
        holding_ = true;
        holdStartMs_ = clock_.nowMilliseconds();
    }
    else if (!bothActive && holding_)
    {
        holding_ = false;
        const unsigned long heldFor = clock_.nowMilliseconds() - holdStartMs_;
        if (heldFor >= minHoldMs_)
        {
            requestedThisTick_ = true;
        }
    }
}

bool ComboSetupModeTrigger::isHolding() const
{
    return holding_;
}

bool ComboSetupModeTrigger::requested() const
{
    return requestedThisTick_;
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_combo_setup_mode_trigger`
Expected: PASS — 6 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites pass (32/32 including this task's new suite).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/adapters/ComboSetupModeTrigger.h lib/McsEsp32/src/adapters/ComboSetupModeTrigger.cpp test/test_combo_setup_mode_trigger/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add ComboSetupModeTrigger
EOF
)"
```

---

## Self-review notes

- **Spec coverage:** All five components (`BootMode`, `BootModeSelector`, `SetupModeRequestStore`/`NvsSetupModeRequestStore`/`FakeSetupModeRequestStore`, `GatedDigitalInput`, `ComboSetupModeTrigger`) are covered across the four tasks. The spec's full test list for `ComboSetupModeTrigger` (isHolding false initially/while one active, becomes true when both active, requested stays false on early release, requested fires once then resets, releasing only one still ends the hold, retriggerable after a full cycle) is present verbatim as six test cases in Task 4. The Non-goals section (no `main.cpp`, `Button`, `MatrixScanner`, `MatrixDigitalInput`, `ToggleTurnoutControl`, or `ToggleTurnoutStation` changes) is respected — no task touches any of those files.
- **Placeholder scan:** No TBD/TODO; every step has complete, concrete code.
- **Type consistency:** `BootMode`/`BootModeSelector::select()`'s signature, `GatedDigitalInput`'s constructor/method names, and `ComboSetupModeTrigger`'s constructor/method names are used identically in each task's own code — no task other than the one that defines each class references it, since #2c-b (not this plan) is where they're wired together.
