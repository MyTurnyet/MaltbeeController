# ESP32 Composition Root (Sub-project #7b) — Design

This is sub-project **#7b**, the remaining half of the original sub-project
#7 ("Composition root") after #7a split off `ToggleTurnoutControl` (now
complete and merged) as new domain/application logic. #7b is the actual
wiring: build `src/esp32/main.cpp` — currently an empty `setup()`/`loop()`
stub — into the real composition root for the ESP32 turnout panel firmware.

All dependencies are complete and merged to `main`: sub-projects 2a
(`NodeConfig`/`ConfigStore`/`CommissioningSession`/`SerialCommissioningAdapter`/
`EspUartPort`), 2b (`WiFiLink`/`MqttLink`/`TopicScheme`/`PayloadCodec`/
`JmriTurnoutCommandAdapter`/`JmriFeedbackSource`), 3 (`MatrixScanner`/
`MatrixDigitalInput`), 4 (`LedPairDriver`/`LedPairOutput`), 5
(`LedPairStation`), 6 (JMRI command/feedback wiring into `TurnoutControl`,
plus the topic self-echo fix), and 7a (`ToggleTurnoutControl`).

Scope note: despite the number of decisions below, this does not need
another split like #7 → #7a/#7b did. Everything here is wiring, a
config-table/constant decision, or one small composition-helper class with
no new domain *behavior* — unlike #7a, which hid a genuine logic gap.

## Decisions (confirmed via Q&A)

1. **New `ToggleTurnoutStation` composition-helper class**, not inline
   construction in `main.cpp` for all 12 stacks.
2. **`JmriTurnoutCommandAdapter` changes `retained` from `true` to `false`**
   on its command-topic publish — removes the risk (flagged in
   `docs/superpowers/specs/2026-08-29-jmri-topic-self-echo-design.md`) of a
   retained command replaying to a late subscriber or after a broker restart
   and re-actuating a solenoid without a fresh button press. A turnout
   command is a one-shot action, not state; nothing needs it replayed.
3. **Boot explicitly gates on `NodeConfig::validate()`**: Wi-Fi/MQTT
   connection attempts only start if the loaded config is valid. An
   unconfigured/factory-default node boots into "matrix scan + blinking LEDs
   + serial commissioning only" rather than attempting `WiFi.begin("", "")`
   and relying on it being harmless.
4. **`Turnout::isLocked()`/`isDisabled()` checks stay a non-goal** in
   `ToggleTurnoutControl` (unchanged from #7a) — nothing in this project's
   ESP32 design (`NodeConfig`, commissioning commands, MQTT payloads) has a
   way to lock or disable a turnout yet, so the check would have no caller
   ever exercising it. Revisit if/when such a mechanism is added.

## Architecture

```
                         ┌─────────────────────────┐
   EspUartPort ────────► │   SerialCommissioningAdapter │───► CommissioningSession ──► NvsConfigStore
   (Serial)              └─────────────────────────┘        (draft NodeConfig)      (persisted)

   runningConfig (global NodeConfig, loaded once at boot from NvsConfigStore)
        │
        ├─ channelJmriNames ──► JmriTurnoutCommandAdapter ──┐
        │                       JmriFeedbackSource ─────────┤
        │                                                    ▼
        │                                              MqttLink ──► WiFiLink
        │
   MatrixScanner (3 row ArduinoDigitalOutput, 4 col ArduinoDigitalInput)
        │
        ├─► MatrixDigitalInput ×12 ──┐
        │                             ▼
   LedPairStation ×12 ──closed()/thrown()──► ToggleTurnoutStation ×12 ──► (shared) JmriTurnoutCommandAdapter
```

### `ToggleTurnoutStation` (new, `lib/McsEsp32/src/adapters/ToggleTurnoutStation.h`/`.cpp`)

Mirrors `TurnoutStation`'s role but, unlike it (and unlike `LedPairStation`),
owns no raw GPIO — it takes already-constructed ports by reference, since on
the ESP32 side the button (`MatrixDigitalInput`, backed by a *shared*
`MatrixScanner`) and the LED outputs (`LedPairStation::closed()`/`thrown()`,
each owning its *own* GPIO) are constructed separately in `main.cpp`. Because
it only depends on ports (`DigitalInput&`, `DigitalOutput&`, `Clock&`,
`TurnoutCommandPort&`), it needs no `#ifdef ARDUINO` guard and is fully
native-testable — like `ToggleTurnoutControl` itself.

```cpp
#pragma once

#include "domain/Turnout.h"
#include "domain/Button.h"
#include "domain/Indicator.h"
#include "domain/TurnoutIndicator.h"
#include "ports/Clock.h"
#include "ports/DigitalInput.h"
#include "ports/DigitalOutput.h"
#include "ports/TurnoutCommandPort.h"
#include "../application/ToggleTurnoutControl.h"

class ToggleTurnoutStation
{
public:
    ToggleTurnoutStation(int address, const char* name, DigitalInput& button,
                          DigitalOutput& closedOutput, DigitalOutput& thrownOutput,
                          Clock& clock, TurnoutCommandPort& commandPort);

    void begin();
    void update();
    void applyFeedback(TurnoutFeedback feedback);
    void clearIndicator();

private:
    static constexpr unsigned long DEBOUNCE_MS = 30;

    Turnout turnout_;
    Button button_;
    Indicator closedIndicator_;
    Indicator thrownIndicator_;
    TurnoutIndicator turnoutIndicator_;
    ToggleTurnoutControl control_;
};
```

- Constructor stores all references, builds `turnout_(address, name,
  TurnoutPosition::Closed, false, false)` (matches `TurnoutStation`'s
  hardcoded initial-position/lock/disable convention), and wires
  `control_(button_, turnout_, turnoutIndicator_, commandPort)`.
- `begin()`: calls `turnoutIndicator_.clear()` — puts the LED pair into
  blink/unconfirmed mode. This is the *only* GPIO-adjacent responsibility
  this class has; the underlying `LedPairStation`'s own `begin()` (real GPIO
  setup) is called separately by `main.cpp`, since `ToggleTurnoutStation`
  doesn't own that station.
