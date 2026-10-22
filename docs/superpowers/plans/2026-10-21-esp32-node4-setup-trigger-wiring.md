# Composite Setup Trigger Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `BootMode::WirelessSetup` actually reachable on the 4-turnout node board by wiring a composite "either button" input into `ButtonSetupModeTrigger`.

**Architecture:** A new `EitherActiveDigitalInput` adapter composes exactly two `DigitalInput&`s (OR semantics), fully native-testable. `src/esp32node4/main.cpp` gets a `Clock`, two real GPIO buttons (the kit's BOOT button and a new dedicated SETUP button on GPIO16), and the panel's existing trigger-check logic, verbatim.

**Tech Stack:** C++17, Catch2 (native tests), the project's existing `test/support/FakeDigitalInput.h` test double.

## Global Constraints

- `pio test -e native` must pass all pre-existing suites unchanged, plus the one new suite this plan adds.
- `ButtonSetupModeTrigger`, `ArduinoDigitalInput`, `ArduinoClock`, `NvsSetupModeRequestStore`, and `src/esp32/main.cpp` (the 12-channel panel) must not be modified.
- `pio run -e esp32node4` must build; `pio run -e esp32dev` and `pio run -e megaatmega2560` must be unaffected.
- Full spec: `docs/superpowers/specs/2026-10-21-esp32-node4-setup-trigger-wiring-design.md`.

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `lib/McsEsp32/src/adapters/EitherActiveDigitalInput.h` | Create | Composite `DigitalInput`, active when either of two inputs is |
| `lib/McsEsp32/src/adapters/EitherActiveDigitalInput.cpp` | Create | Implementation |
| `test/test_either_active_digital_input/test_main.cpp` | Create | 4 cases covering all active/inactive combinations |
| `src/esp32node4/main.cpp` | Modify | Add `Clock`, two real GPIO buttons, and the setup-trigger wiring |

---

## Task 1: `EitherActiveDigitalInput`

**Files:**
- Create: `lib/McsEsp32/src/adapters/EitherActiveDigitalInput.h`
- Create: `lib/McsEsp32/src/adapters/EitherActiveDigitalInput.cpp`
- Test: `test/test_either_active_digital_input/test_main.cpp`

**Interfaces:**
- Consumes: `DigitalInput` port (`lib/McsCore/src/ports/DigitalInput.h`).
- Produces: `EitherActiveDigitalInput(DigitalInput& first, DigitalInput& second)` implementing `DigitalInput::isActive() const` — consumed by Task 2's `main.cpp` wiring.

- [ ] **Step 1: Write the failing tests**

Create `test/test_either_active_digital_input/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/EitherActiveDigitalInput.h"
#include "support/FakeDigitalInput.h"

TEST_CASE("both inputs inactive is inactive")
{
    FakeDigitalInput first;
    FakeDigitalInput second;
    EitherActiveDigitalInput composite(first, second);

    REQUIRE_FALSE(composite.isActive());
}

TEST_CASE("first input active, second inactive is active")
{
    FakeDigitalInput first;
    FakeDigitalInput second;
    first.active = true;
    EitherActiveDigitalInput composite(first, second);

    REQUIRE(composite.isActive());
}

TEST_CASE("first input inactive, second active is active")
{
    FakeDigitalInput first;
    FakeDigitalInput second;
    second.active = true;
    EitherActiveDigitalInput composite(first, second);

    REQUIRE(composite.isActive());
}

TEST_CASE("both inputs active is active")
{
    FakeDigitalInput first;
    FakeDigitalInput second;
    first.active = true;
    second.active = true;
    EitherActiveDigitalInput composite(first, second);

    REQUIRE(composite.isActive());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_either_active_digital_input`
Expected: FAIL to compile — `EitherActiveDigitalInput` is not declared.

- [ ] **Step 3: Write `EitherActiveDigitalInput.h`**

Create `lib/McsEsp32/src/adapters/EitherActiveDigitalInput.h`:

```cpp
#pragma once

#include "ports/DigitalInput.h"

// Active when either of the two composed inputs is active. Exactly two
// inputs, not a variadic list — the one need this exists for ("either
// the BOOT or the dedicated SETUP button") is for precisely two.
class EitherActiveDigitalInput final : public DigitalInput
{
public:
    EitherActiveDigitalInput(DigitalInput& first, DigitalInput& second);

    [[nodiscard]] bool isActive() const override;

private:
    DigitalInput& first_;
    DigitalInput& second_;
};
```

- [ ] **Step 4: Write `EitherActiveDigitalInput.cpp`**

Create `lib/McsEsp32/src/adapters/EitherActiveDigitalInput.cpp`:

```cpp
#include "EitherActiveDigitalInput.h"

EitherActiveDigitalInput::EitherActiveDigitalInput(DigitalInput& first, DigitalInput& second)
    : first_(first), second_(second)
{
}

bool EitherActiveDigitalInput::isActive() const
{
    return first_.isActive() || second_.isActive();
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_either_active_digital_input`
Expected: PASS — all 4 cases.

- [ ] **Step 6: Run the full native suite as a collateral-damage check**

Run: `pio test -e native`
Expected: all pre-existing suites pass unchanged, plus this new one (51 suites total: 50 from before this plan + `test_either_active_digital_input`).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/adapters/EitherActiveDigitalInput.h lib/McsEsp32/src/adapters/EitherActiveDigitalInput.cpp
git add test/test_either_active_digital_input/test_main.cpp
```

Use `/arlo-commits` to compose and create the commit — do not hand-write the message.

---

## Task 2: Wire the trigger into `src/esp32node4/main.cpp`

**Files:**
- Modify: `src/esp32node4/main.cpp`

**Interfaces:**
- Consumes: `EitherActiveDigitalInput` (Task 1); `ArduinoClock` (`lib/McsCore/src/adapters/ArduinoClock.h`, implements `Clock`); `ArduinoDigitalInput` (`lib/McsCore/src/adapters/ArduinoDigitalInput.h`, already used by the panel); `ButtonSetupModeTrigger` (`lib/McsEsp32/src/adapters/ButtonSetupModeTrigger.h`, unmodified — constructor `(DigitalInput& button, Clock& clock, unsigned long minHoldMs)`, methods `update()`, `requested() const`); `NvsSetupModeRequestStore::requestOnNextBoot()` (already constructed in this file).

- [ ] **Step 1: Replace the full contents of `src/esp32node4/main.cpp`**

Replace the full contents of `src/esp32node4/main.cpp` with:

```cpp
#include <Arduino.h>

#include <string>

#include <nvs_flash.h>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ButtonSetupModeTrigger.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/EitherActiveDigitalInput.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "application/CommissioningSession.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/MacAddress.h"
#include "domain/NodeConfig.h"
#include "domain/SetupApName.h"

namespace
{
    constexpr unsigned long UART_BAUD_RATE = 115200;
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
}

namespace
{
    struct NvsBootstrap
    {
        NvsBootstrap()
        {
            // ESP-IDF runs global constructors before app_main(), so Arduino's
            // initArduino() has not yet called nvs_flash_init() when the globals
            // below read NVS. Initialize it here (idempotent - initArduino()'s
            // later call returns ESP_OK) so this boot's config is the one
            // commissioning actually saved.
            esp_err_t err = nvs_flash_init();
            if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                nvs_flash_erase();
                nvs_flash_init();
            }
        }
    };
}

