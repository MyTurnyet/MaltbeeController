# 4-Turnout Node: Presence/Collision Detection (Sub-project #8) — Design

This is sub-project **#8** of the 4-turnout node board effort. It depends
on sub-project #7
(`docs/superpowers/specs/2026-10-22-esp32-node4-loco2mqtt-wiring-design.md`,
the real `WiFiLink`/`MqttLink` stack now wired into `src/esp32node4/main.cpp`).

## Problem

The node board has no way to detect a `nodeId` collision — if two
physical node4 boards are accidentally commissioned with the same
`nodeId`, nothing on either board notices. The panel already solves
this (`NodeIdentityGuard` + `MqttPresenceAnnouncer`, over
`PresenceTopics`), and sub-project #7 already gave the node board
everything presence detection needs: real MQTT, and a MAC-derived (not
`nodeId`-derived) client ID (`"maltbee-node4-" + mac4`) — the exact
precondition the panel's own history (CLAUDE.md's "Presence + collision
detection" section) explains is required for two colliding boards to
stay simultaneously connected long enough to observe each other's live
publish.

## Decision

Wire the panel's same two classes, unmodified, mirroring its exact
pattern. One addition node4 doesn't have yet: its 4 buttons are raw
`ArduinoDigitalInput`s fed directly into `ToggleTurnoutStation`, with no
suppression mechanism. The panel's buttons are each wrapped in a
`GatedDigitalInput` for exactly this purpose; node4 needs the same.

### New globals

```cpp
GatedDigitalInput gatedButtons[4] = {
    GatedDigitalInput(buttons[0]), GatedDigitalInput(buttons[1]),
    GatedDigitalInput(buttons[2]), GatedDigitalInput(buttons[3]),
};
```
(added right after the existing `buttons[4]` array)

```cpp
NodeIdentityGuard identityGuard(ownMac.lastFourHexDigits());
MqttPresenceAnnouncer presenceAnnouncer(mqttLink, systemClock, runningConfig.nodeId, ownMac.lastFourHexDigits());
```
(added right after `mqttLink`, before `turnoutCommandPort`/`feedbackSource`
— the announcer holds `mqttLink` by reference)

`ToggleTurnoutStation stations[4]` changes to take `gatedButtons[i]`
instead of `buttons[i]`.

### `setup()` changes

Inside the existing `if (configValid) { wifiLink.begin(...);
mqttLink.begin(...); }` block, add:

```cpp
mqttLink.subscribe(PresenceTopics::macTopic(runningConfig.nodeId),
                    [](const std::string& payload) { identityGuard.onMacObserved(payload); });
```

### `loop()` changes

Insert between the existing `setupTrigger.update();` and `if
(setupTrigger.requested())`:

```cpp
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
```

Add `presenceAnnouncer.update(mqttLink.connected());` right after the
existing `wifiLink.poll()`/`mqttLink.poll()` block. Change the feedback
gate from `if (configValid && mqttLink.connected())` to `if (configValid
&& mqttLink.connected() && !collision)`, matching the panel exactly.

## Components touched

All reused unmodified: `NodeIdentityGuard`, `MqttPresenceAnnouncer`,
`GatedDigitalInput`, `PresenceTopics`. Only `src/esp32node4/main.cpp`
changes.

## Testing

No native test for `main.cpp` — composition root, build-verified only,
this project's established convention. Verification: `pio run
-e esp32node4` succeeds; `pio run -e esp32dev` and `pio run
-e megaatmega2560` are unaffected; `pio test -e native` stays green.

## Net effect

Once two node4 boards are accidentally commissioned with the same
`nodeId` and both reach `BootMode::Normal`, each observes the other's
MAC on `panel/<nodeId>/mac` within 30 seconds (the heartbeat interval)
and latches a collision — suppressing all 4 buttons via
`gatedButtons[i].setSuppressed(true)`, falling back to
`clearIndicator()`'s blink/unconfirmed LED state, and logging the
collision once over serial, identical to the panel's existing behavior.

## Non-goals

- The identify-overlay (`IdentifyModeTimer` + the `panel/<nodeId>/identify`
  subscription) — a separate choice from this sub-project's scope,
  deferred to whichever future sub-project adds it.
- Any change to `NodeIdentityGuard`, `MqttPresenceAnnouncer`,
  `GatedDigitalInput`, `PresenceTopics`, or `src/esp32/main.cpp` — all
  reused completely unmodified.
