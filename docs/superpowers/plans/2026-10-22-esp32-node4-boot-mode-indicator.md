# 4-Turnout Node: Boot-Mode LED Indicator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the 4-turnout node board's independent-output LED pairs a boot-mode visual (unconfirmed blink for `NeedsCommissioning`, fast identify-flash for `WirelessSetup`, untouched for `Normal`), mirroring the 12-channel panel's existing `BootModeIndicator`/`LedPairFlashIndicator<N>` mechanism exactly.

**Architecture:** Add `IndependentLedPairFlashIndicator<N>`, a header-only template class in `lib/McsEsp32/src/adapters/` implementing the existing `BootModeIndicator` port, templated against `IndependentLedPairDriver*` instead of `LedPairDriver*`. It is a near-byte-for-byte mirror of the existing `LedPairFlashIndicator<N>` (`lib/McsEsp32/src/adapters/LedPairFlashIndicator.h`) — same two public methods, same body shape — swapping only the pointee type. Wire one instance into `src/esp32node4/main.cpp`, called from `setup()`/`loop()` in the exact same two call sites the panel's `src/esp32/main.cpp` already uses.

**Tech Stack:** C++17 (`-std=gnu++17`), PlatformIO, Catch2 (native tests), `std::array` (header already includes `<array>`/`<cstddef>` via its mirror).

## Global Constraints

