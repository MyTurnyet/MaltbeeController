# ESP32 Hardware Adapters for #3/#4 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `LedPairStation`, a config-driven composition helper that owns one turnout's real ESP32 LED-pair GPIO plus its `LedPairDriver` and two `LedPairOutput`s, so sub-project #7's composition root can build 12 of them without repeating the wiring by hand — mirroring what `TurnoutStation` already does for the Mega.

**Architecture:** One new hardware-shim class in `lib/McsEsp32/src/adapters/`, `#ifdef ARDUINO`-guarded end to end like `TurnoutStation`, with no native test (impossible, not just unwanted — see Global Constraints). Verified by a temporary build-check wire-up in `src/esp32/main.cpp` that is reverted before the task ends.

**Tech Stack:** C++17, PlatformIO (`esp32dev`/`native`/`megaatmega2560` environments), Catch2 (native tests elsewhere in the repo — not used by this plan).

## Global Constraints

- Full design source of truth: `docs/superpowers/specs/2026-08-29-esp32-hardware-adapters-design.md`.
- `LedPairStation` lives in `lib/McsEsp32/src/adapters/`, not `lib/McsCore/`: this is ESP32-only composition, exactly like `MatrixScanner`/`LedPairDriver` before it.
- `LedPairStation.cpp` must be wrapped in `#ifdef ARDUINO` / `#endif` for its **entire contents**, matching `lib/McsCore/src/adapters/TurnoutStation.cpp`'s pattern exactly. `LedPairStation.h` is **not** guarded (declarations only, matching `TurnoutStation.h`).
- **No native test exists or should be attempted for this class.** `ArduinoDigitalOutput`'s method bodies (`lib/McsCore/src/adapters/ArduinoDigitalOutput.cpp`) are entirely `#ifdef ARDUINO`-guarded, so they compile to nothing under the `native` environment (no `ARDUINO` macro there). Any class that constructs an `ArduinoDigitalOutput` and calls `begin()`/`set()` on it — which `LedPairStation` must do — fails to *link* under `native`, not just "isn't tested." This is the same reason `TurnoutStation` has no native test either. Verification is a `pio run -e esp32dev` build-check only.
- `gpioPin`'s `ArduinoDigitalOutput` must be constructed with `activeLow = false` (GPIO HIGH = green, per `docs/led-wiring.md` and `LedPairDriver.h`'s own header comment "Green corresponds to the GPIO driven HIGH") — matching `TurnoutStation`'s existing LED convention (`thrownOutput_(config.thrownLedPin, false)`).
- `begin()` must call the real GPIO's `begin()` **before** the driver's `begin()` — reversing the order would let the GPIO's `begin()` silently overwrite the driver's already-written initial color (the exact bug sub-project #4's final review found and fixed).
- `blinkIntervalMs` and `defaultColor` are constructor parameters shared across every `LedPairStation` a caller builds — they are **not** fields of `LedPairConfig`. `LedPairConfig` has exactly one field: `gpioPin`.
- `green()`/`red()` return `DigitalOutput&` in color terms, never turnout-position terms (`thrown`/`closed`) — that mapping is sub-project #7's job, not this class's.
- Do not touch `MatrixScanner`, `MatrixDigitalInput`, `LedPairDriver`, `LedPairOutput`, `ArduinoDigitalInput`, or `ArduinoDigitalOutput` — all are reused completely unmodified.
- Cross-library include: `LedPairStation.h` needs `ArduinoDigitalOutput` from `McsCore`. Use a rooted include, `#include "adapters/ArduinoDigitalOutput.h"`, matching this project's existing convention for cross-library includes (same convention `LedPairDriver.h` already uses for `#include "ports/Clock.h"`). This is the first rooted include of an *adapter* header (existing rooted includes are all `domain/`/`ports/`) — safe because no other library has a file named `ArduinoDigitalOutput.h` under `adapters/` (see `CLAUDE.md`'s "Trap to watch for" section on basename uniqueness).
- `src/esp32/main.cpp` must end this task exactly as it started: `#include <Arduino.h>` followed by empty `setup()`/`loop()`. Any temporary wiring used for the build-check must be reverted before committing.
- After this task, `pio run -e esp32dev`, `pio run -e megaatmega2560`, and `pio test -e native` must all still succeed.

---

### Task 1: `LedPairStation` composition helper