NvsBootstrap nvsBootstrap;

ArduinoClock systemClock;

const MacAddress ownMac = EspDeviceIdentity().mac();

EspUartPort uartPort(UART_BAUD_RATE);
NvsConfigStore4 configStore;
CommissioningSession4 commissioningSession(configStore);
SerialCommissioningAdapter4 serialCommissioningAdapter(uartPort, commissioningSession);
WebFormCommissioningAdapter4 webFormAdapter(commissioningSession);
CaptivePortalServer4 captivePortalServer(webFormAdapter);

NvsSetupModeRequestStore setupModeRequestStore;
NodeConfig4 runningConfig = configStore.load();
const bool wirelessSetupRequested = setupModeRequestStore.consumeRequest();
const BootMode bootMode = BootModeSelector::select(runningConfig, wirelessSetupRequested);
const bool configValid = runningConfig.validate().empty();

ArduinoDigitalInput bootButton(0, /*activeLow=*/true, /*useInternalPullup=*/true);
ArduinoDigitalInput setupButton(16, /*activeLow=*/true, /*useInternalPullup=*/true);
EitherActiveDigitalInput setupTriggerInput(bootButton, setupButton);
ButtonSetupModeTrigger setupTrigger(setupTriggerInput, systemClock, SETUP_TRIGGER_HOLD_MS);