- `update()`: `button_.update(); control_.update();` — same shape as
  `TurnoutStation::update()` minus the second button.
- `applyFeedback(feedback)`: forwards to `control_.applyFeedback(feedback)`.
- `clearIndicator()`: forwards to `turnoutIndicator_.clear()` — used by
  `main.cpp` to revert a station to blink mode whenever MQTT is
  disconnected, per the implementation doc's "On disconnect" rule. Verified
  safe to call every `loop()` tick while disconnected (not just once on the
  transition): `LedPairDriver::applyState()` early-returns when the
  requested mode already matches `currentMode_`, so repeated calls once
  already blinking are no-ops.

## `src/esp32/main.cpp`

### Setup

1. Construct `EspUartPort`, `NvsConfigStore`; `CommissioningSession` (loads
   its own draft `NodeConfig` internally) wrapped by
   `SerialCommissioningAdapter`.
2. Load a **separate**, global `NodeConfig runningConfig = configStore.load();`
   — the "committed" config this boot's object graph binds to.
   `JmriTurnoutCommandAdapter`/`JmriFeedbackSource` hold a
   `const std::array<...>&` into `runningConfig.channelJmriNames`, so this
   must be a stable-lifetime object (global), not a temporary. Commissioning
   changes stay confined to `CommissioningSession`'s own draft and only
   affect a *future* boot, after `save` + `reboot` — matches the 2a design's
   existing draft-then-explicit-save discipline.
3. Configure every LED GPIO as output and call `ToggleTurnoutStation::begin()`
   for all 12 stations (blink/unconfirmed mode) before anything touches
   Wi-Fi — matches the implementation doc's startup sequence.
4. Configure the 7 matrix GPIOs (`begin()` on the 3 row
   `ArduinoDigitalOutput`s and 4 column `ArduinoDigitalInput`s).
5. `const bool configValid = runningConfig.validate().empty();`
6. If `configValid`: `wifiLink.begin(runningConfig.wifiSsid,
   runningConfig.wifiPassword); mqttLink.begin(runningConfig.brokerHost,
   runningConfig.brokerPort);`. If not, neither is called.

### Loop

1. `matrixScanner.update();`
2. `serialCommissioningAdapter.poll();` — if `rebootRequested()`,
   `ESP.restart();`.
3. If `configValid`: `wifiLink.poll(); mqttLink.poll();`
4. If `configValid && mqttLink.connected()`: drain
   `jmriFeedbackSource.poll(feedback)` in a `while` loop; for each result,
   call `applyFeedback(feedback)` on every station (broadcast — each
   station's `ToggleTurnoutControl`/`Turnout` self-filters by address,
   exactly the pattern `src/mega/main.cpp` already uses for LocoNet
   feedback).
5. If `!configValid || !mqttLink.connected()`: call `clearIndicator()` on
   every station (idempotent per-tick, see above) — reverts any
   previously-confirmed LED to blink mode once the connection is lost, per
   the implementation doc's "On disconnect" rule.
6. Call `update()` on every station (button debounce + toggle-command send).

### Constants (anonymous namespace in `main.cpp`)

