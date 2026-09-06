# ESP32 Presence/Collision-Detection Heartbeat (Sub-project #9b) — Design

This is sub-project **#9b** of the Loco2MQTT pivot (see
`docs/superpowers/specs/2026-09-06-esp32-loco2mqtt-turnout-bridge-design.md`
for #9a, the first part, already merged). It redesigns the existing
presence/collision-detection feature (originally sub-project #2d-a —
`NodeIdentityGuard`, `MqttPresenceAnnouncer`, `PresenceTopics`) so it
actually works against Loco2MQTT's broker, which ignores the MQTT retained
flag entirely.

## Why this is needed

The #2d-a design let two panels that were accidentally commissioned with
the same `nodeId` detect each other: each panel publishes its own MAC,
**retained**, to `panel/<nodeId>/mac`, and subscribes to that same topic.
Because both panels used a `nodeId`-derived MQTT client ID
(`"maltbee-esp32-" + nodeId`), the broker's duplicate-clientId behavior
meant the two panels could never be connected at the same time — whichever
connected second would kick the first off. Without the retained flag, the
panel that just got kicked off would never be present on the topic when the
other one was there to observe it; **retention was the only reason the
non-connected panel's last claim stayed on the broker for the other to read
on its next connect.** Loco2MQTT's broker ignores the retained flag
outright (documented in the Loco2MQTT integration contract), so as shipped
today this feature is silently non-functional against it — two colliding
panels would simply never notice each other.

## Scope decisions (from user Q&A during brainstorming)

- **Heartbeat both `status` and `mac`**, not just `mac`. The same
  retained-message problem affects the `status` topic's usefulness to any
  external observer (e.g. `mosquitto_sub -t 'panel/#' -v`, mentioned in the
  hardware bringup checklist) — a panel that reconnects mid-session
  currently can't tell a late subscriber it's online, same root cause as
  the collision-detection gap. Fixing both in the same change is more
  consistent than fixing only the one collision detection strictly needs.
- **30-second heartbeat interval**, matching the cadence Loco2MQTT itself
  uses for its own turnout-state re-publish (already documented in this
  codebase as the reference interval for "how Loco2MQTT works around no
  retain"). Worst-case collision-detection latency: one panel notices the
  other within 30s of both being connected simultaneously.
- **`NodeIdentityGuard` is unchanged** — its self-echo immunity (a panel
  observing its own MAC is never a false collision) and latch-once
  semantics already handle a periodically-repeating self-observation
  correctly; both are already tested.

## Architecture

Two changes, both narrowly scoped:

**1. Unique per-panel MQTT client ID** (`src/esp32/main.cpp`). Today:
```cpp
const std::string mqttClientId = "maltbee-esp32-" + std::to_string(runningConfig.nodeId);
```
Two panels sharing a `nodeId` — the exact collision scenario — therefore
also share a client ID today, which is *why* the broker's duplicate-client
disconnect behavior was something #2d-a had to design around rather than
avoid. Change the client ID to derive from the panel's own MAC instead
(the same `ownMac.lastFourHexDigits()` value already used by `SetupApName`,
`NodeIdentityGuard`, and `MqttPresenceAnnouncer` for per-panel identity):
```cpp
const std::string mqttClientId = "maltbee-esp32-" + ownMac.lastFourHexDigits();
```
Now two colliding panels connect with distinct client IDs and the broker
lets both stay connected simultaneously — which the heartbeat below
requires in order to work at all (each panel needs to actually be online
at the same time as the other to observe its periodic publish).

**2. `MqttPresenceAnnouncer` becomes a heartbeat, not a pure edge trigger**
(`lib/McsEsp32/src/application/MqttPresenceAnnouncer.h`/`.cpp`). Gains a
`Clock&` dependency, following the exact pattern `IdentifyModeTimer`
already establishes in this codebase (elapsed-time check via
`nowMilliseconds()`, no internal `update()`-driven bookkeeping beyond a
single "last published at" timestamp):

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

Behavior: publishes immediately on the disconnect→connect edge (unchanged
from today), and again every 30 seconds thereafter while connected,
resetting the "last announced" timestamp on every publish (including the
edge one) so the first heartbeat lands a full 30s after the edge publish,
not immediately after. On disconnect, `wasConnected_` resets so the next
connect is treated as a fresh edge — matching today's "re-announces after a
disconnect and reconnect cycle" behavior exactly. Both publishes now use
`retained=false` — the flag is dead weight against Loco2MQTT and keeping
it `true` would misleadingly suggest it still matters.

**3. `src/esp32/main.cpp`'s construction call site** updates to pass
`systemClock` as the new second constructor argument:
```cpp
MqttPresenceAnnouncer presenceAnnouncer(mqttLink, systemClock, runningConfig.nodeId, ownMac.lastFourHexDigits());
```

## File layout

- `lib/McsEsp32/src/application/MqttPresenceAnnouncer.h`/`.cpp` — modified
  as above.
- `src/esp32/main.cpp` — two one-line changes (`mqttClientId` construction,
  `presenceAnnouncer` construction).
- `test/test_mqtt_presence_announcer/test_main.cpp` — modified (see
  Testing below).
- No new files. `NodeIdentityGuard`, `PresenceTopics`, `test_node_identity_guard`,
  `test_presence_topics` are all untouched.

## Error handling

No new failure modes. `MqttTransport::publish()` has always been a
fire-and-forget call with no return value to check (matches every other
publish call site in this codebase); the heartbeat doesn't change that
contract. If a heartbeat publish is silently dropped by the network (the
Loco2MQTT contract's own documented QoS-0-only, no-delivery-guarantee
posture), the next one 30 seconds later serves as its own retry — no
special handling needed, same as Loco2MQTT's own turnout-state heartbeat
already works this way.

## Testing

All-native, using the existing `FakeMqttTransport` plus a `FakeClock`
(both already exist in `test/support/`):

- Existing 4 test cases in `test/test_mqtt_presence_announcer/test_main.cpp`
  updated for the new constructor signature (`FakeClock` argument added)
  and `retained` assertions changed from `true` to `false`:
  - "update does not publish anything while never connected"
  - "update publishes online status and mac on the connect edge"
  - "update does not re-publish on every tick while still connected" (now
    specifically: doesn't re-publish before 30s of clock time have
    elapsed)
  - "update re-announces after a disconnect and reconnect cycle"
- New test cases:
  - "update re-publishes after the heartbeat interval elapses while still
    connected" — `update(true)`, advance the fake clock by
    `kHeartbeatIntervalMs`, `update(true)` again, expect a second
    status+mac publish pair.
  - "update does not re-publish just before the heartbeat interval
    elapses" — advance by `kHeartbeatIntervalMs - 1`, expect no second
    publish (boundary case, mirrors this codebase's existing boundary-test
    style for `IdentifyModeTimer`/`NodeConfig`).

## Out of scope for this slice

- Decommissioning `jmri/panel_mqtt_turnout_bridge.py` and its docs
  (sub-project #9c — unrelated to presence detection).
- Any change to `NodeIdentityGuard`'s collision logic itself, `PresenceTopics`'
  topic naming, or the identify-blink feature (#2d-b) — none of these are
  affected by the retained-flag problem this slice fixes.
- Clearing or aging out a stale collision latch, or any other change to
  the "known, accepted limitation" already documented for #2d-a (a
  decommissioned panel's stale MAC claim causing one false-positive boot
  for its replacement) — the heartbeat doesn't change that tradeoff either
  way, since it was never about retention in the first place (it's about
  the latch never clearing once tripped within a single boot).
