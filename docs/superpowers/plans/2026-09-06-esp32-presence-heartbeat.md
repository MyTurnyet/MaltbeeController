# ESP32 Presence/Collision-Detection Heartbeat (Sub-project #9b) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the ESP32 panel's presence/collision-detection feature work against Loco2MQTT's broker, which ignores the MQTT retained flag entirely — by giving each panel a unique MQTT client ID and turning its status/mac announcements into a 30-second heartbeat instead of a one-shot connect-edge publish.

**Architecture:** `MqttPresenceAnnouncer` gains a `Clock&` dependency (same elapsed-time pattern this codebase's `IdentifyModeTimer` already uses) so it can re-publish on a timer, not just on the disconnect→connect edge. `src/esp32/main.cpp`'s MQTT client ID changes from a `nodeId`-derived string to a MAC-derived one, so two panels sharing a `nodeId` (the collision case) no longer kick each other off the broker and can both stay connected long enough to observe each other's heartbeat.

**Tech Stack:** C++17, PlatformIO (`native` Catch2 tests, `esp32dev` Arduino/ESP32 build).

**Design doc:** `docs/superpowers/specs/2026-09-06-esp32-presence-heartbeat-design.md` — read it for the full rationale; this plan is the executable breakdown of it.

## Global Constraints

- Run `pio test -e native` (and any `-f <suite>` filtered run) via **Bash/Git Bash, not PowerShell** — PowerShell on this Windows machine produces false `ERRORED` results on commits that pass cleanly under Bash.
- `NodeIdentityGuard`, `PresenceTopics`, and their tests are explicitly **out of scope** — do not modify them. `NodeIdentityGuard`'s existing self-echo immunity and latch-once semantics already handle a heartbeat's repeated self-observation correctly.
- The heartbeat interval is **30000ms** (30 seconds), matching Loco2MQTT's own turnout-state re-publish cadence.
- Both `status` and `mac` publishes now use `retained=false` — Loco2MQTT ignores the flag, and keeping it `true` would misleadingly suggest it still matters.
- Every commit in this repo goes through the **`arlo-commits` skill**, never a raw `git commit`.
- No comments in code unless explaining a non-obvious "why".

---

### Task 1: `MqttPresenceAnnouncer` — heartbeat instead of edge-only publish

**Files:**
- Modify: `lib/McsEsp32/src/application/MqttPresenceAnnouncer.h`
- Modify: `lib/McsEsp32/src/application/MqttPresenceAnnouncer.cpp`
- Test: `test/test_mqtt_presence_announcer/test_main.cpp`

**Interfaces:**
- Consumes: `Clock` port (`lib/McsCore/src/ports/Clock.h`, existing — `nowMilliseconds() -> unsigned long`), `FakeClock` test double (`test/support/FakeClock.h`, existing — has an `advanceBy(unsigned long ms)` method, same one `IdentifyModeTimer`'s tests already use), `PresenceTopics::statusTopic(int)`/`macTopic(int)` (existing, unchanged), `MqttTransport::publish(topic, payload, retained)` (existing, unchanged).
- Produces: `MqttPresenceAnnouncer(MqttTransport&, Clock&, int nodeId, std::string ownMac)` (constructor signature changes — gains the `Clock&` parameter as the second argument), `update(bool currentlyConnected)` (unchanged signature, changed behavior), `MqttPresenceAnnouncer::kHeartbeatIntervalMs` (`static constexpr unsigned long`, value `30000`). Task 2 (`main.cpp` wiring) uses the new constructor signature.

- [ ] **Step 1: Replace the test file with the failing (heartbeat-aware) version**

Replace the full contents of `test/test_mqtt_presence_announcer/test_main.cpp` with:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "application/MqttPresenceAnnouncer.h"
#include "support/FakeClock.h"
#include "support/FakeMqttTransport.h"

TEST_CASE("update does not publish anything while never connected")
{
    FakeMqttTransport transport;
    FakeClock clock;
    MqttPresenceAnnouncer announcer(transport, clock, 5, "AAAA");

    announcer.update(false);

    REQUIRE(transport.published.empty());
}

TEST_CASE("update publishes online status and mac on the connect edge, not retained")
{
    FakeMqttTransport transport;
    FakeClock clock;
    MqttPresenceAnnouncer announcer(transport, clock, 5, "AAAA");

    announcer.update(true);

    REQUIRE(transport.published.size() == 2);
    REQUIRE(transport.published[0].topic == "panel/5/status");
    REQUIRE(transport.published[0].payload == "online");
    REQUIRE_FALSE(transport.published[0].retained);
    REQUIRE(transport.published[1].topic == "panel/5/mac");
    REQUIRE(transport.published[1].payload == "AAAA");
    REQUIRE_FALSE(transport.published[1].retained);
}

TEST_CASE("update does not re-publish before the heartbeat interval elapses")
{
    FakeMqttTransport transport;
    FakeClock clock;
    MqttPresenceAnnouncer announcer(transport, clock, 5, "AAAA");

    announcer.update(true);
    clock.advanceBy(MqttPresenceAnnouncer::kHeartbeatIntervalMs - 1);
    announcer.update(true);
    announcer.update(true);

    REQUIRE(transport.published.size() == 2);
}

TEST_CASE("update re-publishes once the heartbeat interval elapses while still connected")
{
    FakeMqttTransport transport;
    FakeClock clock;
    MqttPresenceAnnouncer announcer(transport, clock, 5, "AAAA");

    announcer.update(true);
    clock.advanceBy(MqttPresenceAnnouncer::kHeartbeatIntervalMs);
    announcer.update(true);

    REQUIRE(transport.published.size() == 4);
    REQUIRE(transport.published[2].topic == "panel/5/status");
    REQUIRE(transport.published[3].topic == "panel/5/mac");
}

TEST_CASE("update re-announces immediately after a disconnect and reconnect cycle")
{
    FakeMqttTransport transport;
    FakeClock clock;
    MqttPresenceAnnouncer announcer(transport, clock, 5, "AAAA");

    announcer.update(true);
    announcer.update(false);
    announcer.update(true);

    REQUIRE(transport.published.size() == 4);
    REQUIRE(transport.published[2].topic == "panel/5/status");
    REQUIRE(transport.published[3].topic == "panel/5/mac");
}
```

- [ ] **Step 2: Run the test to see it fail to compile**

Run (in Bash/Git Bash): `pio test -e native -f test_mqtt_presence_announcer`
Expected: build FAILS — `MqttPresenceAnnouncer`'s constructor doesn't take a `Clock&` yet, and `kHeartbeatIntervalMs` doesn't exist.

- [ ] **Step 3: Update `MqttPresenceAnnouncer.h`**

Replace the full contents of `lib/McsEsp32/src/application/MqttPresenceAnnouncer.h` with:

```cpp
#pragma once

#include <string>

#include "../domain/PresenceTopics.h"
#include "../ports/MqttTransport.h"
#include "ports/Clock.h"

class MqttPresenceAnnouncer
{
public:
    static constexpr unsigned long kHeartbeatIntervalMs = 30000;

    MqttPresenceAnnouncer(MqttTransport& transport, Clock& clock, int nodeId, std::string ownMac);

    void update(bool currentlyConnected);

private:
    void announce();

    MqttTransport& transport_;
    Clock& clock_;
    int nodeId_;
    std::string ownMac_;
    bool wasConnected_ = false;
    unsigned long lastAnnouncedAtMs_ = 0;
};
```

- [ ] **Step 4: Update `MqttPresenceAnnouncer.cpp`**

Replace the full contents of `lib/McsEsp32/src/application/MqttPresenceAnnouncer.cpp` with:

```cpp
#include "MqttPresenceAnnouncer.h"

MqttPresenceAnnouncer::MqttPresenceAnnouncer(MqttTransport& transport, Clock& clock, const int nodeId,
                                              std::string ownMac)
    : transport_(transport), clock_(clock), nodeId_(nodeId), ownMac_(std::move(ownMac))
{
}

void MqttPresenceAnnouncer::update(const bool currentlyConnected)
{
    if (!currentlyConnected)
    {
        wasConnected_ = false;
        return;
    }

    const bool justConnected = !wasConnected_;
    const bool heartbeatDue = clock_.nowMilliseconds() - lastAnnouncedAtMs_ >= kHeartbeatIntervalMs;

    if (justConnected || heartbeatDue)
    {
        announce();
    }

    wasConnected_ = true;
}

void MqttPresenceAnnouncer::announce()
{
    transport_.publish(PresenceTopics::statusTopic(nodeId_), "online", false);
    transport_.publish(PresenceTopics::macTopic(nodeId_), ownMac_, false);
    lastAnnouncedAtMs_ = clock_.nowMilliseconds();
}
```

- [ ] **Step 5: Run the test to see it pass**

Run: `pio test -e native -f test_mqtt_presence_announcer`
Expected: PASS (5 test cases).

- [ ] **Step 6: Run the full native suite**

Run: `pio test -e native`
Expected: PASS — every suite, unchanged elsewhere (nothing else in the native build references `MqttPresenceAnnouncer`).

- [ ] **Step 7: Commit**

Commit via the `arlo-commits` skill (message should reflect: `MqttPresenceAnnouncer` becomes a 30s heartbeat instead of a connect-edge-only publish, and drops the now-meaningless retained flag).

---

### Task 2: Wire `main.cpp` — unique client ID + heartbeat construction

**Files:**
- Modify: `src/esp32/main.cpp`

**Interfaces:**
- Consumes: `MqttPresenceAnnouncer`'s new constructor signature (Task 1), `MacAddress::lastFourHexDigits()` (existing, already used elsewhere in this same file for `SetupApName`/`NodeIdentityGuard`), `systemClock` (existing global `ArduinoClock`, already implements the `Clock` port — already used to construct `IdentifyModeTimer`, `mqttLink`, etc.).
- Produces: a fully wired, building `esp32dev` firmware. Nothing downstream depends on this task — it's the top of the stack.

This file is **not** part of the native test build (`test_build_src = false` in `platformio.ini`) — the only verification gate is `pio run -e esp32dev` actually compiling.

- [ ] **Step 1: Make the MQTT client ID unique per panel**

In `src/esp32/main.cpp`, find:

```cpp
const std::string mqttClientId = "maltbee-esp32-" + std::to_string(runningConfig.nodeId);
```

and change it to:

```cpp
const std::string mqttClientId = "maltbee-esp32-" + ownMac.lastFourHexDigits();
```

(`ownMac` is already declared earlier in this file, well before this line — it's the same global `SetupApName`/`NodeIdentityGuard`/`MqttPresenceAnnouncer` already use.)

- [ ] **Step 2: Pass `systemClock` into `MqttPresenceAnnouncer`'s construction**

In `src/esp32/main.cpp`, find:

```cpp
MqttPresenceAnnouncer presenceAnnouncer(mqttLink, runningConfig.nodeId, ownMac.lastFourHexDigits());
```

and change it to:

```cpp
MqttPresenceAnnouncer presenceAnnouncer(mqttLink, systemClock, runningConfig.nodeId, ownMac.lastFourHexDigits());
```

(`systemClock` is already declared earlier in this file — it's the same `ArduinoClock` instance already passed to `mqttLink`, `identifyTimer`, and every other clock-consuming object in this composition root.)

- [ ] **Step 3: Build-check the ESP32 firmware**

Run: `pio run -e esp32dev`
Expected: BUILD SUCCESS.

- [ ] **Step 4: Run the full native suite once more**

Run: `pio test -e native`
Expected: PASS — unaffected, since `main.cpp` isn't part of the native build.

- [ ] **Step 5: Commit**

Commit via the `arlo-commits` skill (message should reflect: give each panel a unique MQTT client ID derived from its MAC, so colliding panels no longer kick each other off the broker, and wire the new heartbeat-capable `MqttPresenceAnnouncer` constructor).

---

## Out of scope for this plan

Matches the design doc's "Out of scope" section: `jmri/panel_mqtt_turnout_bridge.py` decommissioning (sub-project #9c), any change to `NodeIdentityGuard`'s collision logic or `PresenceTopics`' topic naming, the identify-blink feature (#2d-b), and the already-documented "stale retained MAC on a decommissioned/reassigned panel" limitation (unaffected by this change either way).
