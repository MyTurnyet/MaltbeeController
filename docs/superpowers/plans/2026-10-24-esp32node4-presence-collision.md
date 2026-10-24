# Presence/Collision Detection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire `NodeIdentityGuard`/`MqttPresenceAnnouncer`/`GatedDigitalInput` into `src/esp32node4/main.cpp`, mirroring the panel's (`src/esp32/main.cpp`) existing presence/collision detection exactly, so two node4 boards accidentally commissioned with the same `nodeId` can detect each other and suppress their buttons.

**Architecture:** Pure composition-root edit to one file. No new library code — `NodeIdentityGuard`, `MqttPresenceAnnouncer`, `GatedDigitalInput`, and `PresenceTopics` are all already built, already tested, and already proven on the panel; this plan only wires them into the node board the same way.

**Tech Stack:** C++17 (`-std=gnu++17`), PlatformIO, ESP32 Arduino core.

## Global Constraints

- No change to `NodeIdentityGuard`, `MqttPresenceAnnouncer`, `GatedDigitalInput`, `PresenceTopics`, or `src/esp32/main.cpp` — all reused completely unmodified.
- No identify-overlay (`IdentifyModeTimer` + the `panel/<nodeId>/identify` subscription) — out of scope, a separate future sub-project.
- The collision-log message text uses "node" instead of the panel's "panel" wording: `"NodeId collision detected: this node is " + ownMac.lastFourHexDigits() + ", another node claiming this node id is " + identityGuard.observedMac() + "\n"`.
- The feedback-apply gate changes from `configValid && mqttLink.connected()` to `configValid && mqttLink.connected() && !collision`, matching the panel exactly.
- Run `pio test -e native` and all three `pio run -e ...` build commands via the Bash tool, not PowerShell, per `CLAUDE.md` (PowerShell has produced false ERRORED/crash results on this machine; also note running `pio test -e native` while another instance is still finishing elsewhere can produce spurious ERRORED results on a handful of suites — a resource-contention artifact, not a real failure — if seen, confirm nothing else is running `pio` and re-run once in isolation).
- ACN commit notation: the `main.cpp` change has no possible unit test → `@` risk, `F` intention — the established convention for every prior composition-root-only commit in this project.

---

### Task 1: Wire presence/collision detection into `src/esp32node4/main.cpp`

**Files:**
- Modify: `src/esp32node4/main.cpp`

**Interfaces:**
- Consumes: `GatedDigitalInput` (`lib/McsEsp32/src/adapters/GatedDigitalInput.h` — `explicit GatedDigitalInput(DigitalInput& inner)`, `void setSuppressed(bool)`, `bool isActive() const override`), `NodeIdentityGuard` (`lib/McsEsp32/src/domain/NodeIdentityGuard.h` — `explicit NodeIdentityGuard(std::string ownMac)`, `void onMacObserved(const std::string&)`, `bool collisionDetected() const`, `const std::string& observedMac() const`), `MqttPresenceAnnouncer` (`lib/McsEsp32/src/application/MqttPresenceAnnouncer.h` — `MqttPresenceAnnouncer(MqttTransport&, Clock&, int nodeId, std::string ownMac)`, `void update(bool currentlyConnected)`), `PresenceTopics::macTopic(int nodeId)` (`lib/McsEsp32/src/domain/PresenceTopics.h`, already included in this file). All already-existing globals (`buttons[4]`, `mqttLink`, `systemClock`, `ownMac`, `runningConfig`, `uartPort`) unchanged.
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