- `IndependentLedPairFlashIndicator<N>` must be header-only (no `.cpp`), matching `LedPairFlashIndicator<N>`'s own header-only template shape — do not split a `.cpp` out for it.
- `show(BootMode::WirelessSetup)` calls `setIdentifying(true)` on every pair. `show(BootMode::NeedsCommissioning)` calls `setIdentifying(false)` implicitly (by calling `setGreen(false)`/`setRed(false)`, which only has visible effect once identifying is already off — but call `setIdentifying(false)` is NOT part of the mirrored class, same as the original: the original never calls `setIdentifying(false)` explicitly either, relying on `applyIdentifying(mode == BootMode::WirelessSetup)` running unconditionally on every `show()` call to turn it off for non-WirelessSetup modes). Mirror the original's exact logic, do not add anything.
- `update()` is a no-op body — `IndependentLedPairStation::update()` already drives per-tick flash/blink timing (confirmed in sub-project #4).
- Test doubles: use the existing `FakeClock`/`FakeDigitalOutput` (`test/support/`) and real `IndependentLedPairDriver` instances. Do not mock `IndependentLedPairDriver` itself.
- `main.cpp` wiring must match `src/esp32/main.cpp`'s exact two call sites: `bootModeIndicator.show(bootMode);` in `setup()` right after the last `begin()` loop and before the `BootMode::WirelessSetup` check; `bootModeIndicator.update();` as the very first line of `loop()`.
- No change to `LedPairFlashIndicator`, `BootModeIndicator`, `IndependentLedPairDriver`, `IndependentLedPairStation`, `IndependentLedPairOutput`, `ToggleTurnoutStation`, or `src/esp32/main.cpp` — all reused unmodified.
- Run `pio test -e native` via the Bash tool, not PowerShell, per `CLAUDE.md` (PowerShell has produced false ERRORED/crash results on this machine).
- ACN commit notation: the new class + its test is behavior-adding with matching tests → `F` intention, `^` risk if the diff is ≤ 8 LoC of production code (it will not be — the header is ~40 lines — so use `^` only if tests fully cover it and nothing is hand-wavy; otherwise `!`). The `main.cpp`-only wiring commit has no possible unit test → `@` risk (the established convention for composition-root-only changes in this project, per `CLAUDE.md`), `F` intention.

---

### Task 1: `IndependentLedPairFlashIndicator<N>` class + native tests

**Files:**
- Create: `lib/McsEsp32/src/adapters/IndependentLedPairFlashIndicator.h`
- Create: `test/test_independent_led_pair_flash_indicator/test_main.cpp`

**Interfaces:**
- Consumes: `BootModeIndicator` port (`lib/McsEsp32/src/ports/BootModeIndicator.h` — `virtual void show(BootMode mode) = 0; virtual void update() = 0;`), `IndependentLedPairDriver` (`lib/McsEsp32/src/domain/IndependentLedPairDriver.h` — constructor `IndependentLedPairDriver(DigitalOutput& greenGpio, DigitalOutput& redGpio, Clock& clock, unsigned long blinkIntervalMs, LedPairColor defaultColor)`; methods `setGreen(bool)`, `setRed(bool)`, `setIdentifying(bool)`, `update()`), `FakeClock`/`FakeDigitalOutput` (`test/support/`).
- Produces: `IndependentLedPairFlashIndicator<N>` — constructor `explicit IndependentLedPairFlashIndicator(const std::array<IndependentLedPairDriver*, N>& pairs)`, methods `void show(BootMode mode) override;` and `void update() override;`. Task 2 constructs one instance of this with `N = 4` and an array of `&ledStations[i].driver()`.

- [ ] **Step 1: Write the failing test file**

Create `test/test_independent_led_pair_flash_indicator/test_main.cpp` with this exact content:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/IndependentLedPairFlashIndicator.h"
#include "domain/IndependentLedPairDriver.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalOutput.h"

#include <array>

namespace
{
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
}

TEST_CASE("WirelessSetup starts the identify flash on every pair")
{
    FakeClock clock;
    FakeDigitalOutput greenGpioA;
    FakeDigitalOutput redGpioA;
    FakeDigitalOutput greenGpioB;
    FakeDigitalOutput redGpioB;
    IndependentLedPairDriver pairA(greenGpioA, redGpioA, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    IndependentLedPairDriver pairB(greenGpioB, redGpioB, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    std::array<IndependentLedPairDriver*, 2> pairs{&pairA, &pairB};
    IndependentLedPairFlashIndicator indicator(pairs);

    indicator.show(BootMode::WirelessSetup);

    REQUIRE(greenGpioA.isSet());
    REQUIRE_FALSE(redGpioA.isSet());
    REQUIRE(greenGpioB.isSet());
    REQUIRE_FALSE(redGpioB.isSet());
}

TEST_CASE("NeedsCommissioning starts the unconfirmed blink on every pair")
{
    FakeClock clock;
    FakeDigitalOutput greenGpioA;
    FakeDigitalOutput redGpioA;
    FakeDigitalOutput greenGpioB;
    FakeDigitalOutput redGpioB;
    IndependentLedPairDriver pairA(greenGpioA, redGpioA, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    IndependentLedPairDriver pairB(greenGpioB, redGpioB, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    std::array<IndependentLedPairDriver*, 2> pairs{&pairA, &pairB};
    IndependentLedPairFlashIndicator indicator(pairs);

    pairA.setGreen(true);
    pairB.setGreen(true);
    indicator.show(BootMode::NeedsCommissioning);
    clock.advanceBy(BLINK_INTERVAL_MS);
    pairA.update();
    pairB.update();

    REQUIRE_FALSE(greenGpioA.isSet());
    REQUIRE(redGpioA.isSet());
    REQUIRE_FALSE(greenGpioB.isSet());
    REQUIRE(redGpioB.isSet());
}

TEST_CASE("Normal leaves identifying off so later station ticks can drive the pairs")
{
    FakeClock clock;
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    IndependentLedPairDriver pair(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    std::array<IndependentLedPairDriver*, 1> pairs{&pair};
    IndependentLedPairFlashIndicator indicator(pairs);

    indicator.show(BootMode::WirelessSetup);
    indicator.show(BootMode::Normal);
    pair.setGreen(true);

    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());
}
```

This mirrors `test/test_led_pair_flash_indicator/test_main.cpp`'s 3 cases exactly, extended to assert both the green and red GPIO per pair (since `IndependentLedPairDriver` owns two real outputs instead of one shared pin) — the same assertion-doubling pattern `test/test_independent_led_pair_driver/test_main.cpp` already established over `test_led_pair_driver`.

The middle test's expected final state (`greenGpioA` false, `redGpioA` true) follows from `IndependentLedPairDriver`'s state machine (`lib/McsEsp32/src/domain/IndependentLedPairDriver.cpp`): `setGreen(true)` enters `Mode::Green` with `lastDisplayedColor_ = Green`; `show(NeedsCommissioning)` calls `setGreen(false)` then `setRed(false)`, both of which resolve to `Mode::Blink` with `blinkShowingLastColor_ = true` (so it immediately redisplays `lastDisplayedColor_ = Green`); advancing the clock by one interval and calling `update()` flips `blinkShowingLastColor_` to `false`, which shows the opposite color, `Red`.

- [ ] **Step 2: Run the test to verify it fails to compile**

Run (from the repo root, via Bash, not PowerShell):
```bash
pio test -e native -f test_independent_led_pair_flash_indicator
```
Expected: build failure — `adapters/IndependentLedPairFlashIndicator.h: No such file or directory` (the header doesn't exist yet).

- [ ] **Step 3: Create the header**

Create `lib/McsEsp32/src/adapters/IndependentLedPairFlashIndicator.h` with this exact content:

```cpp
#pragma once

#include "../ports/BootModeIndicator.h"
#include "../domain/IndependentLedPairDriver.h"

#include <array>
#include <cstddef>

// WirelessSetup: 150ms identify-flash on every pair (IndependentLedPairDriver's
// own setIdentifying() override).
// NeedsCommissioning: 500ms unconfirmed blink on every pair (release both
// colors so IndependentLedPairDriver's existing blink path runs). Distinct
// from WirelessSetup.
// Normal: identifying off; later station ticks own the pairs.
// update() is a no-op — IndependentLedPairStation::update() already ticks
// the flash/blink timing every loop().
//
// Near-exact mirror of LedPairFlashIndicator<N>
// (lib/McsEsp32/src/adapters/LedPairFlashIndicator.h), templated against
// IndependentLedPairDriver* instead of LedPairDriver* — see that class and
// CLAUDE.md's "Independent-output turnout indicator" section for why this
// project keeps independent-output logic in its own, separately-maintained
// copy rather than sharing it with the shared-GPIO driver's equivalents.
template <std::size_t N>
class IndependentLedPairFlashIndicator : public BootModeIndicator
{
public:
    explicit IndependentLedPairFlashIndicator(const std::array<IndependentLedPairDriver*, N>& pairs)
        : pairs_(pairs)
    {
    }

    void show(BootMode mode) override
    {
        applyIdentifying(mode == BootMode::WirelessSetup);
        if (mode == BootMode::NeedsCommissioning)
        {
            releaseAllPairs();
        }
    }

    void update() override
    {
    }

private:
    void applyIdentifying(bool identifying)
    {
        for (IndependentLedPairDriver* pair : pairs_)
        {
            pair->setIdentifying(identifying);
        }
    }

    void releaseAllPairs()
    {
        for (IndependentLedPairDriver* pair : pairs_)
        {
            pair->setGreen(false);
            pair->setRed(false);
        }
    }

    std::array<IndependentLedPairDriver*, N> pairs_;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run:
```bash
pio test -e native -f test_independent_led_pair_flash_indicator
```
Expected: `3 test cases: 3 succeeded` (8 assertions: 4 + 4 + 2... count them as reported by Catch2 — don't hardcode a guessed assertion total in the commit message, just confirm 3/3 test cases pass).

- [ ] **Step 5: Run the full native suite to confirm no regressions**

Run:
```bash
pio test -e native
```
Expected: every existing suite still passes, plus the new `test_independent_led_pair_flash_indicator` suite (52 suites total, up from 51).

- [ ] **Step 6: Commit**

Stage exactly these two files:
```bash
git add lib/McsEsp32/src/adapters/IndependentLedPairFlashIndicator.h test/test_independent_led_pair_flash_indicator/test_main.cpp
```

Commit using Arlo's Commit Notation (`^ F` — new behavior, fully covered by the 3 new test cases, consistent with the project's existing convention for this exact kind of change, e.g. sub-project #2's `IndependentLedPairDriver`/`IndependentLedPairOutput` commits):

```
^ F Add IndependentLedPairFlashIndicator boot-mode LED adapter

Mirrors LedPairFlashIndicator<N> for the node board's independent-output
LED pairs. 3 new test cases over FakeClock/FakeDigitalOutput, mirroring
test_led_pair_flash_indicator's own 3 cases with both GPIOs asserted per
pair.
```

---

### Task 2: Wire `IndependentLedPairFlashIndicator<4>` into `src/esp32node4/main.cpp`

**Files:**
- Modify: `src/esp32node4/main.cpp`

**Interfaces:**
- Consumes: `IndependentLedPairFlashIndicator<N>` from Task 1 (`#include "adapters/IndependentLedPairFlashIndicator.h"`, constructor `explicit IndependentLedPairFlashIndicator(const std::array<IndependentLedPairDriver*, N>& pairs)`, `show(BootMode)`, `update()`). `IndependentLedPairStation::driver()` (already exists, returns `IndependentLedPairDriver&`, confirmed in `lib/McsEsp32/src/adapters/IndependentLedPairStation.h`). The existing `ledStations[4]` array, `bootMode` global, and `setup()`/`loop()` bodies already in `src/esp32node4/main.cpp` (reproduced in full below for exact insertion points).
- Produces: nothing further downstream — this is the composition root, the final piece of this sub-project.

The current full content of `src/esp32node4/main.cpp` is:

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
    IndependentLedPairStation(IndependentLedPairConfig{.redGpioPin = TURNOUT_CONFIGS[0].redGpio,
                                                       .greenGpioPin = TURNOUT_CONFIGS[0].greenGpio},
                              systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    IndependentLedPairStation(IndependentLedPairConfig{.redGpioPin = TURNOUT_CONFIGS[1].redGpio,
                                                       .greenGpioPin = TURNOUT_CONFIGS[1].greenGpio},
                              systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    IndependentLedPairStation(IndependentLedPairConfig{.redGpioPin = TURNOUT_CONFIGS[2].redGpio,
                                                       .greenGpioPin = TURNOUT_CONFIGS[2].greenGpio},
                              systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    IndependentLedPairStation(IndependentLedPairConfig{.redGpioPin = TURNOUT_CONFIGS[3].redGpio,
                                                       .greenGpioPin = TURNOUT_CONFIGS[3].greenGpio},
                              systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
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

Apply these four changes:

- [ ] **Step 1: Add the `#include` for the new header**

The existing `#include` block lists `adapters/` headers alphabetically, and `"IndependentLedPairFlashIndicator.h"` sorts before `"IndependentLedPairStation.h"` (`F` < `S`). Insert the new include *immediately before* the existing one, so the block reads:

```cpp
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspUartPort.h"
#include "adapters/IndependentLedPairFlashIndicator.h"
#include "adapters/IndependentLedPairStation.h"
#include "adapters/NullTurnoutCommandPort.h"
```

- [ ] **Step 2: Add the `bootModeLedPairs` array and `bootModeIndicator` instance**

Immediately after the `ledStations[4]` array definition (right after its closing `};`) and before `NullTurnoutCommandPort turnoutCommandPort;`, insert:

```cpp
const std::array<IndependentLedPairDriver*, 4> bootModeLedPairs = {
    &ledStations[0].driver(),
    &ledStations[1].driver(),
    &ledStations[2].driver(),
    &ledStations[3].driver(),
};
IndependentLedPairFlashIndicator<4> bootModeIndicator(bootModeLedPairs);
```

This requires `#include <array>`, which `IndependentLedPairFlashIndicator.h` already pulls in transitively — but add it explicitly to `main.cpp`'s own include block too (immediately after `#include <string>`), since `main.cpp` names `std::array` directly and should not rely on an included header's own includes:

```cpp
#include <Arduino.h>

#include <array>
#include <string>
```

- [ ] **Step 3: Call `bootModeIndicator.show(bootMode)` in `setup()`**

In `setup()`, insert immediately after the `station.begin();` loop's closing brace and before the `if (bootMode == BootMode::WirelessSetup)` check:

```cpp
    for (auto& station : stations)
    {
        station.begin();
    }

    bootModeIndicator.show(bootMode);

    if (bootMode == BootMode::WirelessSetup)
```

- [ ] **Step 4: Call `bootModeIndicator.update()` as the first line of `loop()`**

Change `loop()`'s first line from:

```cpp
void loop()
{
    serialCommissioningAdapter.poll();
```

to:

```cpp
void loop()
{
    bootModeIndicator.update();
    serialCommissioningAdapter.poll();
```

- [ ] **Step 5: Verify the node4 build**

Run:
```bash
pio run -e esp32node4
```
Expected: `SUCCESS`.

- [ ] **Step 6: Verify the other three environments are unaffected**

Run:
```bash
pio run -e esp32dev
pio run -e megaatmega2560
pio test -e native
```
Expected: all three still succeed/pass exactly as before this task (no files outside `src/esp32node4/main.cpp` and Task 1's two new files were touched, so this should be a formality — but confirm it, since `main.cpp` changes have no native test to catch a mistake).

- [ ] **Step 7: Commit**

Stage exactly this one file:
```bash
git add src/esp32node4/main.cpp
```

Commit using Arlo's Commit Notation. This is a composition-root-only change with no possible unit test — per `CLAUDE.md`'s established convention for this exact situation (e.g. sub-project #3's and #4's `main.cpp`-only wiring commits), use `@ F`:

```
@ F Wire boot-mode LED indicator into node4 main.cpp

Mirrors src/esp32/main.cpp's exact bootModeIndicator.show()/update() call
sites. Verified by pio run -e esp32node4 build only, per project
convention for composition-root-only changes.
```

---

### Task 3: Update `CLAUDE.md` and the PCB design doc

**Files:**
- Modify: `CLAUDE.md`
- Modify: `docs/pcb/turnout-node-4ch-pcb-design.md`

**Interfaces:**
- Consumes: the finished `IndependentLedPairFlashIndicator<N>` class and its wiring from Tasks 1-2. No code interfaces — this is a documentation-only task.
- Produces: nothing downstream.

- [ ] **Step 1: Add a new section to `CLAUDE.md`**

Find the `### Turnout station wiring (sub-project #4)` section in `CLAUDE.md` (under "### Current source layout"). Immediately after that section's content ends and before the next `### Loco2MQTT turnout bridge (sub-project #9a)` heading, insert a new section:

```markdown
### Boot-mode LED indicator (sub-project #5)

Closes half of the roadmap item "Boot-mode, setup and identify feedback
for the new LEDs" — the half that doesn't need MQTT. Discussed with
Paige: on the panel, `BootModeIndicator`/`LedPairFlashIndicator<N>` is
driven purely by the locally-known `BootMode` value computed at boot, with
no MQTT dependency; the identify-overlay (`IdentifyModeTimer` plus an MQTT
subscription callback) is a separate mechanism the node board can't have
yet, since `Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource` still
need the `Basic<X><N>` treatment before any WiFi/MQTT wiring lands on the
node board (sub-project #1's forward-looking note, still open). **This
sub-project is the boot-mode indicator only; the identify-overlay moves to
whichever future sub-project adds real Loco2MQTT wiring to the node
board.**

One new class,
`lib/McsEsp32/src/adapters/IndependentLedPairFlashIndicator.h` — a
near-byte-for-byte mirror of the panel's own
`LedPairFlashIndicator<N>` (`lib/McsEsp32/src/adapters/LedPairFlashIndicator.h`),
header-only, templated against `IndependentLedPairDriver*` instead of
`LedPairDriver*`. Implements `BootModeIndicator`:
`show(BootMode::WirelessSetup)` calls `setIdentifying(true)` on every
pair (the same 150ms flash `IndependentLedPairDriver` already provides,
confirmed unchanged from sub-project #2); `show(BootMode::NeedsCommissioning)`
calls `setGreen(false)`/`setRed(false)` on every pair, which — against a
driver already resting in `Mode::Blink` from `begin()`/`clearIndicator()`
— is a no-op, so `NeedsCommissioning` shows the exact same red/green
alternating blink the unconfirmed state already shows (the same "keep the
existing blink visual" decision sub-project #2 made, inherited here rather
than re-decided); `show(BootMode::Normal)` leaves identifying off.
`update()` is a no-op body, since `IndependentLedPairStation::update()`
already drives the flash/blink timing every tick.

`src/esp32node4/main.cpp` wiring mirrors the panel's exact two call sites:
a `bootModeLedPairs` array of `IndependentLedPairDriver*` built from
`ledStations[i].driver()`, one `IndependentLedPairFlashIndicator<4>
bootModeIndicator(bootModeLedPairs);` global, `bootModeIndicator.show(bootMode);`
called in `setup()` right after the `station.begin()` loop and before the
`BootMode::WirelessSetup` check, and `bootModeIndicator.update();` as the
very first line of `loop()` — identical placement to `src/esp32/main.cpp`.

**Net effect:** all 4 node-board LED pairs now flash fast (150ms,
green/red alternating) for the duration of a `BootMode::WirelessSetup`
boot, instead of sitting solid red for the whole session — closing the
"Known, accepted gap" sub-project #4 documented about wireless-setup mode
giving no LED feedback. `NeedsCommissioning` and `Normal` are visually
unchanged (both already showed the unconfirmed blink via `begin()`).

**Test coverage added:** `test/test_independent_led_pair_flash_indicator`
(3 cases, mirroring `test_led_pair_flash_indicator`'s 3 cases with both
GPIOs asserted per pair, same pattern `test_independent_led_pair_driver`
already established over `test_led_pair_driver`) — 51 → 52 suites.

**Non-goals, deferred:** identify-overlay wiring (`IdentifyModeTimer`, an
MQTT `panel/<nodeId>/identify` subscription, a `setIdentifying()` loop
driven by it) — blocked on real WiFi/MQTT wiring landing on the node
board, a separate future sub-project. Status/activity LEDs and the
4-channel commissioning form remain their own future sub-projects too.
```

- [ ] **Step 2: Re-grep and correct the rooted-include tally if this sub-project changed it**

`IndependentLedPairFlashIndicator.h` uses relative includes (`../ports/BootModeIndicator.h`, `../domain/IndependentLedPairDriver.h`) — same library, so it does **not** add to the rooted-include count. Confirm this by running:

```bash
grep -rn '#include "ports/\|#include "domain/\|#include "adapters/\|#include "application/"' lib/McsEsp32/src lib/McsLoconet/src --include='*.h' --include='*.cpp' | grep -v '^lib/McsEsp32/src/.*#include "ports/MdnsResolver.h"'
```

If the count differs from 44 (39 McsEsp32 + 5 McsLoconet, the figure `CLAUDE.md` currently states), update the "Include convention" paragraph's stated counts to match — do not guess or increment blindly, per that paragraph's own warning about drift. If the count is unchanged at 44, make no edit here.

- [ ] **Step 3: Update `docs/pcb/turnout-node-4ch-pcb-design.md`'s §7 checklist**

Find this line (near the end of the file, in the implementation-status checklist):

```markdown
- [ ] **Firmware work** (§7) items 4-5 — identify/setup-mode flashing, status LED ownership.
```

Replace it with:

```markdown
- [x] **Firmware work** (§7) **item 4, boot/setup-mode half** — `IndependentLedPairFlashIndicator` mirrors `LedPairFlashIndicator` for the node board's independent outputs, sub-project #5 (2026-10-22). The identify-overlay half of item 4, and item 5 (status/activity LEDs), remain open — identify is blocked on real WiFi/MQTT wiring landing on the node board (own future sub-project).
```

- [ ] **Step 4: Verify the native suite is still green**

Run:
```bash
pio test -e native
```
Expected: unaffected (this task touches only documentation), all 52 suites pass.

- [ ] **Step 5: Commit**

Stage exactly these two files:
```bash
git add CLAUDE.md docs/pcb/turnout-node-4ch-pcb-design.md
```

Commit using Arlo's Commit Notation (`. d` — documentation only, zero behavior change, mechanically consistent with the already-landed code):

```
. d Document sub-project #5 boot-mode LED indicator

Adds the CLAUDE.md section and marks the boot/setup half of PCB design
doc §7 item 4 done; identify-overlay half stays open pending real
WiFi/MQTT wiring on the node board.
```

---

## Self-Review Notes

**Spec coverage:** every element of the approved design spec
(`docs/superpowers/specs/2026-10-22-esp32-node4-boot-mode-indicator-design.md`)
maps to a task — the `IndependentLedPairFlashIndicator<N>` class (Task 1),
the exact `main.cpp` wiring at the exact two call sites (Task 2), the 3
mirrored test cases (Task 1), and documentation (Task 3, going beyond the
spec's own silence on docs because `CLAUDE.md` has required a matching
update after every prior sub-project in this effort).

**Placeholder scan:** no TBD/TODO; every code block is complete and
exact, including the full current-state reproduction of `main.cpp` needed
to make Task 2's insertion points unambiguous without requiring the
implementer to guess at surrounding context.

**Type consistency:** `IndependentLedPairFlashIndicator<N>` (Task 1) is
constructed in Task 2 as `IndependentLedPairFlashIndicator<4>
bootModeIndicator(bootModeLedPairs);` with `bootModeLedPairs` typed
`std::array<IndependentLedPairDriver*, 4>` — matching the constructor
signature `explicit IndependentLedPairFlashIndicator(const
std::array<IndependentLedPairDriver*, N>& pairs)` Task 1 defines. `show`/
`update` signatures match the `BootModeIndicator` port exactly in both
tasks.
