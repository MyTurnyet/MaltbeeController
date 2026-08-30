# ESP32 Presence + Collision Detection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the MQTT presence-topic asymmetry left as an explicit placeholder since sub-project #7b, and add active `nodeId`-collision self-detection to the ESP32 turnout panel firmware.

**Architecture:** Two small, native-testable classes (`PresenceTopics` for topic naming, `NodeIdentityGuard` for pure collision-comparison logic) plus one application-layer coordinator (`MqttPresenceAnnouncer`, wiring `MqttTransport` to the new topics on the connect edge), wired into `src/esp32/main.cpp`'s existing globals/`setup()`/`loop()` alongside the already-built `GatedDigitalInput` suppression mechanism from sub-project #2c-b2.

**Tech Stack:** C++17, PlatformIO (`native` for tests, `esp32dev` for the real build), Catch2 for native tests.

## Global Constraints

- New global declaration order in `src/esp32/main.cpp`: `const MacAddress ownMac = EspDeviceIdentity().mac();` goes immediately after `ArduinoClock systemClock;`. `NodeIdentityGuard identityGuard(...)` and `MqttPresenceAnnouncer presenceAnnouncer(...)` go immediately after the existing `MqttLink mqttLink(...)` declaration, since `presenceAnnouncer` takes `mqttLink` by reference.
- `EspDeviceIdentity().mac()` is now called at global scope (reversing sub-project #2c-b2's deferred-into-`setup()` choice). Justification, to preserve in code comments if a reviewer asks: `esp_efuse_mac_get_default()` reads directly from an eFuse register with no dependency on NVS or the WiFi driver stack — unlike this project's two real prior static-init bugs (NVS init timing in #7b, the `clock()`/`clock_t` symbol collision in #5). This also avoids introducing a `std::optional`-wrapped deferred-init global, which would be a first departure from this file's "every object is a plain, unconditionally-constructed global" pattern.
- Suppression composition in `loop()`: `gatedButtons[0]`/`gatedButtons[1]` are suppressed when `setupTrigger.isHolding() || collision`; `gatedButtons[2]` through `gatedButtons[11]` are suppressed when `collision` alone.
- The feedback/clear-indicator branch condition changes from `if (configValid && mqttLink.connected())` to `if (configValid && mqttLink.connected() && !collision)`.
- One-shot collision logging: a `static bool collisionLogged = false;` local inside `loop()`, checked so the serial log line prints exactly once per boot, not every tick.
- `mqttWillTopic`'s inline construction (`"panel/" + std::to_string(runningConfig.nodeId) + "/status"`) is replaced with `PresenceTopics::statusTopic(runningConfig.nodeId)`, so the LWT and the new "online" publish share one topic definition instead of two independently-typed string literals that could drift apart.
- `NodeIdentityGuard` has no `MqttTransport` dependency — it is pure comparison logic (own-MAC string vs. observed-MAC string), wired to MQTT only via a lambda in `main.cpp`'s composition root, not via a constructor dependency.
- `MqttPresenceAnnouncer::update()` must publish on the `false` → `true` connection edge only, never every tick while already connected, and must re-arm after a `true` → `false` → `true` cycle.
- `NodeIdentityGuard`'s collision latch never clears itself once tripped — a later `onMacObserved()` call with the panel's own MAC does not un-latch it.

---

### Task 1: `PresenceTopics`

**Files:**
- Create: `lib/McsEsp32/src/domain/PresenceTopics.h`
- Test: `test/test_presence_topics/test_main.cpp`

**Interfaces:**
- Produces: `static std::string PresenceTopics::statusTopic(int nodeId)`, `static std::string PresenceTopics::macTopic(int nodeId)` — both consumed directly by Task 3 (`MqttPresenceAnnouncer`) and by Task 4 (`main.cpp`'s `mqttWillTopic` and the new `mqttLink.subscribe(...)` call).

This mirrors `lib/McsEsp32/src/domain/TopicScheme.h`'s existing pattern exactly (a header-only class with two static string-building methods, no `.cpp` file, no test double needed since it takes only primitive arguments).

- [ ] **Step 1: Write the failing test**

Create `test/test_presence_topics/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/PresenceTopics.h"

TEST_CASE("statusTopic builds the panel status topic for a given node id")
{
    REQUIRE(PresenceTopics::statusTopic(5) == "panel/5/status");
}

TEST_CASE("macTopic builds the panel mac topic for a given node id")
{
    REQUIRE(PresenceTopics::macTopic(5) == "panel/5/mac");
}

TEST_CASE("both topics use the node id's decimal string form for a multi-digit id")
{
    REQUIRE(PresenceTopics::statusTopic(42) == "panel/42/status");
    REQUIRE(PresenceTopics::macTopic(42) == "panel/42/mac");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_presence_topics`
Expected: FAIL — "PresenceTopics.h: No such file or directory" (the header doesn't exist yet).

- [ ] **Step 3: Write the implementation**

Create `lib/McsEsp32/src/domain/PresenceTopics.h`:

```cpp
#pragma once

#include <string>

class PresenceTopics
{
public:
    static std::string statusTopic(int nodeId)
    {
        return "panel/" + std::to_string(nodeId) + "/status";
    }

    static std::string macTopic(int nodeId)
    {
        return "panel/" + std::to_string(nodeId) + "/mac";
    }
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_presence_topics`
Expected: PASS — all 3 test cases pass.

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: all suites pass (37, up from 36 — this is a new test binary).

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/domain/PresenceTopics.h test/test_presence_topics/test_main.cpp
git commit -m "feat: add PresenceTopics for panel status/mac topic naming"
```

(Classify this commit yourself using Arlo's Commit Notation per this project's CLAUDE.md — do not use the message above literally. See "Committing" note at the end of this plan.)

---

### Task 2: `NodeIdentityGuard`

**Files:**
- Create: `lib/McsEsp32/src/domain/NodeIdentityGuard.h`
- Create: `lib/McsEsp32/src/domain/NodeIdentityGuard.cpp`
- Test: `test/test_node_identity_guard/test_main.cpp`

**Interfaces:**
- Produces: `NodeIdentityGuard(std::string ownMac)` constructor; `void onMacObserved(const std::string& observedMac)`; `[[nodiscard]] bool collisionDetected() const` — consumed by Task 4 (`main.cpp`'s composition root, via a lambda forwarding MQTT-subscribed payloads into `onMacObserved()`, and a direct call to `collisionDetected()` each `loop()` tick).

This is pure comparison logic — no `MqttTransport` dependency, no Arduino dependency, no `Clock` dependency. Independent of Task 1 and Task 3; can be built in either order relative to them.

- [ ] **Step 1: Write the failing tests**

Create `test/test_node_identity_guard/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/NodeIdentityGuard.h"

TEST_CASE("no collision is detected before any mac has been observed")
{
    NodeIdentityGuard guard("AAAA");

    REQUIRE_FALSE(guard.collisionDetected());
}

TEST_CASE("observing the panel's own mac does not trigger a collision")
{
    NodeIdentityGuard guard("AAAA");

    guard.onMacObserved("AAAA");

    REQUIRE_FALSE(guard.collisionDetected());
}

TEST_CASE("observing a different mac triggers a collision")
{
    NodeIdentityGuard guard("AAAA");

    guard.onMacObserved("BBBB");

    REQUIRE(guard.collisionDetected());
}

TEST_CASE("a detected collision does not clear when the own mac is observed again")
{
    NodeIdentityGuard guard("AAAA");

    guard.onMacObserved("BBBB");
    guard.onMacObserved("AAAA");

    REQUIRE(guard.collisionDetected());
}

TEST_CASE("observing the own mac multiple times in a row never trips a false positive")
{
    NodeIdentityGuard guard("AAAA");

    guard.onMacObserved("AAAA");
    guard.onMacObserved("AAAA");
    guard.onMacObserved("AAAA");

    REQUIRE_FALSE(guard.collisionDetected());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_node_identity_guard`
Expected: FAIL — "NodeIdentityGuard.h: No such file or directory".

- [ ] **Step 3: Write the implementation**

Create `lib/McsEsp32/src/domain/NodeIdentityGuard.h`:

```cpp
#pragma once

#include <string>

class NodeIdentityGuard
{
public:
    explicit NodeIdentityGuard(std::string ownMac);

    void onMacObserved(const std::string& observedMac);
    [[nodiscard]] bool collisionDetected() const;

private:
    std::string ownMac_;
    bool collisionDetected_ = false;
};
```

Create `lib/McsEsp32/src/domain/NodeIdentityGuard.cpp`:

```cpp
#include "NodeIdentityGuard.h"

NodeIdentityGuard::NodeIdentityGuard(std::string ownMac) : ownMac_(std::move(ownMac))
{
}

void NodeIdentityGuard::onMacObserved(const std::string& observedMac)
{
    if (observedMac != ownMac_)
    {
        collisionDetected_ = true;
    }
}

bool NodeIdentityGuard::collisionDetected() const
{
    return collisionDetected_;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_node_identity_guard`
Expected: PASS — all 5 test cases pass.

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: all suites pass (38, up from 37 after Task 1).

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/domain/NodeIdentityGuard.h lib/McsEsp32/src/domain/NodeIdentityGuard.cpp test/test_node_identity_guard/test_main.cpp
git commit -m "feat: add NodeIdentityGuard for nodeId collision detection"
```

(Classify using ACN — see "Committing" note at the end.)

---

### Task 3: `MqttPresenceAnnouncer`

**Files:**
- Create: `lib/McsEsp32/src/application/MqttPresenceAnnouncer.h`
- Create: `lib/McsEsp32/src/application/MqttPresenceAnnouncer.cpp`
- Test: `test/test_mqtt_presence_announcer/test_main.cpp`

**Interfaces:**
- Consumes: `PresenceTopics::statusTopic(int)`/`PresenceTopics::macTopic(int)` (Task 1); `MqttTransport::publish(const std::string& topic, const std::string& payload, bool retained)` (existing port, `lib/McsEsp32/src/ports/MqttTransport.h`).
- Produces: `MqttPresenceAnnouncer(MqttTransport& transport, int nodeId, std::string ownMac)` constructor; `void update(bool currentlyConnected)` — consumed by Task 4 (`main.cpp`, called once per `loop()` tick with `mqttLink.connected()`).

Depends on Task 1 (`PresenceTopics`) but not on Task 2. This is the first class in `lib/McsEsp32/src/application/` this plan introduces — it follows the same pattern as the existing `lib/McsEsp32/src/application/CommissioningSession.h`/`.cpp` (header declares the public interface, `.cpp` holds the implementation, both files use relative includes within the library).

- [ ] **Step 1: Write the failing tests**

Create `test/test_mqtt_presence_announcer/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "application/MqttPresenceAnnouncer.h"
#include "support/FakeMqttTransport.h"

TEST_CASE("update does not publish anything while never connected")
{
    FakeMqttTransport transport;
    MqttPresenceAnnouncer announcer(transport, 5, "AAAA");

    announcer.update(false);

    REQUIRE(transport.published.empty());
}

TEST_CASE("update publishes online status and mac on the connect edge")
{
    FakeMqttTransport transport;
    MqttPresenceAnnouncer announcer(transport, 5, "AAAA");

    announcer.update(true);

    REQUIRE(transport.published.size() == 2);
    REQUIRE(transport.published[0].topic == "panel/5/status");
    REQUIRE(transport.published[0].payload == "online");
    REQUIRE(transport.published[0].retained);
    REQUIRE(transport.published[1].topic == "panel/5/mac");
    REQUIRE(transport.published[1].payload == "AAAA");
    REQUIRE(transport.published[1].retained);
}

TEST_CASE("update does not re-publish on every tick while still connected")
{
    FakeMqttTransport transport;
    MqttPresenceAnnouncer announcer(transport, 5, "AAAA");

    announcer.update(true);
    announcer.update(true);
    announcer.update(true);

    REQUIRE(transport.published.size() == 2);
}

TEST_CASE("update re-announces after a disconnect and reconnect cycle")
{
    FakeMqttTransport transport;
    MqttPresenceAnnouncer announcer(transport, 5, "AAAA");

    announcer.update(true);
    announcer.update(false);
    announcer.update(true);

    REQUIRE(transport.published.size() == 4);
    REQUIRE(transport.published[2].topic == "panel/5/status");
    REQUIRE(transport.published[3].topic == "panel/5/mac");
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_mqtt_presence_announcer`
Expected: FAIL — "MqttPresenceAnnouncer.h: No such file or directory".

- [ ] **Step 3: Write the implementation**

Create `lib/McsEsp32/src/application/MqttPresenceAnnouncer.h`:

```cpp
#pragma once

#include <string>

#include "../domain/PresenceTopics.h"
#include "../ports/MqttTransport.h"

class MqttPresenceAnnouncer
{
public:
    MqttPresenceAnnouncer(MqttTransport& transport, int nodeId, std::string ownMac);

    void update(bool currentlyConnected);

private:
    MqttTransport& transport_;
    int nodeId_;
    std::string ownMac_;
    bool wasConnected_ = false;
};
```

Create `lib/McsEsp32/src/application/MqttPresenceAnnouncer.cpp`:

```cpp
#include "MqttPresenceAnnouncer.h"

MqttPresenceAnnouncer::MqttPresenceAnnouncer(MqttTransport& transport, const int nodeId, std::string ownMac)
    : transport_(transport), nodeId_(nodeId), ownMac_(std::move(ownMac))
{
}

void MqttPresenceAnnouncer::update(const bool currentlyConnected)
{
    if (currentlyConnected && !wasConnected_)
    {
        transport_.publish(PresenceTopics::statusTopic(nodeId_), "online", true);
        transport_.publish(PresenceTopics::macTopic(nodeId_), ownMac_, true);
    }
    wasConnected_ = currentlyConnected;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_mqtt_presence_announcer`
Expected: PASS — all 4 test cases pass.

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: all suites pass (39, up from 38 after Task 2).

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/application/MqttPresenceAnnouncer.h lib/McsEsp32/src/application/MqttPresenceAnnouncer.cpp test/test_mqtt_presence_announcer/test_main.cpp
git commit -m "feat: add MqttPresenceAnnouncer for online/offline presence publishing"
```

(Classify using ACN — see "Committing" note at the end.)

---

### Task 4: Wire everything into `src/esp32/main.cpp`

**Files:**
- Modify: `src/esp32/main.cpp`

**Interfaces:**
- Consumes: `PresenceTopics::statusTopic(int)`/`macTopic(int)` (Task 1); `NodeIdentityGuard(std::string)`, `void onMacObserved(const std::string&)`, `bool collisionDetected() const` (Task 2); `MqttPresenceAnnouncer(MqttTransport&, int, std::string)`, `void update(bool)` (Task 3); `EspDeviceIdentity::mac()` returning `MacAddress` (existing, `#2c-b1`); `MacAddress::lastFourHexDigits()` returning `std::string` (existing, `#2c-b1`); `GatedDigitalInput::setSuppressed(bool)` (existing, `#2c-a`); `MqttLink::subscribe(const std::string&, std::function<void(const std::string&)>)` (existing, `MqttTransport` port).
- Produces: nothing consumed by a later task — this is the final task.

This is composition-root restructuring with zero test coverage of its own (`src/` is excluded from `native`, `test_build_src = false` in `platformio.ini`) — the same situation as sub-project #2c-b2's own main.cpp wiring task. Verification is `pio run -e esp32dev` build success plus the numbered manual read-through checklist at Step 8. Depends on Tasks 1, 2, and 3 all being complete. Do not improvise beyond what's written here; every changed line is specified.

Read the current `src/esp32/main.cpp` yourself before editing to confirm it matches every "replace this" block below exactly — this file was last touched by sub-project #2c-b2 and nothing since, but verify rather than assume.

- [ ] **Step 1: Add the new includes**

Replace the existing include block:

```cpp
#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ArduinoDigitalOutput.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/ComboSetupModeTrigger.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspUartPort.h"
#include "adapters/GatedDigitalInput.h"
#include "adapters/JmriFeedbackSource.h"
#include "adapters/JmriTurnoutCommandAdapter.h"
#include "adapters/LedPairStation.h"
#include "adapters/MatrixDigitalInput.h"
#include "adapters/MqttLink.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "adapters/WiFiLink.h"
#include "application/CommissioningSession.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/LedPairDriver.h"
#include "domain/MatrixScanner.h"
#include "domain/NodeConfig.h"
#include "domain/SetupApName.h"
#include "ports/TurnoutCommandPort.h"
```

with:

```cpp
#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ArduinoDigitalOutput.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/ComboSetupModeTrigger.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspUartPort.h"
#include "adapters/GatedDigitalInput.h"
#include "adapters/JmriFeedbackSource.h"
#include "adapters/JmriTurnoutCommandAdapter.h"
#include "adapters/LedPairStation.h"
#include "adapters/MatrixDigitalInput.h"
#include "adapters/MqttLink.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "adapters/WiFiLink.h"
#include "application/CommissioningSession.h"
#include "application/MqttPresenceAnnouncer.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/LedPairDriver.h"
#include "domain/MatrixScanner.h"
#include "domain/NodeConfig.h"
#include "domain/NodeIdentityGuard.h"
#include "domain/PresenceTopics.h"
#include "domain/SetupApName.h"
#include "ports/TurnoutCommandPort.h"
```

`domain/MacAddress.h` needs no separate include — `adapters/EspDeviceIdentity.h` already includes it internally, and `ownMac`'s type is visible transitively, matching how `SetupApName::from(identity.mac())` already worked without an explicit `MacAddress.h` include before this change.

- [ ] **Step 2: Add the `ownMac` global**

Replace:

```cpp
ArduinoClock systemClock;

EspUartPort uartPort(UART_BAUD_RATE);
```

with:

```cpp
ArduinoClock systemClock;

const MacAddress ownMac = EspDeviceIdentity().mac();

EspUartPort uartPort(UART_BAUD_RATE);
```

- [ ] **Step 3: Replace the `mqttWillTopic` construction**

Replace:

```cpp
const std::string mqttWillTopic = "panel/" + std::to_string(runningConfig.nodeId) + "/status";
```

with:

```cpp
const std::string mqttWillTopic = PresenceTopics::statusTopic(runningConfig.nodeId);
```

- [ ] **Step 4: Add the `identityGuard`/`presenceAnnouncer` globals**

Replace:

```cpp
MqttLink mqttLink(systemClock, RETRY_INTERVAL_MS, mqttClientId, mqttWillTopic, mqttWillMessage);

JmriTurnoutCommandAdapter turnoutCommandPort(mqttLink, runningConfig.channelJmriNames);
```

with:

```cpp
MqttLink mqttLink(systemClock, RETRY_INTERVAL_MS, mqttClientId, mqttWillTopic, mqttWillMessage);

NodeIdentityGuard identityGuard(ownMac.lastFourHexDigits());
MqttPresenceAnnouncer presenceAnnouncer(mqttLink, runningConfig.nodeId, ownMac.lastFourHexDigits());

JmriTurnoutCommandAdapter turnoutCommandPort(mqttLink, runningConfig.channelJmriNames);
```

- [ ] **Step 5: Simplify the `WirelessSetup` branch of `setup()` to reuse `ownMac`, and add the mac-topic subscription**

Replace:

```cpp
    if (bootMode == BootMode::WirelessSetup)
    {
        EspDeviceIdentity identity;
        const std::string apName = SetupApName::from(identity.mac());
        captivePortalServer.begin(apName, WIRELESS_SETUP_AP_PASSPHRASE);
        uartPort.write("MaltBee panel in wireless setup mode. Connect to " + apName + "\n");
        return;
    }

    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
    }
```

with:

```cpp
    if (bootMode == BootMode::WirelessSetup)
    {
        const std::string apName = SetupApName::from(ownMac);
        captivePortalServer.begin(apName, WIRELESS_SETUP_AP_PASSPHRASE);
        uartPort.write("MaltBee panel in wireless setup mode. Connect to " + apName + "\n");
        return;
    }

    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
        mqttLink.subscribe(PresenceTopics::macTopic(runningConfig.nodeId),
                            [](const std::string& payload) { identityGuard.onMacObserved(payload); });
    }
```

`ownMac` is now computed once as a global (Step 2) instead of constructing a second, redundant `EspDeviceIdentity` locally here — this removes a duplicate MAC read that existed only because `setup()` previously had no other source for the MAC. The lambda passed to `subscribe()` needs no capture list: `identityGuard` is a named global, already visible by the time `setup()` runs.

- [ ] **Step 6: Add collision detection, logging, and expanded suppression to `loop()`**

Replace:

```cpp
    matrixScanner.update();

    setupTrigger.update();
    gatedButtons[0].setSuppressed(setupTrigger.isHolding());
    gatedButtons[1].setSuppressed(setupTrigger.isHolding());

    if (setupTrigger.requested())
```

with:

```cpp
    matrixScanner.update();

    setupTrigger.update();

    const bool collision = identityGuard.collisionDetected();
    static bool collisionLogged = false;
    if (collision && !collisionLogged)
    {
        uartPort.write("NodeId collision detected: another panel is claiming this node id.\n");
        collisionLogged = true;
    }

    gatedButtons[0].setSuppressed(setupTrigger.isHolding() || collision);
    gatedButtons[1].setSuppressed(setupTrigger.isHolding() || collision);
    for (int i = 2; i < 12; ++i)
    {
        gatedButtons[i].setSuppressed(collision);
    }

    if (setupTrigger.requested())
```

`collision` is computed once, early in the tick, and reused consistently below for both suppression and the feedback-gating check in Step 7 — this avoids any within-tick inconsistency between "collision as of this check" and "collision as of that check." It reflects whatever `identityGuard` last observed as of the end of the *previous* tick's `mqttLink.poll()` call (a one-tick lag versus computing it after this tick's poll) — inconsequential given the matrix-scan loop runs many times per second and collision detection has no real-time requirement.

- [ ] **Step 7: Add the presence announcement call and gate feedback on `collision`**

Replace:

```cpp
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

with:

```cpp
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

`presenceAnnouncer.update()` is called unconditionally every tick (safe even when `configValid` is false — `mqttLink.connected()` is simply always false in that case, so `update(false)` is a no-op).

- [ ] **Step 8: Run the esp32dev build**

Run: `pio run -e esp32dev`
Expected: SUCCESS. Note the RAM/Flash usage reported — expect a small increase over #2c-b2's baseline (RAM 16.8%/Flash 80.9%), since two new small classes are now genuinely linked in.

- [ ] **Step 9: Run the megaatmega2560 build (regression check)**

Run: `pio run -e megaatmega2560`
Expected: SUCCESS, usage unchanged — `src/esp32/main.cpp` is excluded from this environment's `build_src_filter` entirely.

- [ ] **Step 10: Run the full native suite (regression check)**

Run: `pio test -e native`
Expected: all suites pass, unchanged count from Task 3 (this file is never part of the `native` build).

- [ ] **Step 11: Manually re-read `setup()`/`loop()` against this task's target code**

Confirm line-by-line, citing the actual file's line numbers in your report:

(a) `ownMac` is declared once, as a global, immediately after `systemClock` — not inside any function, not constructed a second time anywhere else in the file (the old `EspDeviceIdentity identity;` local inside the `WirelessSetup` branch is gone).

(b) `identityGuard` and `presenceAnnouncer` are declared after `mqttLink` and before `turnoutCommandPort` — confirm this compiles (it does, per Step 8, but confirm the *declaration order* itself, since a reviewer without a successful build in front of them needs to be able to verify this from the diff alone).

(c) The `mqttLink.subscribe(...)` call appears exactly once, inside the `if (configValid)` block in `setup()`, after `mqttLink.begin(...)` — not before it, and not duplicated in `loop()`.

(d) `collision` is computed exactly once per `loop()` tick, before the suppression lines, and the *same* local is reused (not recomputed) in the feedback-gating `if` later in the same tick.

(e) All 12 `gatedButtons` entries get a `setSuppressed()` call every tick — indices 0/1 via `setupTrigger.isHolding() || collision`, indices 2 through 11 via the `for` loop on `collision` alone. No index is skipped, no index is set twice.

(f) `collisionLogged` is a `static` local inside `loop()` (not a global, not a member of any class) — confirm it persists across calls as intended and that the log line is genuinely unreachable a second time once `collisionLogged` is `true`.

(g) `presenceAnnouncer.update(mqttLink.connected())` is called exactly once per tick, unconditionally (not nested inside `if (configValid)`).

- [ ] **Step 12: Commit**

```bash
git add src/esp32/main.cpp
git commit -m "feat: wire presence announcement and collision detection into main.cpp"
```

(Classify using ACN — see "Committing" note at the end.)

---

## Committing

This project's `CLAUDE.md` requires every commit to be classified with Arlo's Commit Notation (`<risk symbol> <intention letter> <description>`) via the `/arlo-commits` skill's rules, not the placeholder `feat:`-style messages shown in this plan's steps above. When executing this plan (directly or via a dispatched implementer), replace each placeholder commit message with a real ACN-classified one:
- Any `F`/`B` commit touching more than 8 lines (including test changes) is automatically capped at `!` (Risky), never `^`/`.`, regardless of test coverage — check the actual diff size before choosing a symbol.
- Task 4's `main.cpp` commit has no native test coverage at all (by design — `test_build_src = false` excludes `src/`), which independently supports `!` there on top of the line-count rule.
- Format the summary line with a literal space between the risk symbol and the intention letter (`! F description`, not `!F description`).