**Files:**
- Create: `lib/McsEsp32/src/adapters/LedPairStation.h`
- Create: `lib/McsEsp32/src/adapters/LedPairStation.cpp`
- Modify (temporarily, then revert before committing): `src/esp32/main.cpp`
- Modify: `CLAUDE.md` (append `LedPairStation` to the `McsEsp32` adapters bullet)

**Interfaces:**
- Consumes: `ArduinoDigitalOutput(int pin, bool activeLow)` + `.begin()`/`.set(bool)`/`.isSet()` (`lib/McsCore/src/adapters/ArduinoDigitalOutput.h`); `Clock& { unsigned long nowMilliseconds() const }` (`lib/McsCore/src/ports/Clock.h`); `DigitalOutput { void set(bool); bool isSet() const; }` (`lib/McsCore/src/ports/DigitalOutput.h`); `LedPairColor{Green,Red}`, `LedPairDriver(DigitalOutput&, Clock&, unsigned long, LedPairColor)` + `.begin()`/`.update()` (`lib/McsEsp32/src/domain/LedPairDriver.h`); `LedPairOutput(LedPairDriver&, LedPairColor)` (`lib/McsEsp32/src/adapters/LedPairOutput.h`).
- Produces: `struct LedPairConfig { int gpioPin; };` and `class LedPairStation` with constructor `LedPairStation(const LedPairConfig& config, Clock& clock, unsigned long blinkIntervalMs, LedPairColor defaultColor)`, plus `void begin()`, `void update()`, `DigitalOutput& green()`, `DigitalOutput& red()`. Sub-project #7 will construct 12 of these and consume `green()`/`red()` to build `Indicator`s.

This class has no native test (see Global Constraints), so this task's steps replace the usual "write failing test → make it pass" cycle with "write the class → prove it compiles and links under `esp32dev` by temporarily wiring it into `src/esp32/main.cpp` → revert the wiring."

- [ ] **Step 1: Write `LedPairStation.h`**

```cpp
#pragma once

#include "adapters/ArduinoDigitalOutput.h"
#include "ports/Clock.h"
#include "ports/DigitalOutput.h"
#include "../domain/LedPairDriver.h"
#include "LedPairOutput.h"

struct LedPairConfig
{
    int gpioPin;
};

class LedPairStation
{
public:
    LedPairStation(const LedPairConfig& config, Clock& clock, unsigned long blinkIntervalMs,
                   LedPairColor defaultColor);

    void begin();
    void update();

    DigitalOutput& green();
    DigitalOutput& red();

private:
    ArduinoDigitalOutput gpio_;
    LedPairDriver driver_;
    LedPairOutput green_;
    LedPairOutput red_;
};
```

- [ ] **Step 2: Write `LedPairStation.cpp`**

```cpp
#ifdef ARDUINO

#include "LedPairStation.h"

LedPairStation::LedPairStation(const LedPairConfig& config, Clock& clock,
                                const unsigned long blinkIntervalMs, const LedPairColor defaultColor)
    : gpio_(config.gpioPin, false)
    , driver_(gpio_, clock, blinkIntervalMs, defaultColor)
    , green_(driver_, LedPairColor::Green)
    , red_(driver_, LedPairColor::Red)
{
}

void LedPairStation::begin()
{
    gpio_.begin();
    driver_.begin();
}

void LedPairStation::update()
{
    driver_.update();
}

DigitalOutput& LedPairStation::green()
{
    return green_;
}

DigitalOutput& LedPairStation::red()
{
    return red_;
}

#endif
```

- [ ] **Step 3: Confirm `native` and `megaatmega2560` are unaffected**

Run: `pio test -e native`
Expected: all 27 existing suites still `[PASSED]`, exit code 0. This confirms `LedPairStation.cpp`'s `#ifdef ARDUINO` guard makes it compile to an empty translation unit under `native` (no `ARDUINO` macro defined there), so it can't break linking even though nothing references the class from a test.

Run: `pio run -e megaatmega2560`
Expected: `SUCCESS`. `[env:megaatmega2560]`'s `lib_ignore = McsEsp32` (see `platformio.ini`) means this file is never even compiled for that environment — this run simply confirms no unrelated regression.

- [ ] **Step 4: Temporarily wire `LedPairStation` into `src/esp32/main.cpp` for a build-check**

Replace the full contents of `src/esp32/main.cpp` with:

```cpp
#include <Arduino.h>

#include "adapters/ArduinoClock.h"
#include "adapters/LedPairStation.h"

ArduinoClock clock;
LedPairConfig ledPairConfig{4};
LedPairStation ledPairStation(ledPairConfig, clock, 400UL, LedPairColor::Green);

void setup()
{
    ledPairStation.begin();
}

void loop()
{
    ledPairStation.update();
}
```

