# ESP32 Composition Root (Sub-project #7b) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `src/esp32/main.cpp` — currently an empty `setup()`/`loop()` stub — into the real composition root for the ESP32 turnout panel firmware, wiring together bench-serial commissioning, matrix button scanning, shared-GPIO LED pairs, and JMRI/MQTT turnout command/feedback for all 12 turnouts.

**Architecture:** One new native-testable composition-helper class (`ToggleTurnoutStation`) bundles one turnout's `Button`+`Turnout`+`TurnoutIndicator`+`ToggleTurnoutControl` behind a small surface (`begin()`/`update()`/`applyFeedback()`/`clearIndicator()`), taking already-constructed ports by reference. `src/esp32/main.cpp` builds 12 of these plus the shared matrix/LED/commissioning/MQTT infrastructure as global objects, wiring pure composition — no new business logic lives in `main.cpp` itself.

**Tech Stack:** C++17, PlatformIO (`native` for tests, `esp32dev` for the real build), Catch2 for native tests.

## Global Constraints

- No `Arduino.h` dependency in domain/application/port code. `ToggleTurnoutStation` depends only on `lib/McsCore` ports/domain classes and `lib/McsEsp32`'s `ToggleTurnoutControl` — all already Arduino-independent — so it must compile under `native` with no guard.
- The `#ifdef ARDUINO` convention on existing hardware shims (`LedPairStation`, `WiFiLink`, `MqttLink`, `NvsConfigStore`, `EspUartPort`, `ArduinoDigitalInput`, `ArduinoDigitalOutput`, `ArduinoClock`) is untouched by this plan — none of their guards change.
- `ToggleTurnoutStation` must be fully native-testable via existing test doubles (`FakeDigitalInput`, `FakeDigitalOutput`, `FakeClock`, `FakeTurnoutCommandPort` in `test/support/`) — no new test doubles needed.
- Exact constant values (from the approved spec):
  - `blinkIntervalMs = 500`, `defaultColor = LedPairColor::Red`
  - `DEBOUNCE_MS = 30` (matches `TurnoutStation`'s existing value)
  - Wi-Fi/MQTT `retryIntervalMs = 5000`
  - MQTT `clientId = "maltbee-esp32-" + std::to_string(runningConfig.nodeId)`
  - MQTT LWT `willTopic = "panel/" + std::to_string(runningConfig.nodeId) + "/status"`, `willMessage = "offline"` (explicit placeholders, pending sub-project #2d)
  - `JmriTurnoutCommandAdapter`'s publish `retained` changes from `true` to `false`
- `src/esp32/main.cpp` is a composition root only — no business logic, matching `CLAUDE.md`'s rule and `src/mega/main.cpp`'s existing precedent. It is build-check only (`pio run -e esp32dev`), not native-tested, same convention as `src/mega/main.cpp`.
- `pio test -e native` must stay green (28/28 suites plus the new one), and `pio run -e megaatmega2560` must build unchanged — this plan touches nothing under `lib/McsLoconet` or `src/mega/`.

---

### Task 1: `ToggleTurnoutStation` composition-helper class

**Files:**
- Create: `lib/McsEsp32/src/adapters/ToggleTurnoutStation.h`
- Create: `lib/McsEsp32/src/adapters/ToggleTurnoutStation.cpp`
- Test: `test/test_toggle_turnout_station/test_main.cpp`

**Interfaces:**
- Consumes:
  - `Turnout(int address, const char* name, TurnoutPosition, bool locked, bool disabled)` — `lib/McsCore/src/domain/Turnout.h`
  - `Button(DigitalInput& input, Clock& clock, unsigned long debounceMilliseconds)` — `lib/McsCore/src/domain/Button.h`; has `void update()`
  - `Indicator(DigitalOutput& output)` — `lib/McsCore/src/domain/Indicator.h`
  - `TurnoutIndicator(Indicator& thrownIndicator, Indicator& closedIndicator)` — `lib/McsCore/src/domain/TurnoutIndicator.h`; has `void display(TurnoutPosition)` and `void clear()`
  - `ToggleTurnoutControl(Button&, Turnout&, TurnoutIndicator&, TurnoutCommandPort&)` — `lib/McsEsp32/src/application/ToggleTurnoutControl.h`; has `void update()` and `void applyFeedback(TurnoutFeedback)`
  - `struct TurnoutFeedback { int address; TurnoutPosition position; };` — `lib/McsCore/src/ports/TurnoutCommandPort.h`
- Produces (consumed by Task 3):
  ```cpp
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
  };
  ```

- [ ] **Step 1: Write the failing test file**

Create `test/test_toggle_turnout_station/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/ToggleTurnoutStation.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeDigitalOutput.h"
#include "support/FakeTurnoutCommandPort.h"

namespace
{
    constexpr unsigned long DEBOUNCE_MS = 30;
}

TEST_CASE("begin() clears the indicator even if an output was previously left on")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    FakeDigitalOutput closedOutput;
    FakeDigitalOutput thrownOutput;
    FakeTurnoutCommandPort commandPort;

    thrownOutput.set(true);

    ToggleTurnoutStation station(101, "Test Turnout", buttonInput, closedOutput, thrownOutput, clock, commandPort);
    station.begin();

    REQUIRE_FALSE(closedOutput.isSet());
    REQUIRE_FALSE(thrownOutput.isSet());
}

TEST_CASE("update() sends the opposite of the turnout's constructed-default position on a debounced press")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    FakeDigitalOutput closedOutput;
    FakeDigitalOutput thrownOutput;
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutStation station(101, "Test Turnout", buttonInput, closedOutput, thrownOutput, clock, commandPort);
    station.begin();

    buttonInput.active = true;
    station.update();
    clock.advanceBy(DEBOUNCE_MS);
    station.update();

    REQUIRE(commandPort.sentCommands.size() == 1);
    REQUIRE(commandPort.sentCommands[0].address == 101);
    REQUIRE(commandPort.sentCommands[0].position == TurnoutPosition::Thrown);
}

TEST_CASE("applyFeedback for a matching address updates the turnout and lights the matching output")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    FakeDigitalOutput closedOutput;
    FakeDigitalOutput thrownOutput;
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutStation station(101, "Test Turnout", buttonInput, closedOutput, thrownOutput, clock, commandPort);
    station.begin();

    station.applyFeedback({101, TurnoutPosition::Thrown});

    REQUIRE(thrownOutput.isSet());
    REQUIRE_FALSE(closedOutput.isSet());
}

TEST_CASE("applyFeedback for a non-matching address leaves a previously-lit output unchanged")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    FakeDigitalOutput closedOutput;
    FakeDigitalOutput thrownOutput;
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutStation station(101, "Test Turnout", buttonInput, closedOutput, thrownOutput, clock, commandPort);
    station.begin();
    station.applyFeedback({101, TurnoutPosition::Thrown});

    station.applyFeedback({202, TurnoutPosition::Closed});

    REQUIRE(thrownOutput.isSet());
    REQUIRE_FALSE(closedOutput.isSet());
}

TEST_CASE("clearIndicator turns both outputs off even after a real applyFeedback lit one")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    FakeDigitalOutput closedOutput;
    FakeDigitalOutput thrownOutput;
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutStation station(101, "Test Turnout", buttonInput, closedOutput, thrownOutput, clock, commandPort);
    station.begin();
    station.applyFeedback({101, TurnoutPosition::Thrown});
    REQUIRE(thrownOutput.isSet());

    station.clearIndicator();

    REQUIRE_FALSE(closedOutput.isSet());
    REQUIRE_FALSE(thrownOutput.isSet());
}
```

- [ ] **Step 2: Run the test to verify it fails to compile**

Run: `pio test -e native -f test_toggle_turnout_station`
Expected: FAIL — `ToggleTurnoutStation.h` does not exist yet.

- [ ] **Step 3: Write the header**

Create `lib/McsEsp32/src/adapters/ToggleTurnoutStation.h`:

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

- [ ] **Step 4: Write the implementation**

Create `lib/McsEsp32/src/adapters/ToggleTurnoutStation.cpp`:

```cpp
#include "ToggleTurnoutStation.h"

ToggleTurnoutStation::ToggleTurnoutStation(const int address, const char* name, DigitalInput& button,
                                            DigitalOutput& closedOutput, DigitalOutput& thrownOutput,
                                            Clock& clock, TurnoutCommandPort& commandPort)
    : turnout_(address, name, TurnoutPosition::Closed, false, false)
    , button_(button, clock, DEBOUNCE_MS)
    , closedIndicator_(closedOutput)
    , thrownIndicator_(thrownOutput)
    , turnoutIndicator_(thrownIndicator_, closedIndicator_)
    , control_(button_, turnout_, turnoutIndicator_, commandPort)
{
}

void ToggleTurnoutStation::begin()
{
    turnoutIndicator_.clear();
}

void ToggleTurnoutStation::update()
{
    button_.update();
    control_.update();
}

void ToggleTurnoutStation::applyFeedback(const TurnoutFeedback feedback)
{
    control_.applyFeedback(feedback);
}

void ToggleTurnoutStation::clearIndicator()
{
    turnoutIndicator_.clear();
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_toggle_turnout_station`
Expected: PASS — 5 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites pass (29/29 including the new one).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/adapters/ToggleTurnoutStation.h lib/McsEsp32/src/adapters/ToggleTurnoutStation.cpp test/test_toggle_turnout_station/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add ToggleTurnoutStation composition helper
EOF
)"
```

---

### Task 2: Change `JmriTurnoutCommandAdapter`'s command publish to non-retained

**Files:**
- Modify: `lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.cpp:25`
- Modify: `test/test_jmri_turnout_command_adapter/test_main.cpp:28`

**Interfaces:**
- Consumes: nothing new — this is a one-line change to an existing, already-tested class.
- Produces: no interface change — `JmriTurnoutCommandAdapter`'s public surface (`send(int, TurnoutPosition)`) is unchanged; only its `MqttTransport::publish()` call's third argument changes.

- [ ] **Step 1: Update the existing test assertion to expect `retained == false`**

In `test/test_jmri_turnout_command_adapter/test_main.cpp`, change line 28 from:

```cpp
    REQUIRE(transport.published[0].retained);
```

to:

```cpp
    REQUIRE_FALSE(transport.published[0].retained);
```

- [ ] **Step 2: Run the test to verify it now fails against the unchanged production code**

Run: `pio test -e native -f test_jmri_turnout_command_adapter`
Expected: FAIL — `"send publishes the expected topic, payload, and retained flag"` fails because production code still publishes with `retained = true`.

- [ ] **Step 3: Change the production code**

In `lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.cpp`, change line 25 from:

```cpp
    transport_.publish(TopicScheme::topicFor(jmriName), PayloadCodec::encode(position), true);
```

to:

```cpp
    transport_.publish(TopicScheme::topicFor(jmriName), PayloadCodec::encode(position), false);
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_jmri_turnout_command_adapter`
Expected: PASS — all 5 test cases in this file pass.

- [ ] **Step 5: Run the JMRI wiring integration test and full native suite to check for regressions**

Run: `pio test -e native -f test_jmri_turnout_wiring`
Expected: PASS — this test only asserts topic and payload, not the retained flag, so it is unaffected.

Run: `pio test -e native`
Expected: All suites pass.

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.cpp test/test_jmri_turnout_command_adapter/test_main.cpp
git commit -m "$(cat <<'EOF'
^ B Stop retaining JMRI turnout commands on the command topic
EOF
)"
```

---

### Task 3: `src/esp32/main.cpp` composition root

**Files:**
- Modify: `src/esp32/main.cpp` (currently an empty `#include <Arduino.h>` / `setup(){}` / `loop(){}` stub)

**Interfaces:**
- Consumes (all pre-existing except `ToggleTurnoutStation` from Task 1 and the fixed `JmriTurnoutCommandAdapter` from Task 2):
  - `ArduinoClock` (`lib/McsCore/src/adapters/ArduinoClock.h`) — `unsigned long nowMilliseconds() const`
  - `ArduinoDigitalOutput(int pin, bool activeLow)` (`lib/McsCore/src/adapters/ArduinoDigitalOutput.h`) — `void begin()`, implements `DigitalOutput`
  - `ArduinoDigitalInput(int pin, bool activeLow, bool useInternalPullup)` (`lib/McsCore/src/adapters/ArduinoDigitalInput.h`) — `void begin()`, implements `DigitalInput`
  - `MatrixScanner(std::array<DigitalOutput*,3>, std::array<DigitalInput*,4>)` (`lib/McsEsp32/src/domain/MatrixScanner.h`) — `void update()`; `kRowCount = 3`, `kColumnCount = 4`
  - `MatrixDigitalInput(MatrixScanner&, int row, int col)` (`lib/McsEsp32/src/adapters/MatrixDigitalInput.h`) — implements `DigitalInput`
  - `struct LedPairConfig { int gpioPin; };` and `LedPairStation(const LedPairConfig&, Clock&, unsigned long blinkIntervalMs, LedPairColor defaultColor)` (`lib/McsEsp32/src/adapters/LedPairStation.h`) — `void begin()`, `void update()`, `DigitalOutput& green()`, `DigitalOutput& red()`
  - `enum class LedPairColor { Green, Red };` (`lib/McsEsp32/src/domain/LedPairDriver.h`)
  - `ToggleTurnoutStation` from Task 1 (`lib/McsEsp32/src/adapters/ToggleTurnoutStation.h`)
  - `WiFiLink(Clock&, unsigned long retryIntervalMs)` (`lib/McsEsp32/src/adapters/WiFiLink.h`) — `void begin(const std::string& ssid, const std::string& password)`, `void poll()`, `bool connected() const`
  - `MqttLink(Clock&, unsigned long retryIntervalMs, std::string clientId, std::string willTopic, std::string willMessage)` (`lib/McsEsp32/src/adapters/MqttLink.h`) — implements `MqttTransport`; `void begin(const std::string& host, int port)`, `void poll()`, `bool connected()`
  - `JmriTurnoutCommandAdapter(MqttTransport&, const std::array<std::string,12>& channelJmriNames)` (`lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.h`) — implements `TurnoutCommandPort`
  - `JmriFeedbackSource(MqttTransport&, const std::array<std::string,12>& channelJmriNames)` (`lib/McsEsp32/src/adapters/JmriFeedbackSource.h`) — `bool poll(TurnoutFeedback& outFeedback)`
  - `NodeConfig` (`lib/McsEsp32/src/domain/NodeConfig.h`) — `nodeId`, `wifiSsid`, `wifiPassword`, `brokerHost`, `brokerPort`, `channelJmriNames` (`std::array<std::string, kChannelCount>`, `kChannelCount = 12`), `validate()` returns `std::vector<std::string>` (empty = valid)
  - `NvsConfigStore` (`lib/McsEsp32/src/adapters/NvsConfigStore.h`) — `NodeConfig load()`, `bool save(const NodeConfig&)`
  - `CommissioningSession(ConfigStore&)` (`lib/McsEsp32/src/application/CommissioningSession.h`) — `std::string apply(const ParsedCommand&)`, `bool rebootRequested() const`; loads its own draft internally at construction
  - `SerialCommissioningAdapter(UartPort&, CommissioningSession&)` (`lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h`) — `void poll()` (internally parses lines via `CommandLineParser` and dispatches to the session), `bool rebootRequested() const`
  - `EspUartPort(unsigned long baudRate)` (`lib/McsEsp32/src/adapters/EspUartPort.h`) — `void begin()`, implements `UartPort`
  - `struct TurnoutFeedback { int address; TurnoutPosition position; };` (`lib/McsCore/src/ports/TurnoutCommandPort.h`)
- Produces: nothing (this is the leaf composition root; no other task depends on it).

- [ ] **Step 1: Write the complete composition root**

Replace the entire contents of `src/esp32/main.cpp` with:

```cpp
#include <Arduino.h>

#include <string>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ArduinoDigitalOutput.h"
#include "adapters/EspUartPort.h"
#include "adapters/JmriFeedbackSource.h"
#include "adapters/JmriTurnoutCommandAdapter.h"
#include "adapters/LedPairStation.h"
#include "adapters/MatrixDigitalInput.h"
#include "adapters/MqttLink.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WiFiLink.h"
#include "application/CommissioningSession.h"
#include "domain/LedPairDriver.h"
#include "domain/MatrixScanner.h"
#include "domain/NodeConfig.h"
#include "ports/TurnoutCommandPort.h"

namespace
{
    struct TurnoutPanelConfig
    {
        int address;
        const char* name;
        int matrixRow;
        int matrixColumn;
        int ledGpio;
    };

    constexpr TurnoutPanelConfig TURNOUT_CONFIGS[12] = {
        {1, "T1", 0, 0, 4},    {2, "T2", 0, 1, 13},   {3, "T3", 0, 2, 14},   {4, "T4", 0, 3, 16},
        {5, "T5", 1, 0, 17},   {6, "T6", 1, 1, 22},   {7, "T7", 1, 2, 23},   {8, "T8", 1, 3, 25},
        {9, "T9", 2, 0, 26},   {10, "T10", 2, 1, 27}, {11, "T11", 2, 2, 32}, {12, "T12", 2, 3, 33},
    };

    constexpr int MATRIX_ROW_PINS[MatrixScanner::kRowCount] = {18, 19, 21};
    constexpr int MATRIX_COLUMN_PINS[MatrixScanner::kColumnCount] = {34, 35, 36, 39};

    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
    constexpr unsigned long RETRY_INTERVAL_MS = 5000;
    constexpr unsigned long UART_BAUD_RATE = 115200;
}

ArduinoClock clock;

EspUartPort uartPort(UART_BAUD_RATE);
NvsConfigStore configStore;
CommissioningSession commissioningSession(configStore);
SerialCommissioningAdapter serialCommissioningAdapter(uartPort, commissioningSession);

NodeConfig runningConfig = configStore.load();
const bool configValid = runningConfig.validate().empty();

ArduinoDigitalOutput matrixRow0(MATRIX_ROW_PINS[0], true);
ArduinoDigitalOutput matrixRow1(MATRIX_ROW_PINS[1], true);
ArduinoDigitalOutput matrixRow2(MATRIX_ROW_PINS[2], true);

ArduinoDigitalInput matrixCol0(MATRIX_COLUMN_PINS[0], true, false);
ArduinoDigitalInput matrixCol1(MATRIX_COLUMN_PINS[1], true, false);
ArduinoDigitalInput matrixCol2(MATRIX_COLUMN_PINS[2], true, false);
ArduinoDigitalInput matrixCol3(MATRIX_COLUMN_PINS[3], true, false);

MatrixScanner matrixScanner({&matrixRow0, &matrixRow1, &matrixRow2},
                            {&matrixCol0, &matrixCol1, &matrixCol2, &matrixCol3});

LedPairStation ledStations[12] = {
    LedPairStation({TURNOUT_CONFIGS[0].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[1].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[2].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[3].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[4].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[5].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[6].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[7].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[8].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[9].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[10].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[11].ledGpio}, clock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
};

MatrixDigitalInput matrixButtons[12] = {
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[0].matrixRow, TURNOUT_CONFIGS[0].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[1].matrixRow, TURNOUT_CONFIGS[1].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[2].matrixRow, TURNOUT_CONFIGS[2].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[3].matrixRow, TURNOUT_CONFIGS[3].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[4].matrixRow, TURNOUT_CONFIGS[4].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[5].matrixRow, TURNOUT_CONFIGS[5].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[6].matrixRow, TURNOUT_CONFIGS[6].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[7].matrixRow, TURNOUT_CONFIGS[7].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[8].matrixRow, TURNOUT_CONFIGS[8].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[9].matrixRow, TURNOUT_CONFIGS[9].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[10].matrixRow, TURNOUT_CONFIGS[10].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[11].matrixRow, TURNOUT_CONFIGS[11].matrixColumn),
};

WiFiLink wifiLink(clock, RETRY_INTERVAL_MS);

const std::string mqttClientId = "maltbee-esp32-" + std::to_string(runningConfig.nodeId);
const std::string mqttWillTopic = "panel/" + std::to_string(runningConfig.nodeId) + "/status";
const std::string mqttWillMessage = "offline";

MqttLink mqttLink(clock, RETRY_INTERVAL_MS, mqttClientId, mqttWillTopic, mqttWillMessage);

JmriTurnoutCommandAdapter turnoutCommandPort(mqttLink, runningConfig.channelJmriNames);
JmriFeedbackSource feedbackSource(mqttLink, runningConfig.channelJmriNames);

ToggleTurnoutStation stations[12] = {
    ToggleTurnoutStation(TURNOUT_CONFIGS[0].address, TURNOUT_CONFIGS[0].name, matrixButtons[0],
                          ledStations[0].green(), ledStations[0].red(), clock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[1].address, TURNOUT_CONFIGS[1].name, matrixButtons[1],
                          ledStations[1].green(), ledStations[1].red(), clock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[2].address, TURNOUT_CONFIGS[2].name, matrixButtons[2],
                          ledStations[2].green(), ledStations[2].red(), clock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[3].address, TURNOUT_CONFIGS[3].name, matrixButtons[3],
                          ledStations[3].green(), ledStations[3].red(), clock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[4].address, TURNOUT_CONFIGS[4].name, matrixButtons[4],
                          ledStations[4].green(), ledStations[4].red(), clock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[5].address, TURNOUT_CONFIGS[5].name, matrixButtons[5],
                          ledStations[5].green(), ledStations[5].red(), clock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[6].address, TURNOUT_CONFIGS[6].name, matrixButtons[6],
                          ledStations[6].green(), ledStations[6].red(), clock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[7].address, TURNOUT_CONFIGS[7].name, matrixButtons[7],
                          ledStations[7].green(), ledStations[7].red(), clock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[8].address, TURNOUT_CONFIGS[8].name, matrixButtons[8],
                          ledStations[8].green(), ledStations[8].red(), clock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[9].address, TURNOUT_CONFIGS[9].name, matrixButtons[9],
                          ledStations[9].green(), ledStations[9].red(), clock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[10].address, TURNOUT_CONFIGS[10].name, matrixButtons[10],
                          ledStations[10].green(), ledStations[10].red(), clock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[11].address, TURNOUT_CONFIGS[11].name, matrixButtons[11],
                          ledStations[11].green(), ledStations[11].red(), clock, turnoutCommandPort),
};

void setup()
{
    uartPort.begin();

    matrixRow0.begin();
    matrixRow1.begin();
    matrixRow2.begin();
    matrixCol0.begin();
    matrixCol1.begin();
    matrixCol2.begin();
    matrixCol3.begin();

    for (auto& ledStation : ledStations)
    {
        ledStation.begin();
    }

    for (auto& station : stations)
    {
        station.begin();
    }

    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
    }
}

void loop()
{
    matrixScanner.update();

    serialCommissioningAdapter.poll();
    if (serialCommissioningAdapter.rebootRequested())
    {
        ESP.restart();
    }

    if (configValid)
    {
        wifiLink.poll();
        mqttLink.poll();
    }

    if (configValid && mqttLink.connected())
    {
        TurnoutFeedback feedback{};
        while (feedbackSource.poll(feedback))
        {
            for (auto& station : stations)
            {
                station.applyFeedback(feedback);
            }
        }
    }
    else
    {
        for (auto& station : stations)
        {
            station.clearIndicator();
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

- [ ] **Step 2: Build-check the ESP32 target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — no compile errors. If there are pin/type mismatches, fix them by re-checking the exact header signatures listed in "Interfaces" above (do not guess).

- [ ] **Step 3: Verify the Mega target and native suite are unaffected**

Run: `pio run -e megaatmega2560`
Expected: `SUCCESS`, unchanged from before this task (this task touches nothing under `src/mega/` or `lib/McsLoconet`).

Run: `pio test -e native`
Expected: All suites still pass — this task adds no native tests of its own (composition roots are build-check only), so the count should match Task 2's result exactly.

- [ ] **Step 4: Commit**

```bash
git add src/esp32/main.cpp
git commit -m "$(cat <<'EOF'
! F Wire ESP32 composition root: matrix, LEDs, JMRI/MQTT, commissioning
EOF
)"
```

(Risk symbol `!`, not `^`, because this file has no automated test of its own beyond a build-check — matches the risk level `src/mega/main.cpp`'s own composition-root commits would warrant under Arlo's Commit Notation.)

---

## Self-review notes

- **Spec coverage:** All four confirmed decisions (Task 1's new class, Task 2's `retained` flag, the boot-gating logic in Task 3's `setup()`/`loop()`, and the `isLocked()`/`isDisabled()` non-goal — simply not added anywhere) are covered. The full constants table, the `TurnoutPanelConfig` struct/array, the LED color mapping (`closedOutput` → `green()`, `thrownOutput` → `red()`), and the disconnect-reverts-to-blink behavior are all present in Task 3. The pre-deployment checklist and non-goals from the spec are documentation-only and require no task.
- **Placeholder scan:** No TBD/TODO; every step has complete, concrete code with exact values (no "add appropriate handling"-style steps).
- **Type consistency:** `ToggleTurnoutStation`'s constructor signature and method names in Task 1's "Produces" block match exactly what Task 3 calls. `TurnoutFeedback`, `TurnoutPosition`, `LedPairColor`, and `NodeConfig::channelJmriNames`'s type (`std::array<std::string, 12>`) are used identically across all three tasks.