namespace
{
    constexpr unsigned long UART_BAUD_RATE = 115200;
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
    constexpr unsigned long RETRY_INTERVAL_MS = 5000;
    constexpr unsigned long BROKER_RESOLVE_INTERVAL_MS = 60000;
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

WiFiLink wifiLink(systemClock, RETRY_INTERVAL_MS);

const std::string mqttClientId = "maltbee-node4-" + ownMac.lastFourHexDigits();
const std::string mqttWillTopic = PresenceTopics::statusTopic(runningConfig.nodeId);
const std::string mqttWillMessage = "offline";

EspMdnsResolver mdnsResolver("maltbee-node4-" + ownMac.lastFourHexDigits());
BrokerAddressResolver brokerAddressResolver(mdnsResolver, systemClock, BROKER_RESOLVE_INTERVAL_MS);
MqttLink mqttLink(systemClock, RETRY_INTERVAL_MS, mqttClientId, mqttWillTopic, mqttWillMessage, brokerAddressResolver);

Loco2MqttTurnoutCommandAdapter4 turnoutCommandPort(mqttLink, runningConfig.channelTurnoutAddresses);
Loco2MqttFeedbackSource4 feedbackSource(mqttLink, runningConfig.channelTurnoutAddresses);

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

    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
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

    for (auto& station : stations)
    {
        station.update();
    }
}
```

Apply these seven changes:

- [ ] **Step 1: Add the 3 new `#include` lines**

Insert `#include "adapters/GatedDigitalInput.h"` alphabetically — between `#include "adapters/EspUartPort.h"` and `#include "adapters/IndependentLedPairFlashIndicator.h"`. Insert `#include "application/MqttPresenceAnnouncer.h"` alphabetically — after `#include "application/CommissioningSession.h"` (last in the `application/` group). Insert `#include "domain/NodeIdentityGuard.h"` alphabetically — between `#include "domain/NodeConfig.h"` and `#include "domain/PresenceTopics.h"`. The resulting full include block:

```cpp
#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ButtonSetupModeTrigger.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/EitherActiveDigitalInput.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspMdnsResolver.h"
#include "adapters/EspUartPort.h"
#include "adapters/GatedDigitalInput.h"
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
#include "application/MqttPresenceAnnouncer.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/LedPairDriver.h"
#include "domain/MacAddress.h"
#include "domain/NodeConfig.h"
#include "domain/NodeIdentityGuard.h"
#include "domain/PresenceTopics.h"
#include "domain/SetupApName.h"
```

This matches `src/esp32/main.cpp`'s exact relative ordering for these same 3 headers.

- [ ] **Step 2: Add `gatedButtons[4]`**

Insert immediately after the `buttons[4]` array's closing `};`:

```cpp
ArduinoDigitalInput buttons[4] = {
    ArduinoDigitalInput(TURNOUT_CONFIGS[0].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
    ArduinoDigitalInput(TURNOUT_CONFIGS[1].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
    ArduinoDigitalInput(TURNOUT_CONFIGS[2].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
    ArduinoDigitalInput(TURNOUT_CONFIGS[3].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
};

GatedDigitalInput gatedButtons[4] = {
    GatedDigitalInput(buttons[0]),
    GatedDigitalInput(buttons[1]),
    GatedDigitalInput(buttons[2]),
    GatedDigitalInput(buttons[3]),
};
```

- [ ] **Step 3: Add `identityGuard`/`presenceAnnouncer`**

Insert immediately after the `mqttLink` declaration and before `Loco2MqttTurnoutCommandAdapter4 turnoutCommandPort`:

```cpp
MqttLink mqttLink(systemClock, RETRY_INTERVAL_MS, mqttClientId, mqttWillTopic, mqttWillMessage, brokerAddressResolver);

NodeIdentityGuard identityGuard(ownMac.lastFourHexDigits());
MqttPresenceAnnouncer presenceAnnouncer(mqttLink, systemClock, runningConfig.nodeId, ownMac.lastFourHexDigits());

Loco2MqttTurnoutCommandAdapter4 turnoutCommandPort(mqttLink, runningConfig.channelTurnoutAddresses);
```

- [ ] **Step 4: Change `stations[4]` to use `gatedButtons[i]`**

Replace the `ToggleTurnoutStation stations[4]` array with:

```cpp
ToggleTurnoutStation stations[4] = {
    ToggleTurnoutStation(TURNOUT_CONFIGS[0].address, TURNOUT_CONFIGS[0].name, gatedButtons[0],
                          ledStations[0].green(), ledStations[0].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[1].address, TURNOUT_CONFIGS[1].name, gatedButtons[1],
                          ledStations[1].green(), ledStations[1].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[2].address, TURNOUT_CONFIGS[2].name, gatedButtons[2],
                          ledStations[2].green(), ledStations[2].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[3].address, TURNOUT_CONFIGS[3].name, gatedButtons[3],
                          ledStations[3].green(), ledStations[3].red(), systemClock, turnoutCommandPort),
};
```

(only `buttons[i]` → `gatedButtons[i]` changes on each line — everything else is identical.)

- [ ] **Step 5: Subscribe to the mac topic in `setup()`**

Change:

```cpp
    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
    }
```

to:

```cpp
    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
        mqttLink.subscribe(PresenceTopics::macTopic(runningConfig.nodeId),
                            [](const std::string& payload) { identityGuard.onMacObserved(payload); });
    }
```

- [ ] **Step 6: Add the collision block, suppress gated buttons, and announce presence in `loop()`**

Change:

```cpp
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
```

to:

```cpp
    setupTrigger.update();

    const bool collision = identityGuard.collisionDetected();
    static bool collisionLogged = false;
    if (collision && !collisionLogged)
    {
        uartPort.write("NodeId collision detected: this node is " + ownMac.lastFourHexDigits() +
                        ", another node claiming this node id is " + identityGuard.observedMac() + "\n");
        collisionLogged = true;
    }

    for (auto& gated : gatedButtons)
    {
        gated.setSuppressed(collision);
    }

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

    presenceAnnouncer.update(mqttLink.connected());

    if (configValid && mqttLink.connected() && !collision)
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
```

This matches the panel's exact placement: the collision block runs right after `setupTrigger.update()` and before the `setupTrigger.requested()` check; `presenceAnnouncer.update(...)` runs right after the `wifiLink`/`mqttLink` poll block and before the feedback-apply-or-clearIndicator block; the feedback gate gains `&& !collision`.

- [ ] **Step 7: Verify all three hardware build targets, then the native suite**

Run (via Bash, never PowerShell):

```bash
pio run -e esp32node4
```
Expected: `SUCCESS`.

```bash
pio run -e esp32dev
pio run -e megaatmega2560
```
Expected: both still `SUCCESS` — this change touches only `src/esp32node4/main.cpp`.

```bash
pio test -e native
```
Expected: all 54 suites still pass (if you see a handful of spurious `ERRORED` suites, confirm no other `pio` process is running and re-run once in isolation — a known resource-contention artifact on this Windows environment, not a real failure).

- [ ] **Step 8: Commit**

Stage exactly this one file:
```bash
git add src/esp32node4/main.cpp
```

Commit using Arlo's Commit Notation:

```
@ F Wire presence/collision detection into node4 main.cpp

Adds GatedDigitalInput wrapping all 4 buttons, NodeIdentityGuard +
MqttPresenceAnnouncer subscribed to the mac topic, and folds collision
into the existing feedback-apply gate - mirroring src/esp32/main.cpp's
proven pattern exactly. Two node4 boards sharing a nodeId by mistake now
suppress their buttons and log the collision once, same as the panel.
Verified by pio run across all 3 environments; no native test possible
for a composition root.
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

Find the `### Loco2MQTT WiFi/MQTT wiring (sub-project #7)` section in `CLAUDE.md` (it should be the section immediately before `### Loco2MQTT turnout bridge (sub-project #9a)` — if a different section now sits between them, insert after whichever is last before #9a's heading). Insert a new section immediately after it:

```markdown
### Presence/collision detection (sub-project #8)

Closes the gap left open by sub-project #7's own non-goals list: the
node board had no way to detect a `nodeId` collision between two
physical boards. `src/esp32node4/main.cpp` now wires the panel's own
`NodeIdentityGuard`/`MqttPresenceAnnouncer` pair, unmodified, over the
real `MqttLink` sub-project #7 wired in. The precondition the panel's
own "Presence + collision detection" section (above) explains is
required — a MAC-derived, not `nodeId`-derived, MQTT client ID, so two
colliding boards stay simultaneously connected long enough to observe
each other — was already satisfied: sub-project #7 used
`"maltbee-node4-" + mac4` for exactly this reason, even though
collision detection itself wasn't wired yet at the time.

The node board's 4 buttons, previously raw `ArduinoDigitalInput`s fed
directly into `ToggleTurnoutStation`, are now each wrapped in a
`GatedDigitalInput` — mirroring the panel's `gatedButtons[12]` — so a
latched collision can suppress all 4 at once via
`gated.setSuppressed(collision)`. `setup()` subscribes to
`PresenceTopics::macTopic(nodeId)` with a lambda forwarding to
`identityGuard.onMacObserved()`, inside the existing `configValid` gate
that also calls `wifiLink.begin()`/`mqttLink.begin()`. `loop()` computes
`collision` once per tick from `identityGuard.collisionDetected()`,
logs it once via a `static` one-shot guard (reusing the panel's exact
wording, with "panel" swapped for "node"), suppresses all 4
`gatedButtons`, and folds `!collision` into the same feedback-apply gate
`clearIndicator()` already used for `configValid`/MQTT-connected —
identical structure to the panel's own `loop()`.
`presenceAnnouncer.update(mqttLink.connected())` runs unconditionally
every tick, same as the panel, driving the 30-second heartbeat that lets
two simultaneously-connected colliding boards each observe the other's
live MAC publish.

**Net effect:** two node4 boards accidentally commissioned with the
same `nodeId`, once both reach `BootMode::Normal` and connect to MQTT,
detect each other within 30 seconds (the heartbeat interval) and
suppress all 4 buttons on both, falling back to the blink/unconfirmed
LED state and logging the collision once over serial — identical
behavior and latency to the panel's existing mechanism.

**Test coverage added: none.** `main.cpp` is a composition root verified
by build only (`pio run -e esp32node4`/`esp32dev`/`megaatmega2560`), same
precedent as every prior `main.cpp`-only change in this project. Suite
count stays at 54.

**Non-goals, deferred:** the identify-overlay (`IdentifyModeTimer`, an
MQTT `panel/<nodeId>/identify` subscription, a `setIdentifying()` loop
driven by it) — a separate choice from this sub-project's scope,
deferred to whichever future sub-project adds it. Status/activity LEDs
and the 4-channel commissioning form remain their own future
sub-projects too.
```

- [ ] **Step 2: Sweep for stale cross-section claims (required — not optional)**

This project's last 3 consecutive sub-projects (#5, #6, #7) each had their final whole-branch review catch at least one now-false "future sub-project"/"blocked on"/"unwired"/"not done" claim left over in an earlier CLAUDE.md section — every time, because the doc task only targeted the one specific stale reference it already knew about instead of sweeping for all of them. Run this grep now, as a required step, not an afterthought:

```bash
grep -n "future sub-project\|blocked on\|unwired\|not done\|still\|yet" CLAUDE.md docs/pcb/turnout-node-4ch-pcb-design.md
```