void setup()
{
    uartPort.begin();

    bootButton.begin();
    setupButton.begin();

    if (bootMode == BootMode::WirelessSetup)
    {
        const std::string apName = SetupApName::from(ownMac);
        captivePortalServer.begin(apName);
        uartPort.write("MaltBee node in wireless setup mode. Connect to " + apName + "\n");
        return;
    }

    uartPort.write(configValid ? "MaltBee node ready (configured).\n"
                                : "MaltBee node needs commissioning. Type 'show'.\n");
}

void loop()
{
    serialCommissioningAdapter.poll();
    if (serialCommissioningAdapter.rebootRequested())
    {
        Serial.flush();
        ESP.restart();
    }

    if (bootMode == BootMode::WirelessSetup)
    {
        captivePortalServer.poll();
        return;
    }

    setupTrigger.update();

    if (setupTrigger.requested())
    {
        const bool stored = setupModeRequestStore.requestOnNextBoot();
        if (stored)
        {
            uartPort.write("Entering wireless setup...\n");
            Serial.flush();
            ESP.restart();
        }
        else
        {
            uartPort.write("Failed to persist wireless setup request; staying in normal mode.\n");
        }
    }
}
```

Three behavior-relevant changes from the current file, beyond the new includes/globals: (1) `setup()` now calls `bootButton.begin()`/`setupButton.begin()` unconditionally before the `WirelessSetup` check, matching the panel's "GPIO init happens regardless of boot mode" pattern; (2) the existing `WirelessSetup` branch in `loop()` gains a `return;` after `captivePortalServer.poll();`, so the trigger-check below never runs while already in wireless setup mode; (3) the panel's exact trigger-check block is appended after that `if` block.

- [ ] **Step 2: Build the new environment**

Run: `pio run -e esp32node4`
Expected: Build succeeds.

- [ ] **Step 3: Build the panel and Mega environments to confirm they're unaffected**

Run: `pio run -e esp32dev`
Run: `pio run -e megaatmega2560`
Expected: Both succeed, unchanged from before this task (this task touches no file either of them compiles).

- [ ] **Step 4: Run the full native suite as a final collateral-damage check**

Run: `pio test -e native`
Expected: all 51 suites pass (this task touches no file under native test, but it's the plan's final task, so confirm nothing regressed).

- [ ] **Step 5: Commit**

```bash
git add src/esp32node4/main.cpp
```

Use `/arlo-commits` to compose and create the commit.

---

## Self-Review Notes

- **Spec coverage:** the spec's "Decision" (the `EitherActiveDigitalInput` class), "main.cpp wiring" (every constructor call, the `setup()`/`loop()` changes, the added `return;`), and "Testing" sections are all covered by Tasks 1-2. The spec's "Non-goals" (4 direct buttons, turnout/LED wiring, any change to `ButtonSetupModeTrigger`/`ArduinoDigitalInput`/`ArduinoClock`/`NvsSetupModeRequestStore`/the panel) are correctly absent from both tasks.
- **Placeholder scan:** no TBD/TODO; every step has complete, exact code.
- **Type consistency:** `EitherActiveDigitalInput`'s constructor signature in Task 1's header matches its single call site in Task 2's `main.cpp` exactly.
