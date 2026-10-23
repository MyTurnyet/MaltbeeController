# Loco2MQTT WiFi/MQTT Wiring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire real `WiFiLink`/`MqttLink`/`BrokerAddressResolver`/`EspMdnsResolver` into `src/esp32node4/main.cpp`, replacing `NullTurnoutCommandPort` with the already-templated `Loco2MqttTurnoutCommandAdapter4`/`Loco2MqttFeedbackSource4`, so a node-board button press genuinely publishes a Loco2MQTT command and a real state-topic confirmation genuinely updates the turnout and its LED.

**Architecture:** Pure composition-root edit to one file — `src/esp32node4/main.cpp` — mirroring `src/esp32/main.cpp`'s existing, working WiFi/MQTT wiring with everything presence/identify-related stripped out. No new library classes, no new ports, no new tests possible (`main.cpp` has no native test target).

**Tech Stack:** C++17 (`-std=gnu++17`), PlatformIO, ESP32 Arduino core (`WiFi.h`, `PubSubClient`, `ESPmDNS`).

## Global Constraints

- No presence/collision detection (`MqttPresenceAnnouncer`/`NodeIdentityGuard`), no identify-overlay (`IdentifyModeTimer` + the `panel/<nodeId>/identify` subscription) — both out of scope, deferred to a future sub-project.
- No new commissioning fields — `NodeConfig4` already carries everything needed (`wifiSsid`, `wifiPassword`, `brokerHost`, `brokerPort`, `channelTurnoutAddresses`).
- No change to `WiFiLink`, `MqttLink`, `BrokerAddressResolver`, `EspMdnsResolver`, `Loco2MqttTurnoutCommandAdapter`, `Loco2MqttFeedbackSource`, `PresenceTopics`, `ToggleTurnoutStation`, or `src/esp32/main.cpp` — all reused completely unmodified.
- MQTT client ID and `EspMdnsResolver`'s self-advertised hostname both use `"maltbee-node4-" + ownMac.lastFourHexDigits()` — MAC-derived, matching the panel's own convention (`"maltbee-esp32-"`/`"maltbee-panel-"`), just with a node4-specific prefix.
- The LWT (will topic/message) uses `PresenceTopics::statusTopic(runningConfig.nodeId)` / `"offline"` — required unconditionally by `MqttLink`'s constructor; stays dormant (nothing publishes `"online"`) until a future presence sub-project adds the active announcer.
- `RETRY_INTERVAL_MS = 5000` and `BROKER_RESOLVE_INTERVAL_MS = 60000` — same values the panel already uses.
- The feedback-apply gate is `configValid && mqttLink.connected()` — no collision term (unlike the panel's `&& !collision`), since the node board has no collision concept yet.
- Run `pio test -e native` and all three `pio run -e ...` build commands via the Bash tool, not PowerShell, per `CLAUDE.md` (PowerShell has produced false ERRORED/crash results on this machine).
- ACN commit notation: the `main.cpp` change has no possible unit test → `@` risk, `F` intention — the established convention for every prior composition-root-only commit in this project.

---

### Task 1: Wire WiFi/MQTT/Loco2MQTT into `src/esp32node4/main.cpp`

**Files:**
- Modify: `src/esp32node4/main.cpp`

**Interfaces:**
- Consumes: `WiFiLink` (`lib/McsEsp32/src/adapters/WiFiLink.h` — `WiFiLink(Clock&, unsigned long retryIntervalMs)`, `begin(const std::string& ssid, const std::string& password)`, `poll()`, `bool connected() const`), `MqttLink` (`lib/McsEsp32/src/adapters/MqttLink.h` — `MqttLink(Clock&, unsigned long retryIntervalMs, std::string clientId, std::string willTopic, std::string willMessage, BrokerAddressResolver&)`, `begin(const std::string& host, int port)`, `poll()`, `bool connected()`), `BrokerAddressResolver` (`lib/McsEsp32/src/application/BrokerAddressResolver.h` — `BrokerAddressResolver(MdnsResolver&, Clock&, unsigned long minResolveIntervalMs)`), `EspMdnsResolver` (`lib/McsEsp32/src/adapters/EspMdnsResolver.h` — `explicit EspMdnsResolver(std::string selfHostname)`), `PresenceTopics::statusTopic(int nodeId)` (`lib/McsEsp32/src/domain/PresenceTopics.h`), `Loco2MqttTurnoutCommandAdapter4`/`Loco2MqttFeedbackSource4` (`lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.h`/`Loco2MqttFeedbackSource.h` — constructors `(MqttTransport&, const std::array<int, 4>&)`, `send(int, TurnoutPosition)` / `bool poll(TurnoutFeedback&)`), `MacAddress::lastFourHexDigits()` (already used elsewhere in this file), `runningConfig` (already a `NodeConfig4` with `.wifiSsid`/`.wifiPassword`/`.brokerHost`/`.brokerPort`/`.nodeId`/`.channelTurnoutAddresses`, already constructed earlier in this file), `TurnoutFeedback` (`lib/McsCore/src/ports/TurnoutCommandPort.h` — `struct TurnoutFeedback { int address; TurnoutPosition position; };`), `ToggleTurnoutStation::applyFeedback(TurnoutFeedback)`/`clearIndicator()` (already used by the panel's `main.cpp`, confirmed unchanged).
- Produces: nothing downstream — this is the composition root, the final piece of this sub-project.

The current full content of `src/esp32node4/main.cpp` is:

```cpp
#include <Arduino.h>

#include <array>
#include <string>

#include <nvs_flash.h>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ButtonSetupModeTrigger.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/EitherActiveDigitalInput.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspUartPort.h"
#include "adapters/IndependentLedPairFlashIndicator.h"
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

const std::array<IndependentLedPairDriver*, 4> bootModeLedPairs = {
    &ledStations[0].driver(),
    &ledStations[1].driver(),
    &ledStations[2].driver(),
    &ledStations[3].driver(),
};
IndependentLedPairFlashIndicator<4> bootModeIndicator(bootModeLedPairs);

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

    bootModeIndicator.show(bootMode);

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
    bootModeIndicator.update();
    serialCommissioningAdapter.poll();
    if (serialCommissioningAdapter.rebootRequested())
    {
        Serial.flush();
        ESP.restart();
    }

    if (bootMode == BootMode::WirelessSetup)
    {
        captivePortalServer.poll();
        for (auto& ledStation : ledStations)
        {
            ledStation.update();
        }
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

Apply these six changes:

- [ ] **Step 1: Update the `#include` block**

Replace the full `#include` block (from `#include <Arduino.h>` through `#include "domain/SetupApName.h"`) with:

```cpp
#include <Arduino.h>

#include <array>
#include <string>

#include <nvs_flash.h>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ButtonSetupModeTrigger.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/EitherActiveDigitalInput.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspMdnsResolver.h"
#include "adapters/EspUartPort.h"
#include "adapters/IndependentLedPairFlashIndicator.h"
#include "adapters/IndependentLedPairStation.h"
#include "adapters/Loco2MqttFeedbackSource.h"
#include "adapters/Loco2MqttTurnoutCommandAdapter.h"
#include "adapters/MqttLink.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "adapters/WiFiLink.h"
#include "application/BrokerAddressResolver.h"
#include "application/CommissioningSession.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/LedPairDriver.h"
#include "domain/MacAddress.h"
#include "domain/NodeConfig.h"
#include "domain/PresenceTopics.h"
#include "domain/SetupApName.h"
```

This inserts `EspMdnsResolver.h` (between `EspDeviceIdentity.h`/`EspUartPort.h`), `Loco2MqttFeedbackSource.h`/`Loco2MqttTurnoutCommandAdapter.h` (between `IndependentLedPairStation.h`/`MqttLink.h`), `MqttLink.h`, and `WiFiLink.h` (as the last `adapters/` include) — all alphabetically ordered, matching `src/esp32/main.cpp`'s exact include ordering for these same 5 headers. It adds a new `application/` group (`BrokerAddressResolver.h` before the existing `CommissioningSession.h`) and inserts `domain/PresenceTopics.h` alphabetically into the existing `domain/` group. It removes `#include "adapters/NullTurnoutCommandPort.h"` (no longer used).

- [ ] **Step 2: Add two new constants**

Replace the first anonymous namespace block:

```cpp
namespace
{
    constexpr unsigned long UART_BAUD_RATE = 115200;
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
}
```

with:

```cpp
namespace
{
    constexpr unsigned long UART_BAUD_RATE = 115200;
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
    constexpr unsigned long RETRY_INTERVAL_MS = 5000;
    constexpr unsigned long BROKER_RESOLVE_INTERVAL_MS = 60000;
}
```

- [ ] **Step 3: Replace `NullTurnoutCommandPort` with the real WiFi/MQTT/Loco2MQTT globals**

Replace this single line:

```cpp
NullTurnoutCommandPort turnoutCommandPort;
```

with:

```cpp
WiFiLink wifiLink(systemClock, RETRY_INTERVAL_MS);

const std::string mqttClientId = "maltbee-node4-" + ownMac.lastFourHexDigits();
const std::string mqttWillTopic = PresenceTopics::statusTopic(runningConfig.nodeId);
const std::string mqttWillMessage = "offline";

EspMdnsResolver mdnsResolver("maltbee-node4-" + ownMac.lastFourHexDigits());
BrokerAddressResolver brokerAddressResolver(mdnsResolver, systemClock, BROKER_RESOLVE_INTERVAL_MS);
MqttLink mqttLink(systemClock, RETRY_INTERVAL_MS, mqttClientId, mqttWillTopic, mqttWillMessage, brokerAddressResolver);

Loco2MqttTurnoutCommandAdapter4 turnoutCommandPort(mqttLink, runningConfig.channelTurnoutAddresses);
Loco2MqttFeedbackSource4 feedbackSource(mqttLink, runningConfig.channelTurnoutAddresses);
```

`brokerAddressResolver` must be declared before `mqttLink` (which holds it by reference) — this ordering already matches. `turnoutCommandPort` keeps its exact name (now a `Loco2MqttTurnoutCommandAdapter4` instead of a `NullTurnoutCommandPort`) so the unmodified `ToggleTurnoutStation stations[4]` array immediately below, which references `turnoutCommandPort` by that name, needs no edit at all.

- [ ] **Step 4: Add the `wifiLink.begin()`/`mqttLink.begin()` block to `setup()`**

In `setup()`, insert immediately after the `WirelessSetup` block's closing brace (`}`) and before the final `uartPort.write(configValid ? ...)` line:

```cpp
    if (bootMode == BootMode::WirelessSetup)
    {
        const std::string apName = SetupApName::from(ownMac);
        captivePortalServer.begin(apName);
        uartPort.write("MaltBee node in wireless setup mode. Connect to " + apName + "\n");
        return;
    }

    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
    }

    uartPort.write(configValid ? "MaltBee node ready (configured).\n"
                                : "MaltBee node needs commissioning. Type 'show'.\n");
```

- [ ] **Step 5: Add the poll + feedback-apply-or-clearIndicator block to `loop()`**

In `loop()`, insert immediately after the `setupTrigger.requested()` block's closing brace (`}`) and before the existing `for (auto& ledStation : ledStations) { ledStation.update(); }` loop:

```cpp
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

    if (configValid)
    {
        wifiLink.poll();
        if (wifiLink.connected())
        {
            mqttLink.poll();
        }
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
```

- [ ] **Step 6: Verify all three hardware build targets**

Run (via Bash, never PowerShell — PowerShell has produced false ERRORED/crash results on this machine):

```bash
pio run -e esp32node4
```
Expected: `SUCCESS`. This is the one build target that actually compiles this file.

```bash
pio run -e esp32dev
pio run -e megaatmega2560
```
Expected: both still `SUCCESS` — this change touches only `src/esp32node4/main.cpp`, so these two should be unaffected, but confirm per this project's standing convention of verifying all build targets after any change.

- [ ] **Step 7: Verify the native test suite is unaffected**

Run:
```bash
pio test -e native
```
Expected: all 54 suites still pass — `main.cpp` is never compiled by the `native` environment (`test_build_src = false`), so this is a sanity check, not a direct test of this change.

- [ ] **Step 8: Commit**

Stage exactly this one file:
```bash
git add src/esp32node4/main.cpp
```

Commit using Arlo's Commit Notation. This is a composition-root-only change with no possible unit test — the established project convention for this exact situation:

```
@ F Wire real WiFi/MQTT/Loco2MQTT into node4 main.cpp

Replaces NullTurnoutCommandPort with Loco2MqttTurnoutCommandAdapter4/
Loco2MqttFeedbackSource4 over a real WiFiLink+MqttLink+
BrokerAddressResolver+EspMdnsResolver stack, mirroring
src/esp32/main.cpp's working reference implementation with presence/
collision detection and the identify-overlay both left out (deferred to
a future sub-project). Closes sub-project #4's "button press produces
zero observable effect" gap. Verified by pio run across all 3
environments; no native test possible for a composition root.
```

---

### Task 2: Update `CLAUDE.md` and the PCB design doc

**Files:**
- Modify: `CLAUDE.md`
- Modify: `docs/pcb/turnout-node-4ch-pcb-design.md`

**Interfaces:**
- Consumes: the finished wiring from Task 1. No code interfaces — documentation only.
- Produces: nothing downstream.

- [ ] **Step 1: Add a new section to `CLAUDE.md`**

Find the `### Loco2MQTT channel-count templating (sub-project #6)` section in `CLAUDE.md` (it should be the section immediately before `### Loco2MQTT turnout bridge (sub-project #9a)` — if a different section now sits between them, insert after whichever is last before #9a's heading). Insert a new section immediately after it:

```markdown
### Loco2MQTT WiFi/MQTT wiring (sub-project #7)

Closes the roadmap item CLAUDE.md has tracked since sub-project #4 as
"real WiFi/MQTT/Loco2MQTT wiring on the node board" — the exact piece
sub-project #6's design spec deferred, naming this sub-project by
number. `src/esp32node4/main.cpp` now wires a real `WiFiLink`/
`MqttLink`/`BrokerAddressResolver`/`EspMdnsResolver` stack, mirroring
`src/esp32/main.cpp`'s working reference implementation with
presence/collision detection and the identify-overlay both left out —
neither is part of this sub-project; both remain deferred to whichever
future sub-project adds them.

`NullTurnoutCommandPort` is gone from this file, replaced by
`Loco2MqttTurnoutCommandAdapter4`/`Loco2MqttFeedbackSource4`
(sub-project #6's templated classes) constructed over the real
`MqttLink`. The MQTT client ID and `EspMdnsResolver`'s self-advertised
hostname both use `"maltbee-node4-" + ownMac.lastFourHexDigits()` —
MAC-derived, mirroring the panel's own `"maltbee-esp32-"`/
`"maltbee-panel-"` convention with a node4-specific prefix. The LWT
(will topic/message) uses `PresenceTopics::statusTopic(runningConfig.nodeId)`/
`"offline"`, the same values the panel passes — required unconditionally
by `MqttLink`'s constructor, and dormant (nothing publishes `"online"`)
until a future presence sub-project adds the active
`MqttPresenceAnnouncer` equivalent, at which point it reuses this exact
topic rather than inventing a new one.

`setup()` gates `wifiLink.begin()`/`mqttLink.begin()` on `configValid`,
mirroring the panel exactly but without the panel's two
`mqttLink.subscribe(...)` calls (both presence/identify-only, out of
scope here). `loop()` polls both links when `configValid`, then applies
real feedback (`feedbackSource.poll()` → `station.applyFeedback()`) when
`configValid && mqttLink.connected()`, or calls `station.clearIndicator()`
otherwise — the panel's equivalent gate also ANDs `!collision`, which
has no node4 equivalent yet.

**Net effect:** a button press on the node board now genuinely publishes
a Loco2MQTT command once commissioned with real WiFi/broker credentials
and LocoNet turnout addresses, and a real state-topic confirmation now
genuinely updates the turnout and its LED — closing sub-project #4's
"Known, accepted gap: a button press currently produces zero observable
effect" for the first time.

**Test coverage added: none.** `main.cpp` is a composition root verified
by build only (`pio run -e esp32node4`/`esp32dev`/`megaatmega2560`), same
precedent as every prior `main.cpp`-only change in this project. Suite
count stays at 54.
```

- [ ] **Step 2: Annotate sub-project #6's now-"done" forward reference**

In `CLAUDE.md`'s `### Loco2MQTT channel-count templating (sub-project #6)` section, find the sentence (in its first paragraph): "Wiring these classes into `src/esp32node4/main.cpp` alongside real `WiFiLink`/`MqttLink`/`BrokerAddressResolver`/`EspMdnsResolver` is its own future sub-project (#7 in this node-board effort's numbering — not the unrelated, older '#7b' presence-detection spec referenced elsewhere in this file), as is presence/collision detection and the identify-overlay (both still blocked on #7 landing first)."

Replace it with:

```markdown
Wiring these classes into `src/esp32node4/main.cpp` alongside real
`WiFiLink`/`MqttLink`/`BrokerAddressResolver`/`EspMdnsResolver` was done
in sub-project #7 (2026-10-22, in this node-board effort's own
numbering — not the unrelated, older "#7b" presence-detection spec
referenced elsewhere in this file) — see below. Presence/collision
detection and the identify-overlay remain deferred to whichever future
sub-project adds them.
```

This fixes the exact kind of staleness the final whole-branch reviews of
both sub-project #5 and sub-project #6 already caught once each in
earlier sections — annotate now, before it recurs a third time.

- [ ] **Step 3: Update the rooted-include tally if this sub-project changed it**

This sub-project adds no new files and changes only `src/esp32node4/main.cpp`'s include list, none of which are rooted includes this sub-project introduces or removes beyond what sub-project #6 already accounted for (the two Loco2Mqtt headers use relative `../` includes, confirmed in sub-project #6). Confirm the tally is still accurate by running:

```bash
grep -rn '#include "ports/\|#include "domain/\|#include "adapters/\|#include "application/"' lib/McsEsp32/src lib/McsLoconet/src --include='*.h' --include='*.cpp' | grep -v 'EspMdnsResolver.h.*ports/MdnsResolver.h'
```

If the count differs from 44, update CLAUDE.md's "Include convention" paragraph to match — do not guess or increment blindly. If unchanged at 44, make no edit here. (Note: `src/esp32node4/main.cpp` itself is a `src/` composition root, not a `lib/` file, so its own includes were never part of this tally to begin with — this check is purely a sanity re-confirmation.)

- [ ] **Step 4: Update `docs/pcb/turnout-node-4ch-pcb-design.md`'s §7 status line**

Find the status line near the top of §7 (search for "Status as of sub-project #6"):

```markdown
**Status as of sub-project #6 (2026-10-22):** items 1 (sub-project #1), 2
(sub-project #4), 3 (sub-projects #2 and #4 together), 4's boot/setup half
(sub-project #5), and 6 (sub-project #3) are done; item 4's identify half
and item 5 remain open, both blocked on real WiFi/MQTT wiring landing on
the node board (sub-project #6 templated the Loco2MQTT classes that
wiring will use; the wiring itself is sub-project #7).
```

Replace it with:

```markdown
**Status as of sub-project #7 (2026-10-22):** items 1 (sub-project #1), 2
(sub-project #4), 3 (sub-projects #2 and #4 together), 4's boot/setup half
(sub-project #5), and 6 (sub-project #3) are done; item 4's identify half
and item 5 remain open. The real WiFi/MQTT/Loco2MQTT wiring both would
build on landed in sub-project #7 (2026-10-22) — neither identify nor
status LEDs are wired yet themselves, but the infrastructure they'll
need now exists on the node board.
```

- [ ] **Step 5: Verify the native suite is still green**

Run:
```bash
pio test -e native
```
Expected: unaffected (this task touches only documentation), all 54 suites pass.

- [ ] **Step 6: Commit**

Stage exactly these two files:
```bash
git add CLAUDE.md docs/pcb/turnout-node-4ch-pcb-design.md
```

Commit using Arlo's Commit Notation (`. d` — documentation only, zero behavior change):

```
. d Document sub-project #7 Loco2MQTT WiFi/MQTT wiring

Adds the CLAUDE.md section, annotates sub-project #6's now-resolved
forward reference to #7, and updates the PCB design doc's §7 status
line.
```

---

## Self-Review Notes

**Spec coverage:** every element of the approved design spec
(`docs/superpowers/specs/2026-10-22-esp32-node4-loco2mqtt-wiring-design.md`)
maps to a task — the exact globals block, `setup()`/`loop()` insertion
points, naming conventions (MAC-derived client ID/mDNS hostname, LWT
values), and the explicit non-goals (no presence/collision, no
identify, no new commissioning fields) are all reflected in Task 1's
before/after code. Task 2 covers documentation, following the pattern
every prior sub-project in this effort has needed — and additionally
closes a staleness gap (sub-project #6's "future sub-project (#7)"
phrasing) proactively, rather than leaving it for a third consecutive
final-review finding of the same shape.

**Placeholder scan:** no TBD/TODO; every code block is complete and
exact, reproducing the full current content of `src/esp32node4/main.cpp`
so Task 1's six edit points are unambiguous.

**Type consistency:** `Loco2MqttTurnoutCommandAdapter4`/
`Loco2MqttFeedbackSource4` (Task 1) match the exact alias names
sub-project #6 defined. `turnoutCommandPort`'s variable name is
deliberately preserved across the type change so the unmodified
`ToggleTurnoutStation stations[4]` array needs no edit — verified by
re-reading that array's construction code, which references
`turnoutCommandPort` by name only, never by type.
