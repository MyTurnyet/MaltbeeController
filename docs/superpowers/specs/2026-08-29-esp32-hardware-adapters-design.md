# ESP32 Hardware Adapters for #3/#4 (Sub-project #5) — Design

This is sub-project **#5** in the ESP32 panel decomposition (see
`docs/superpowers/specs/2026-08-28-esp32-node-config-commissioning-design.md`'s
sub-project table): real-GPIO adapters for sub-project #3 (`MatrixScanner`)
and sub-project #4 (`LedPairDriver`). It depends on both #3 and #4 (already
complete) and feeds into #7 (composition root).

## Problem

`MatrixScanner` and `LedPairDriver` are pure domain classes that consume the
existing `DigitalInput`/`DigitalOutput` ports (`lib/McsCore/src/ports/`).
Something has to construct real ESP32 GPIO objects and hand them in.

The existing `ArduinoDigitalInput`/`ArduinoDigitalOutput` adapters
(`lib/McsCore/src/adapters/`) already do that job unmodified: they're
generic `#ifdef ARDUINO` GPIO wrappers, not AVR-specific, and
`[env:esp32dev]`'s `lib_deps` already build-checks them (confirmed by
reading both `.cpp` files — `ArduinoDigitalInput::begin()` calls
`pinMode(pin_, useInternalPullup_ ? INPUT_PULLUP : INPUT)`, which is exactly
correct for `docs/button-wiring.md`'s input-only column pins 34/35/36/39
with `useInternalPullup = false`). So there is no ESP32-specific gap on the
raw-adapter side.

