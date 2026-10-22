# Turnout Station Wiring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire 4 real turnout stations (button + independent-output LED pair) into the 4-turnout ESP32 node board's composition root.

**Architecture:** A new `IndependentLedPairStation` (adapter) mirrors the existing `LedPairStation` exactly, but owns two real GPIOs instead of one. `src/esp32node4/main.cpp` then grows to construct 4 real buttons, 4 `IndependentLedPairStation`s, and 4 unmodified `ToggleTurnoutStation`s, following the 12-channel panel's established `TURNOUT_CONFIGS[]` + range-`for` pattern, using the existing `NullTurnoutCommandPort` as a placeholder for turnout commands.

**Tech Stack:** C++17, PlatformIO, Catch2 (native tests) — though this plan adds zero new tests (see Global Constraints).

## Global Constraints

- `pio run -e esp32node4` must build after each task.
- `pio run -e esp32dev` and `pio run -e megaatmega2560` must be unaffected.
- `pio test -e native` must stay green at 51/51 (nothing in this plan is native-testable — see each task's rationale).
- `ToggleTurnoutStation`, `LedPairStation`, `IndependentLedPairDriver`, `IndependentLedPairOutput`, `NullTurnoutCommandPort`, and `src/esp32/main.cpp` must not be modified.
- Full spec: `docs/superpowers/specs/2026-10-22-esp32-node4-turnout-station-wiring-design.md`.

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `lib/McsEsp32/src/adapters/IndependentLedPairStation.h` | Create | GPIO-owning composition helper for one turnout's independent-output LED pair |
| `lib/McsEsp32/src/adapters/IndependentLedPairStation.cpp` | Create | Implementation (`#ifdef ARDUINO`-guarded) |
| `src/esp32node4/main.cpp` | Modify | Wire 4 real buttons + LED stations + `ToggleTurnoutStation`s |

---

## Task 1: `IndependentLedPairStation`

**Files:**
- Create: `lib/McsEsp32/src/adapters/IndependentLedPairStation.h`
- Create: `lib/McsEsp32/src/adapters/IndependentLedPairStation.cpp`

**Interfaces:**
- Consumes: `ArduinoDigitalOutput(int pin, bool activeLow)` (`lib/McsCore/src/adapters/ArduinoDigitalOutput.h`); `IndependentLedPairDriver(DigitalOutput& greenGpio, DigitalOutput& redGpio, Clock& clock, unsigned long blinkIntervalMs, LedPairColor defaultColor)` and `IndependentLedPairOutput(IndependentLedPairDriver& driver, LedPairColor color)` (both from sub-project #2, unchanged).
- Produces: `IndependentLedPairConfig{int redGpioPin; int greenGpioPin;}`; `IndependentLedPairStation(const IndependentLedPairConfig& config, Clock& clock, unsigned long blinkIntervalMs, LedPairColor defaultColor)` with `begin()`, `update()`, `setIdentifying(bool)`, `green()`/`red()` returning `DigitalOutput&`, `driver()` returning `IndependentLedPairDriver&` — consumed by Task 2.

**No test file for this task.** This class has zero logic of its own — pure construction and forwarding, mirroring `LedPairStation` (`lib/McsEsp32/src/adapters/LedPairStation.h`/`.cpp`), which likewise has no native test suite. Verification is a build, not a test run — PlatformIO compiles every `.cpp` in a library it pulls in, so this new file is exercised by the `esp32node4` build even before Task 2 references it from `main.cpp`.

- [ ] **Step 1: Write `IndependentLedPairStation.h`**

Create `lib/McsEsp32/src/adapters/IndependentLedPairStation.h`:

```cpp
#pragma once

#include "adapters/ArduinoDigitalOutput.h"
#include "ports/Clock.h"
#include "ports/DigitalOutput.h"
#include "../domain/IndependentLedPairDriver.h"
#include "IndependentLedPairOutput.h"

struct IndependentLedPairConfig
{
    int redGpioPin;
    int greenGpioPin;
};

class IndependentLedPairStation
{
public:
    IndependentLedPairStation(const IndependentLedPairConfig& config, Clock& clock,
                               unsigned long blinkIntervalMs, LedPairColor defaultColor);

    void begin();
    void update();
    void setIdentifying(bool active);

    DigitalOutput& green();
    DigitalOutput& red();
    IndependentLedPairDriver& driver();

private:
    ArduinoDigitalOutput redGpio_;
    ArduinoDigitalOutput greenGpio_;
    IndependentLedPairDriver driver_;
    IndependentLedPairOutput green_;
    IndependentLedPairOutput red_;
};
```

- [ ] **Step 2: Write `IndependentLedPairStation.cpp`**

Create `lib/McsEsp32/src/adapters/IndependentLedPairStation.cpp`:

```cpp
#ifdef ARDUINO

#include "IndependentLedPairStation.h"

IndependentLedPairStation::IndependentLedPairStation(const IndependentLedPairConfig& config, Clock& clock,
                                                       const unsigned long blinkIntervalMs,
                                                       const LedPairColor defaultColor)
    : redGpio_(config.redGpioPin, false)
    , greenGpio_(config.greenGpioPin, false)
    , driver_(greenGpio_, redGpio_, clock, blinkIntervalMs, defaultColor)
    , green_(driver_, LedPairColor::Green)
    , red_(driver_, LedPairColor::Red)
{
}

void IndependentLedPairStation::begin()
{
    redGpio_.begin();
    greenGpio_.begin();
    driver_.begin();
}

void IndependentLedPairStation::update()
{
    driver_.update();
}

void IndependentLedPairStation::setIdentifying(const bool active)
{
    driver_.setIdentifying(active);
}

DigitalOutput& IndependentLedPairStation::green()
{
    return green_;
}

DigitalOutput& IndependentLedPairStation::red()
{
    return red_;
}

IndependentLedPairDriver& IndependentLedPairStation::driver()
{
    return driver_;
}

#endif
```

`ArduinoDigitalOutput(pin, false)` — `false` (not inverted) matches `LedPairStation`'s own convention and the hardware spec's wiring (§3.1: GPIO HIGH drives current through the LED to ground, so HIGH = that color lit).

- [ ] **Step 3: Build to verify it compiles**

Run: `pio run -e esp32node4`
Expected: Build succeeds (this new file is compiled as part of the `McsEsp32` library even though nothing references it yet).

- [ ] **Step 4: Build the panel and Mega environments to confirm they're unaffected**

Run: `pio run -e esp32dev`
Run: `pio run -e megaatmega2560`
Expected: Both succeed, unchanged from before this task.

- [ ] **Step 5: Run the native suite as a sanity check**

Run: `pio test -e native`
Expected: 51/51 suites pass, unchanged (this file is entirely inside `#ifdef ARDUINO`, so it compiles to nothing under `native`).

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/adapters/IndependentLedPairStation.h lib/McsEsp32/src/adapters/IndependentLedPairStation.cpp
```

Use `/arlo-commits` to compose and create the commit — do not hand-write the message.

---

## Task 2: Wire 4 real turnout stations into `src/esp32node4/main.cpp`

**Files:**
- Modify: `src/esp32node4/main.cpp`

**Interfaces:**
- Consumes: `IndependentLedPairStation` (Task 1); `ArduinoDigitalInput` (`lib/McsCore/src/adapters/ArduinoDigitalInput.h`, already used in this file); `NullTurnoutCommandPort` (`lib/McsCore/src/adapters/NullTurnoutCommandPort.h`, implements `TurnoutCommandPort`, no-op `send()`); `ToggleTurnoutStation` (`lib/McsEsp32/src/adapters/ToggleTurnoutStation.h`, unmodified — constructor `(int address, const char* name, DigitalInput& button, DigitalOutput& closedOutput, DigitalOutput& thrownOutput, Clock& clock, TurnoutCommandPort& commandPort)`, methods `begin()`, `update()`).

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
#include "adapters/IndependentLedPairStation.h"
#include "adapters/NullTurnoutCommandPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "application/CommissioningSession.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/LedPairDriver.h"
#include "domain/MacAddress.h"
#include "domain/NodeConfig.h"
#include "domain/SetupApName.h"

namespace
{
    constexpr unsigned long UART_BAUD_RATE = 115200;
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
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

namespace
{
    struct TurnoutNodeConfig
    {
        int address;
        const char* name;
        int buttonGpio;
        int redGpio;
        int greenGpio;
    };

    constexpr TurnoutNodeConfig TURNOUT_CONFIGS[4] = {
        {1, "T1", 34, 32, 33},
        {2, "T2", 35, 25, 26},
        {3, "T3", 36, 27, 13},
        {4, "T4", 39, 23, 19},
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

ArduinoDigitalInput buttons[4] = {
    ArduinoDigitalInput(TURNOUT_CONFIGS[0].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
    ArduinoDigitalInput(TURNOUT_CONFIGS[1].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
    ArduinoDigitalInput(TURNOUT_CONFIGS[2].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
    ArduinoDigitalInput(TURNOUT_CONFIGS[3].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
};

IndependentLedPairStation ledStations[4] = {
    IndependentLedPairStation({TURNOUT_CONFIGS[0].redGpio, TURNOUT_CONFIGS[0].greenGpio}, systemClock,
                              BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    IndependentLedPairStation({TURNOUT_CONFIGS[1].redGpio, TURNOUT_CONFIGS[1].greenGpio}, systemClock,
                              BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    IndependentLedPairStation({TURNOUT_CONFIGS[2].redGpio, TURNOUT_CONFIGS[2].greenGpio}, systemClock,
                              BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    IndependentLedPairStation({TURNOUT_CONFIGS[3].redGpio, TURNOUT_CONFIGS[3].greenGpio}, systemClock,
                              BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
};

NullTurnoutCommandPort turnoutCommandPort;

ToggleTurnoutStation stations[4] = {
    ToggleTurnoutStation(TURNOUT_CONFIGS[0].address, TURNOUT_CONFIGS[0].name, buttons[0],
                          ledStations[0].green(), ledStations[0].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[1].address, TURNOUT_CONFIGS[1].name, buttons[1],
                          ledStations[1].green(), ledStations[1].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[2].address, TURNOUT_CONFIGS[2].name, buttons[2],
                          ledStations[2].green(), ledStations[2].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[3].address, TURNOUT_CONFIGS[3].name, buttons[3],
                          ledStations[3].green(), ledStations[3].red(), systemClock, turnoutCommandPort),
};

void setup()
{
    uartPort.begin();

    bootButton.begin();
    setupButton.begin();

    for (auto& button : buttons)
    {
        button.begin();
    }

    for (auto& ledStation : ledStations)
    {
        ledStation.begin();
    }

    for (auto& station : stations)
    {
        station.begin();
    }

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

    for (auto& ledStation : ledStations)
    {
        ledStation.update();
    }

    for (auto& station : stations)
    {
        station.update();
    }
}
```

Three behavior-relevant additions beyond the new includes/constants/globals, all additive to sub-project #3's file: (1) `setup()` now begins every button, LED station, and station, in that order, before the boot-mode check; (2) `loop()` now updates every LED station and station, placed after the existing trigger-check block (itself unmodified) — this only runs outside `WirelessSetup`, since that branch still returns early; (3) nothing else about the file's control flow changes.

- [ ] **Step 2: Build the new environment**

Run: `pio run -e esp32node4`
Expected: Build succeeds.

- [ ] **Step 3: Build the panel and Mega environments to confirm they're unaffected**

Run: `pio run -e esp32dev`
Run: `pio run -e megaatmega2560`
Expected: Both succeed, unchanged from before this task.

- [ ] **Step 4: Run the full native suite as a final collateral-damage check**

Run: `pio test -e native`
Expected: 51/51 suites pass (this task touches no file under native test).

- [ ] **Step 5: Commit**

```bash
git add src/esp32node4/main.cpp
```

Use `/arlo-commits` to compose and create the commit.

---

## Self-Review Notes

- **Spec coverage:** the spec's `IndependentLedPairStation` component and `main.cpp` wiring sections are covered by Tasks 1-2. The spec's Non-goals (real Loco2MQTT, boot-mode/identify wiring, status/activity LEDs, commissioning-form changes, any change to `ToggleTurnoutStation`/`LedPairStation`/`IndependentLedPairDriver`/`IndependentLedPairOutput`/`NullTurnoutCommandPort`/the panel) are correctly absent from both tasks.
- **Placeholder scan:** no TBD/TODO; every step has complete, exact code.
- **Type consistency:** `IndependentLedPairStation`'s constructor signature and `green()`/`red()`/`begin()`/`update()` method names match exactly between Task 1's header and every call site in Task 2's `main.cpp`. `ToggleTurnoutStation`'s constructor argument order (`address, name, button, closedOutput, thrownOutput, clock, commandPort`) matches its existing, unmodified declaration — Task 2 passes `ledStations[i].green()` as `closedOutput` and `ledStations[i].red()` as `thrownOutput`, the same color convention the panel uses.
