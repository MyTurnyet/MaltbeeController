# ESP32 Presence + Collision Detection (Sub-project #2d-a) — Design

This is sub-project **#2d-a**, split out of sub-project #2d (multi-panel
field identification + collision safety) — the next item in the ESP32
panel decomposition after the now-complete #2c wireless-commissioning arc
(#2c-a/#2c-b1/#2c-b2, all merged). This spec covers the MQTT presence
scheme and active `nodeId`-collision self-detection. Identify-blink (a way
for a technician to make a specific physical panel visually identify
itself) is a separate, independently valuable piece of work and gets its
own brainstorm as sub-project **#2d-b**.

## Context

Sub-project #7b's composition-root spec left two explicit placeholders in
`src/esp32/main.cpp`, both flagged as "pending sub-project #2d":

- MQTT `clientId = "maltbee-esp32-" + nodeId` — already real and correct
  (must be unique per node so the broker doesn't kick a duplicate client
  id), but noted as "the first real use of `nodeId`... ahead of #2d's full
  presence/collision design."
- MQTT LWT `willTopic = "panel/" + nodeId + "/status"`,
  `willMessage = "offline"` — explicit placeholders, since "#2d is
  expected to define the real presence-detection topic scheme."

Reading `lib/McsEsp32/src/adapters/MqttLink.cpp`'s `connect()` confirms
the LWT is already wired correctly at the protocol level
(`client_.connect(clientId_, willTopic_, 1, /*retain=*/true,
willMessage_)`) — the broker will publish a retained `"offline"` to
`willTopic_` if this panel's connection drops uncleanly. But nothing in
this codebase ever publishes the complementary `"online"` message on a
*successful* connect, so the scheme is asymmetric: a panel that
disconnects leaves a permanent, stale `"offline"` behind, and a panel that
reconnects never clears it.

Separately, commissioning today has no way to detect that two physical
panels have been given the same `nodeId` (a real operator-error risk, not
theoretical — `nodeId` is entered locally per panel with no central
authority checking uniqueness before save). Since `clientId` is derived
from `nodeId`, a duplicate would cause the MQTT broker to silently kick
whichever panel connected first every time the second one (re)connects —
a real signal today, but one neither panel can currently observe or act
on.

## Decisions (confirmed via Q&A)

1. **Split #2d into #2d-a (this spec) and #2d-b** (identify-blink),
   mirroring #7/#7a/#7b and #2c/#2c-a/#2c-b/#2c-b1/#2c-b2.
2. **Collision handling: active self-detection**, not just diagnostic
   visibility. Each panel publishes its own MAC-derived identity to a
   retained topic and subscribes to that same topic; observing a value
   that isn't its own means another panel is currently claiming the same
   `nodeId`. On detection: suppress all 12 turnout buttons, force every
   LED into the existing blink/unconfirmed state, and log once via
   serial. Recovery requires the operator to recommission one of the two
   panels with a different `nodeId` and reboot — matching this project's
   existing "config changes take effect on reboot" convention throughout;
   no automatic self-healing without one.
3. **LED indication reuses the existing blink/unconfirmed state** rather
   than building a new, visually distinct pattern. This was explicitly
   flagged as a narrower reading than "a distinct pattern," discussed
   during design, and accepted: it needs zero new `LedPairDriver` logic,
   and a technician actively investigating a suspected collision would
   already be watching serial output or the `panel/<nodeId>/mac` topic
   (which shows exactly which MAC currently holds the claim), not judging
   collision status by LED color alone.
4. **`EspDeviceIdentity::mac()` is now called at global scope** in
   `main.cpp`, reversing #2c-b2's deferred-into-`setup()` choice. #2c-b2
   only needed the MAC conditionally, in the `WirelessSetup` boot branch;
   #2d-a needs it unconditionally, every boot, to construct globals. Global
   scope is safe here because `esp_efuse_mac_get_default()` reads directly
   from an eFuse register with no dependency on NVS or the WiFi driver
   stack — unlike this project's two real prior static-init bugs (NVS
   init timing in #7b, the `clock()`/`clock_t` symbol collision in #5).
   This also avoids introducing `std::optional`-wrapped deferred-init
   globals, which would be a first departure from this file's established
   "every object is a plain, unconditionally-constructed global" pattern.

## Components

### `PresenceTopics` (`lib/McsEsp32/src/domain/PresenceTopics.h`)

Mirrors the existing `TopicScheme` pattern exactly:

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

`statusTopic()` matches the LWT `willTopic` already constructed inline in
`main.cpp` today (`"panel/" + std::to_string(runningConfig.nodeId) +
"/status"`) — this class replaces that inline construction so both the
LWT and the new "online" publish use one shared definition, not two
independently-typed string literals that could drift apart.

### `NodeIdentityGuard` (`lib/McsEsp32/src/domain/NodeIdentityGuard.h`/`.cpp`)

Pure comparison logic, no transport dependency — native-testable with no
fakes beyond plain strings:

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

```cpp
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

Latches permanently once tripped (never clears itself) — deliberate, so a
brief flicker during a reconnect race can't cause the collision state to
flap on and off, which would be more confusing than a state that stays
lit until an operator actually intervenes and reboots.

### `MqttPresenceAnnouncer` (`lib/McsEsp32/src/application/MqttPresenceAnnouncer.h`/`.cpp`)

Wires `MqttTransport` (a port) to the new topics — sits in `application/`
alongside `TurnoutControl`/`ToggleTurnoutControl`, which similarly
coordinate ports:

```cpp
#pragma once

#include <string>

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

```cpp
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

Fires exactly once per connect edge (`false` → `true`), not every tick
while connected — calling `update(mqttLink.connected())` unconditionally
every `loop()` tick is safe and cheap. Re-announces correctly after a
disconnect/reconnect cycle (`wasConnected_` resets to `false` on the
disconnect tick).

### `src/esp32/main.cpp` changes

New global, placed right after `ArduinoClock systemClock;`:

```cpp
const MacAddress ownMac = EspDeviceIdentity().mac();
```

New globals, placed after the existing `MqttLink mqttLink(...)`
declaration (so they can reference it):

```cpp
NodeIdentityGuard identityGuard(ownMac.lastFourHexDigits());
MqttPresenceAnnouncer presenceAnnouncer(mqttLink, runningConfig.nodeId, ownMac.lastFourHexDigits());
```

The existing inline LWT construction:

```cpp
const std::string mqttWillTopic = "panel/" + std::to_string(runningConfig.nodeId) + "/status";
```

becomes:

```cpp
const std::string mqttWillTopic = PresenceTopics::statusTopic(runningConfig.nodeId);
```

In `setup()`, inside the existing `if (configValid) { wifiLink.begin(...);
mqttLink.begin(...); }` block, add the subscription right after
`mqttLink.begin(...)`:

```cpp
mqttLink.subscribe(PresenceTopics::macTopic(runningConfig.nodeId),
                    [](const std::string& payload) { identityGuard.onMacObserved(payload); });
```

The lambda needs no capture list — `identityGuard` is a named global,
already visible by the time `setup()` runs. `MqttLink::connect()` already
replays every subscribed topic's handler on every successful (re)connect
(existing behavior, unchanged), so this one-time `subscribe()` call is
sufficient for the lifetime of the program.

In `loop()`, add right after the existing `wifiLink.poll()`/`mqttLink.poll()`
block:

```cpp
presenceAnnouncer.update(mqttLink.connected());

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
```

This replaces the existing two-line `setSuppressed` calls (currently only
covering indices 0/1 against `setupTrigger.isHolding()` alone) — the
combo-trigger suppression and collision suppression now compose via `||`
for buttons 0/1, and collision alone drives buttons 2-11.

The existing feedback/clear-indicator branch:

```cpp
if (configValid && mqttLink.connected())
{
    // ...applyFeedback...
}
else
{
    for (auto& station : stations) { station.clearIndicator(); }
}
```

becomes:

```cpp
if (configValid && mqttLink.connected() && !collision)
{
    // ...applyFeedback... (unchanged)
}
else
{
    for (auto& station : stations) { station.clearIndicator(); }
}
```

`presenceAnnouncer.update()` is called unconditionally (safe even when
`configValid` is false, since `mqttLink.connected()` is simply always
false in that case and `update(false)` is a no-op). The suppression and
LED-clearing logic both read `collision`, computed once per tick from
`identityGuard.collisionDetected()`.

## Testing

- **`PresenceTopics`**: `statusTopic()`/`macTopic()` produce the expected
  strings for representative node ids (including a multi-digit one).
- **`NodeIdentityGuard`**: constructing with an own-mac and calling
  `onMacObserved()` with the same value leaves `collisionDetected()`
  false; calling it with a different value flips it true; calling it with
  the own value *again* after a collision was already detected leaves it
  true (the latch doesn't clear); an own-mac echo received before any
  foreign value never trips it (guards against a false positive from
  receiving your own just-published retained message back).
- **`MqttPresenceAnnouncer`**, via `FakeMqttTransport`: `update(false)`
  publishes nothing; `update(true)` publishes both topics retained with
  the exact expected payloads; a second `update(true)` while still
  connected publishes nothing further (not spammed every tick);
  `update(false)` then `update(true)` again re-publishes (the edge
  re-arms after a disconnect/reconnect cycle).
- **`src/esp32/main.cpp`**: no native test (`src/` is excluded from
  `native`, `test_build_src = false`). Verified via `pio run -e esp32dev`
  build success and a manual read-through confirming: the subscription is
  registered before any `mqttLink.poll()` call could matter, the
  suppression composition (`||` for 0/1, plain for 2-11) is correct, and
  `collisionLogged` genuinely only fires the log line once.

## File layout

- `lib/McsEsp32/src/domain/`: `PresenceTopics.h`, `NodeIdentityGuard.h`/`.cpp`
- `lib/McsEsp32/src/application/`: `MqttPresenceAnnouncer.h`/`.cpp`
- `test/test_presence_topics/`, `test/test_node_identity_guard/`,
  `test/test_mqtt_presence_announcer/`
- Modify: `src/esp32/main.cpp` (new globals, subscription wiring, `loop()`
  suppression/clear-indicator changes)

## Non-goals

- Identify-blink — sub-project #2d-b, its own brainstorm.
- A proactive pre-commission collision check (querying the broker before
  allowing `save`) — would require network access during commissioning,
  which wireless setup's isolated-AP architecture doesn't have, and bench
  serial commissioning has no live broker connection either.
- Automatic recovery from a detected collision without an operator
  reboot.
- Any change to `GatedDigitalInput`, `ComboSetupModeTrigger`,
  `LedPairDriver`, `ToggleTurnoutStation`, or any class from #2c-a/#2c-b1/
  #2c-b2 — all reused exactly as built.