The one real gap is boilerplate: each of the 12 turnouts needs one real
GPIO + a `LedPairDriver` + two `LedPairOutput`s, constructed identically —
exactly the repetition `TurnoutStation`
(`lib/McsCore/src/adapters/TurnoutStation.h`) already exists to eliminate on
the Mega side (4 turnouts × a 6-field `TurnoutConfig`, one `TurnoutStation`
per row of `src/mega/main.cpp`'s `STATION_CONFIGS` table).

The button-matrix side has no equivalent repetition: it's a single shared
7-GPIO object graph (3 rows + 4 columns) built once for the whole panel, not
per turnout. There's nothing to eliminate by wrapping it, so it stays direct
construction in sub-project #7 — matching what #3's own spec already
scoped ("Wiring up all 12 `MatrixDigitalInput`s with their correct
coordinates and handing them to 12 `Button`s is composition-root work
(sub-project #7)").

## Decision

One new class, `LedPairStation`, in `lib/McsEsp32/src/adapters/` — a 5th
hardware shim alongside this project's existing four
(`ArduinoClock`, `ArduinoDigitalInput`, `ArduinoDigitalOutput`,
`TurnoutStation`, all in `McsCore`). Like `TurnoutStation`, it is
`#ifdef ARDUINO`-guarded end to end (not just its `.cpp` method bodies — the
whole file, matching `TurnoutStation.cpp`'s pattern) and verified by
`pio run -e esp32dev` build-check only, **no native test**. This isn't a
new choice — it's forced by the same constraint `TurnoutStation` already
has: `ArduinoDigitalOutput`'s method bodies compile to nothing under
`native` (its `.cpp` is entirely `#ifdef ARDUINO`), so any class that
constructs one and calls `begin()`/`set()` on it fails to link under
`native` regardless of test intent.

```cpp
struct LedPairConfig
{
    int gpioPin;
};

class LedPairStation
{
public:
    LedPairStation(const LedPairConfig& config, Clock& clock,
                    unsigned long blinkIntervalMs, LedPairColor defaultColor);

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

- `gpio_(config.gpioPin, false)` — `activeLow = false`. `led-wiring.md` and
  `LedPairDriver`'s own header comment ("Green corresponds to the GPIO
  driven HIGH") both fix GPIO HIGH = green, matching `TurnoutStation`'s
  existing LED construction convention (`thrownOutput_(config.thrownLedPin,
  false)`).
- `begin()` calls `gpio_.begin()` then `driver_.begin()`, in that exact
  order. This is not a fresh design choice — it's the convention
  sub-project #4's final review established after finding that constructing
  `LedPairDriver` before the real GPIO's `begin()` runs would let `begin()`
  silently clobber the driver's initial color write. `LedPairStation` is
  the first place that ordering actually gets exercised against a real
  `ArduinoDigitalOutput`.
- `update()` calls `driver_.update()`. Sub-project #7 calls this once per
  loop tick per station, alongside `MatrixScanner::update()` and each
  `TurnoutControl::update()`.
- `green()`/`red()` return `DigitalOutput&`, staying in color terms, not
  turnout-position terms — matching sub-project #4's design decision that
  `LedPairDriver`/`LedPairOutput` have no opinion about what they represent.
  Sub-project #7 decides the color↔position mapping when it constructs each
  turnout's two `Indicator`s (`Indicator(station.green())`,
  `Indicator(station.red())`, assigned to whichever of
  `TurnoutIndicator`'s thrown/closed slots the panel's convention calls for).
- `blinkIntervalMs` and `defaultColor` are shared constructor arguments, not
  per-station fields in `LedPairConfig` — one blink interval and one default
  color for the entire panel, matching how `src/mega/main.cpp`'s single
  `LOCONET_PULSE_DURATION_MS` constant is shared by all 4 `TurnoutStation`s
  rather than living per-row in `TurnoutConfig`. There's no known reason a
  panel would want different blink timing or default color per turnout, and
  `LedPairConfig` only needs to grow if that changes.

### Why no `ButtonMatrixHardware`-style wrapper

Considered and rejected: bundling the matrix's 3 row `ArduinoDigitalOutput`s
+ 4 column `ArduinoDigitalInput`s + `MatrixScanner` into one class, for
symmetry with `LedPairStation`. Rejected because that object graph is built
exactly once per panel, not once per turnout — there's no repeated
construction for a wrapper to DRY up, unlike the 12× repetition
`LedPairStation` and `TurnoutStation` both exist to eliminate. Sub-project
#7 constructs the 7 GPIO objects and the one `MatrixScanner` directly, the
same way `src/mega/main.cpp` already constructs its single
`MrrwaLocoNetSwitchDriver`/`PulsingLocoNetTransport`/`MrrwaLocoNetTurnoutAdapter`
chain directly rather than through a wrapper — both are "build it once,
wire it in" hardware, not per-turnout hardware.

## Components

### `LedPairConfig` (`lib/McsEsp32/src/adapters/LedPairStation.h`)

```cpp
struct LedPairConfig
{
    int gpioPin;
};
```

One field. Deliberately not more: GPIO pin assignments are fixed by this
panel's PCB design (per sub-project #2a's design doc — "GPIO pin
assignments are not field-configurable"), so this struct only needs to
carry the one thing that legitimately varies per turnout.

### `LedPairStation` (`lib/McsEsp32/src/adapters/LedPairStation.h` / `.cpp`)

Shown in full under Decision, above. Constructor initializer order:
`gpio_(config.gpioPin, false)`, `driver_(gpio_, clock, blinkIntervalMs,
defaultColor)`, `green_(driver_, LedPairColor::Green)`, `red_(driver_,
LedPairColor::Red)` — matching `TurnoutStation`'s existing pattern of
constructing owned adapters first, then the domain objects that reference
them by `&`/`&`, all in one member-initializer list.

## Testing

No native test — see Decision above for why one isn't possible, not just
unwanted. Verification is `pio run -e esp32dev` build-check: temporarily
construct one `LedPairStation` in `src/esp32/main.cpp`'s stub, call
`begin()` from `setup()` and `update()` from `loop()`, confirm the build
succeeds, then revert the stub back to empty — mirroring exactly how
sub-project #2a build-checked `NvsConfigStore`/`EspUartPort` before revert,
and leaving `src/esp32/main.cpp` untouched at the end of this sub-project
(the permanent wiring is #7's job).

## Non-goals

- The button-matrix's shared 7-GPIO object graph — direct construction in
  sub-project #7 (see "Why no `ButtonMatrixHardware`-style wrapper" above).
- The row/column-to-turnout mapping and the green/red-to-closed/thrown
  mapping — both sub-project #7's composition-root decisions, per #3's and
  #4's specs.
- Choosing the real `blinkIntervalMs` millisecond value or the real
  `defaultColor` — both passed into `LedPairStation`'s constructor by
  sub-project #7, which picks the actual values.
- Any per-turnout `Button`, `TurnoutIndicator`, or `TurnoutControl` wiring,
  or JMRI command/feedback port construction — all sub-project #7.
- Any change to `MatrixScanner`, `MatrixDigitalInput`, `LedPairDriver`,
  `LedPairOutput`, `ArduinoDigitalInput`, or `ArduinoDigitalOutput` — all
  reused completely unmodified.