| Constant | Value | Notes |
|---|---|---|
| `blinkIntervalMs` | `500` | Arbitrary, easily retuned; not load-bearing. |
| `defaultColor` | `LedPairColor::Red` | Shown before any station has ever displayed a real color (first boot). Red reads as "uncertain/caution" until confirmed. |
| `DEBOUNCE_MS` | `30` | Matches `TurnoutStation`'s existing value. |
| Wi-Fi/MQTT `retryIntervalMs` | `5000` | Passed to both `WiFiLink` and `MqttLink`. |
| MQTT `clientId` | `"maltbee-esp32-" + std::to_string(runningConfig.nodeId)` | Must be unique per node so the broker doesn't kick a duplicate client id. First real use of `nodeId` in this project's ESP32 code, ahead of #2d's full presence/collision design. |
| MQTT LWT `willTopic` | `"panel/" + std::to_string(runningConfig.nodeId) + "/status"` | **Placeholder** — #2d is expected to define the real presence-detection topic scheme. |
| MQTT LWT `willMessage` | `"offline"` | Placeholder, see above. |
| `JmriTurnoutCommandAdapter` publish `retained` | `false` | Changed from `true` — see Decision 2 above. One-line change to existing `.cpp`. |
| Matrix row GPIOs | `{18, 19, 21}` | From `docs/button-wiring.md`. |
| Matrix column GPIOs | `{34, 35, 36, 39}` | From `docs/button-wiring.md`. |
| Per-turnout `{address, name, matrixRow, matrixCol, ledGpio}` | T1–T12 | From `docs/button-wiring.md` (row/col) and `docs/led-wiring.md`/`docs/ESP32_Turnout_Panel_Implementation.md` (LED GPIO): `{1,"T1",0,0,4}`, `{2,"T2",0,1,13}`, `{3,"T3",0,2,14}`, `{4,"T4",0,3,16}`, `{5,"T5",1,0,17}`, `{6,"T6",1,1,22}`, `{7,"T7",1,2,23}`, `{8,"T8",1,3,25}`, `{9,"T9",2,0,26}`, `{10,"T10",2,1,27}`, `{11,"T11",2,2,32}`, `{12,"T12",2,3,33}`. |

GPIO pin assignments are fixed by the panel's own PCB design (per the 2a
spec's scope decision) — not field-configurable via commissioning, unlike
WiFi/broker/channel-name fields.

### Per-turnout config struct

```cpp
struct TurnoutPanelConfig
{
    int address;
    const char* name;
    int matrixRow;
    int matrixColumn;
    int ledGpio;
};

constexpr TurnoutPanelConfig TURNOUT_CONFIGS[12] = {
    {1, "T1", 0, 0, 4},   {2, "T2", 0, 1, 13},  {3, "T3", 0, 2, 14},  {4, "T4", 0, 3, 16},
    {5, "T5", 1, 0, 17},  {6, "T6", 1, 1, 22},  {7, "T7", 1, 2, 23},  {8, "T8", 1, 3, 25},
    {9, "T9", 2, 0, 26},  {10, "T10", 2, 1, 27}, {11, "T11", 2, 2, 32}, {12, "T12", 2, 3, 33},
};
```

`main.cpp` builds, per entry: a `LedPairStation` from `{ledGpio}`, a
`MatrixDigitalInput` from `(matrixScanner, matrixRow, matrixColumn)`, and a
`ToggleTurnoutStation` from `(address, name, that MatrixDigitalInput, that
LedPairStation's green(), that LedPairStation's red(), clock,
jmriTurnoutCommandAdapter)` — one shared `JmriTurnoutCommandAdapter` instance
across all 12, matching how `src/mega/main.cpp` shares one
`turnoutCommandPort` across its 4 `TurnoutStation`s.

### LED-to-indicator color mapping