(GPIO 4, 400ms, and `LedPairColor::Green` are throwaway values for this build-check only — sub-project #7 chooses the real ones. `#include "adapters/ArduinoClock.h"` is a rooted include into `McsCore`, same convention as `LedPairStation.h`'s own includes.)

- [ ] **Step 5: Run the build-check**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`. This proves `LedPairStation` compiles, links, and its constructor/`begin()`/`update()` calls type-check against real `ArduinoDigitalOutput`/`ArduinoClock` instances on the actual ESP32 toolchain.

If it fails, fix `LedPairStation.h`/`.cpp` (not the throwaway wiring) and re-run this step until it passes.

- [ ] **Step 6: Revert `src/esp32/main.cpp`**

Restore it to exactly:

```cpp
#include <Arduino.h>

void setup()
{
}

void loop()
{
}
```

Run: `pio run -e esp32dev` again to confirm this reverted stub still builds (it did before this task started, and must still build after — this is not the LedPairStation build-check, just confirming the revert didn't leave anything broken).
Expected: `SUCCESS`.

- [ ] **Step 7: Update `CLAUDE.md`**

In the `### Current source layout` section, find this line (the `McsEsp32` adapters bullet):

```
  - `adapters/`: SerialCommissioningAdapter, JmriTurnoutCommandAdapter, JmriFeedbackSource (native-tested translation layers, no Arduino dependency); EspUartPort, NvsConfigStore, WiFiLink, MqttLink (`#ifdef ARDUINO`-guarded hardware shims); MatrixDigitalInput (implements `DigitalInput` for one fixed matrix cell, forwarding to `MatrixScanner`'s cached reading); LedPairOutput (implements `DigitalOutput` for one logical side, green or red, of a shared LED pair, forwarding to `LedPairDriver`)
```

Replace it with (appending `LedPairStation` and updating the shim count context):

```
  - `adapters/`: SerialCommissioningAdapter, JmriTurnoutCommandAdapter, JmriFeedbackSource (native-tested translation layers, no Arduino dependency); EspUartPort, NvsConfigStore, WiFiLink, MqttLink (`#ifdef ARDUINO`-guarded hardware shims); MatrixDigitalInput (implements `DigitalInput` for one fixed matrix cell, forwarding to `MatrixScanner`'s cached reading); LedPairOutput (implements `DigitalOutput` for one logical side, green or red, of a shared LED pair, forwarding to `LedPairDriver`); LedPairStation (`#ifdef ARDUINO`-guarded, config-driven per-turnout composition helper — owns one turnout's real LED-pair GPIO plus its `LedPairDriver`/two `LedPairOutput`s, mirroring `TurnoutStation`)
```

Also find this sentence a few lines above (in the "Trap to watch for" preamble):

```
`#ifdef ARDUINO`, on the 10 files that are genuine hardware shims (4 in
`McsCore`, 2 in `McsLoconet`, 4 in `McsEsp32`); every other file needs no
```

Replace `10 files` / `4 in \`McsEsp32\`` with `11 files` / `5 in \`McsEsp32\``:

```
`#ifdef ARDUINO`, on the 11 files that are genuine hardware shims (4 in
`McsCore`, 2 in `McsLoconet`, 5 in `McsEsp32`); every other file needs no
```

- [ ] **Step 8: Commit**

```bash
git add lib/McsEsp32/src/adapters/LedPairStation.h lib/McsEsp32/src/adapters/LedPairStation.cpp CLAUDE.md
git commit -m "^ F Add LedPairStation ESP32 LED-pair composition helper"
```

Do not `git add src/esp32/main.cpp` — it must be back to its original untouched stub content (verified in Step 6) and has no changes to commit.

---

## Self-Review Notes

- **Spec coverage:** `LedPairConfig`/`LedPairStation` (Components section) → Task 1 Steps 1-2. `begin()` ordering constraint → Global Constraints + Step 2. Shared `blinkIntervalMs`/`defaultColor` constructor args → Step 1's signature. No native test, build-check only → Steps 3-6. `CLAUDE.md` update → Step 7. "No `ButtonMatrixHardware` wrapper" non-goal → correctly has no task (nothing to build).
- **Placeholder scan:** none found — every step has literal, complete code and exact commands.
- **Type consistency:** `LedPairStation` constructor signature, `green()`/`red()` return types, and `LedPairConfig`'s single `gpioPin` field match the design doc exactly and are used identically across all steps.