Read every hit. For each one that describes presence/collision detection, `GatedDigitalInput`/button suppression, or `NodeIdentityGuard`/`MqttPresenceAnnouncer` as not yet done on the node board, annotate it in place using this project's established convention (an in-place note, not a deletion — e.g. `**Closed by sub-project #8 — see below.**` immediately before the stale sentence, or a short inline parenthetical where that reads more naturally, matching the style already used elsewhere in this file for sub-projects #4, #5, #6, and #7's own equivalent annotations). Expect at least these specific locations to need annotation (confirm by reading them, don't skip the grep and jump straight to this list — the grep may surface more):

- The `src/esp32node4/main.cpp` history bullet in "Current source layout" (search for "presence/collision and identify remain unwired" — this was itself a sub-project #7 annotation; it needs updating now that presence/collision is done, identify is not).
- Sub-project #1's section (search for "presence/collision detection and identify-blink remain unwired, each its own future sub-project" — only identify-blink remains unwired now).
- Sub-project #4's section, if it says anything about button suppression or collision not existing on the node board (read the whole section to check — it may not mention this at all, in which case there's nothing to annotate there).

Do not stop at the 3 locations listed above if the grep surfaces more — read every hit and judge each one on whether this sub-project made it false.

- [ ] **Step 3: Update the rooted-include tally if this sub-project changed it**

The 3 new includes added in Task 1 (`adapters/GatedDigitalInput.h`, `application/MqttPresenceAnnouncer.h`, `domain/NodeIdentityGuard.h`) are all rooted includes into `lib/McsEsp32` headers from `src/esp32node4/main.cpp` — but `src/` composition roots were never part of this tally to begin with (the tally only counts cross-library includes *within* `lib/`, i.e. one library's file including another library's header). Confirm this is still true by running:

```bash
grep -rn '#include "ports/\|#include "domain/\|#include "adapters/\|#include "application/"' lib/McsEsp32/src lib/McsLoconet/src --include='*.h' --include='*.cpp' | grep -v 'EspMdnsResolver.h.*ports/MdnsResolver.h'
```

If the count differs from 44, update CLAUDE.md's "Include convention" paragraph to match — do not guess or increment blindly. If unchanged at 44, make no edit here.

- [ ] **Step 4: Update `docs/pcb/turnout-node-4ch-pcb-design.md`'s §7 status line if it mentions presence/collision**

Read §7's current status line and item list. If it mentions presence/collision detection as not-yet-done for the node board anywhere (it may not — §7's numbered items 1-7 are about turnout count/buttons/LEDs/identify/status-LEDs/setup-trigger/unchanged-items, and presence/collision was never one of the 7 numbered items, so check item 7's "Unchanged" bullet specifically, which currently reads "Unchanged: ButtonSetupModeTrigger's hold logic, Loco2MQTT per-channel addresses, MqttPresenceAnnouncer, collision detection"), update it to reflect that these are now wired for the node board too, not just the panel. If nothing there is stale, make no edit — this doc's §7 is about the node4 PCB's firmware impact relative to the panel's design, not a running changelog, so it may simply not need a change for this sub-project.

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
. d Document sub-project #8 presence/collision detection

Adds the CLAUDE.md section and sweeps for now-false "unwired"/"not
done" claims about presence/collision detection left over in earlier
sections, per the staleness-sweep process fix adopted after sub-project
#7's final review.
```

---

## Self-Review Notes

**Spec coverage:** every element of the approved design spec
(`docs/superpowers/specs/2026-10-24-esp32node4-presence-collision-design.md`)
maps to a task — the exact globals, `setup()`/`loop()` insertion points,
the node-specific collision-log wording, and the explicit non-goal
(identify-overlay) are all reflected in Task 1's before/after code. Task
2 covers documentation, including a required general staleness sweep
rather than a single hard-coded annotation — the process fix this
effort's last 3 final reviews have each separately recommended.

**Placeholder scan:** no TBD/TODO; every code block is complete and
exact, reproducing the full current content of `src/esp32node4/main.cpp`
so Task 1's seven edit points are unambiguous.

**Type consistency:** `GatedDigitalInput`, `NodeIdentityGuard`, and
`MqttPresenceAnnouncer` are used with exactly the constructor signatures
their real headers declare (confirmed by reading those headers directly
before writing this plan). `gatedButtons[i]` replaces `buttons[i]` only
in the `stations[4]` array's button argument — `ToggleTurnoutStation`'s
own constructor signature is unchanged, since `GatedDigitalInput`
implements the same `DigitalInput` interface `buttons[i]` already
satisfied.