Per `docs/led-wiring.md`: CLOSED → green → GPIO HIGH, THROWN → red → GPIO
LOW. Each `ToggleTurnoutStation`'s `closedIndicator_` wraps its
`LedPairStation::green()`, and `thrownIndicator_` wraps `red()` — consistent
with `LedPairDriver::writeColor()`'s existing `gpio_.set(color ==
LedPairColor::Green)` mapping.

## Error handling

No new error paths beyond what each dependency already handles:
`JmriTurnoutCommandAdapter::send()` already no-ops for an unnamed channel;
an invalid `NodeConfig` degrades to commissioning-only mode (Decision 3); a
dropped MQTT connection degrades every station back to blink/unconfirmed
(Section above). No exceptions anywhere in this layer, consistent with the
rest of the project.

## Testing

- **`ToggleTurnoutStation`** (new): `test/test_toggle_turnout_station/`,
  native, using `FakeDigitalInput`/`FakeDigitalOutput`/`FakeClock`/
  `FakeTurnoutCommandPort` — mirrors `ToggleTurnoutControl`'s existing test
  cases through the station's public surface:
  - `begin()` puts both outputs off (blink/unconfirmed state).
  - A debounced press on `update()` sends the opposite of the turnout's
    current position through the command port.
  - `applyFeedback()` for a matching address updates the turnout position
    and lights the matching output; for a non-matching address, changes
    nothing.
  - `clearIndicator()` turns both outputs off regardless of what was
    previously displayed (including after a real `applyFeedback()` had lit
    one of them).
- **`JmriTurnoutCommandAdapter`**: existing test updated to assert
  `retained == false` on the published message instead of `true`.
- **`src/esp32/main.cpp`**: not unit-tested — composition roots are
  build-check only (`pio run -e esp32dev`), same convention as
  `src/mega/main.cpp`. No behavior lives there that isn't already covered by
  the classes it wires together.
- Full suite (`pio test -e native`) must stay green; `pio run -e esp32dev`
  and `pio run -e megaatmega2560` must both build clean — the Mega side is
  completely untouched by this sub-project.

## Non-goals

- Wireless captive-portal commissioning (#2c) and multi-panel
  collision/identify-blink (#2d) — the `clientId`/LWT values chosen here are
  explicit placeholders #2d is expected to revisit.
- Any change to the sibling `MaltbeeTurnoutController` repo. Its
  `MqttPositionReporter` needs an additive `/state` publish for real
  feedback to ever arrive at this panel — tracked as an external dependency,
  not implemented here.
- Resolving the JMRI name-vs-id keying mismatch or the documented `/trains/`
  topic-prefix mismatch — both depend on the real broker/JMRI configuration
  outside this repo. Written up below as a pre-deployment checklist instead
  of resolved in code.
- `Turnout::isLocked()`/`isDisabled()` checks in `ToggleTurnoutControl` —
  confirmed non-goal (Decision 4).
- Hardware bring-up/verification (#8) — this sub-project is programming-only,
  same caveat `CLAUDE.md` already carries for Milestones 9-11 on the Mega
  side.

## Pre-deployment checklist (not resolved by this sub-project)

Carried forward from `docs/superpowers/specs/2026-08-29-jmri-topic-self-echo-design.md`'s
"Cross-project dependency" section — all three must be resolved against the
real layout before this panel receives real feedback, none are code changes
in this repo:

1. The sibling `MaltbeeTurnoutController`'s `MqttPositionReporter` must add a
   `/state` publish (its own release cadence).
2. Whoever commissions a physical panel must enter, via `turnout <n> name
   <value>`, whatever value actually matches the real driver's published
   topic naming (its numeric turnout id today, not this project's `LT1`-style
   placeholder names) — needs to become a documented commissioning
   instruction.
3. The real JMRI broker's documented `/trains/` topic prefix vs. this
   project's (and the sibling's) unprefixed `track/turnout/...` scheme needs
   verification against the actual broker configuration.

Until resolved, `JmriFeedbackSource` simply receives no messages and every
station's LED stays in blink/unconfirmed mode indefinitely — a safe,
already-designed-for degraded state, not a crash or incorrect confirmation.

4. **NVS initialization ordering** (found by the final whole-branch review,
   fixed in this branch): global C++ constructors run before
   `app_main()`/`initArduino()` on the Arduino-ESP32 toolchain, so any global
   that reads NVS (here, `NodeConfig runningConfig = configStore.load();`
   and `CommissioningSession`'s constructor) would silently see an
   uninitialized NVS partition and always fall back to factory-default
   config — meaning saved commissioning data would never take effect on a
   real boot. Fixed by adding an `NvsBootstrap` global, declared first in
   `src/esp32/main.cpp`, that calls `nvs_flash_init()` before any other
   global's constructor runs. This is invisible to both `pio run -e
   esp32dev` and the native suite — **explicitly verify on real hardware
   during sub-project #8's bring-up**: commission a node over bench serial
   (`wifi`/`broker`/`turnout ... name`/`save`/`reboot`), then confirm `show`
   after the reboot reports the values that were actually saved, not
   factory defaults.
5. **Blocking MQTT connect symptom** (found by the final whole-branch
   review): `MqttLink::connect()`'s underlying `PubSubClient::connect()` is
   synchronous, so an unreachable *broker* (WiFi connected, broker down or
   wrong host/port) still blocks `loop()` for the length of the connect
   timeout on every retry — including freezing the bench-serial
   commissioning console an operator would be using to fix that exact
   config. This branch mitigates the more common case (skip the MQTT
   connect attempt entirely while WiFi isn't connected yet), but a
   genuinely unreachable broker with WiFi up is still a known, unfixed
   blocking window. If sub-project #8's bring-up ever shows the commissioning
   console going unresponsive for several seconds at a time, check the
   broker host/port before suspecting matrix-scanning or serial-adapter
   bugs.
