# 4-Turnout Node: Loco2MQTT WiFi/MQTT Wiring (Sub-project #7) — Design

This is sub-project **#7** of the 4-turnout node board effort. It depends
on sub-project #6
(`docs/superpowers/specs/2026-10-22-esp32-node4-loco2mqtt-channel-count-design.md`,
`Loco2MqttTurnoutCommandAdapter4`/`Loco2MqttFeedbackSource4`).

## Scope narrowed from the original roadmap

The roadmap item this sub-project was planned against was "real
WiFi/MQTT/Loco2MQTT wiring on the node board." Sub-project #6's own
design spec already pre-narrowed this when it deferred the piece: *"No
presence/collision detection, no identify-overlay — both remain blocked
on #7 landing first."* **Decision, confirmed with Paige: this sub-project
is exactly that — real `WiFiLink`/`MqttLink`/`BrokerAddressResolver`/
`EspMdnsResolver` wiring into `src/esp32node4/main.cpp`, nothing else.**
Presence/collision detection and the identify-overlay both remain
deferred to whichever sub-project comes after #7.

## Problem

`src/esp32node4/main.cpp` has no WiFi/MQTT code at all —
`ToggleTurnoutStation`'s command port is `NullTurnoutCommandPort` (a
no-op) and there is no feedback source. Per sub-project #4's documented
"Known, accepted gap," a button press on the node board currently
produces zero observable effect. Sub-project #6 made
`Loco2MqttTurnoutCommandAdapter4`/`Loco2MqttFeedbackSource4`
constructible at N=4; this sub-project wires them in for real,
mirroring the panel's already-working reference implementation
(`src/esp32/main.cpp`) with everything presence/identify-related
stripped out.

## Decision

Mirror the panel's exact wiring shape. Two choices needed a naming
convention, since nothing established precedent specifically for the
node board:

- **MQTT client ID / own mDNS hostname.** The panel uses
  `"maltbee-esp32-" + mac4` / `"maltbee-panel-" + mac4` — MAC-derived,
  not `nodeId`-derived, so two colliding boards never fight over a
  broker client ID even before any collision-detection logic exists
  (the panel's own `CLAUDE.md` history explains this choice in detail —
  see "Presence + collision detection" there). The node board gets the
  same MAC-derived pattern with its own prefix:
  `"maltbee-node4-" + mac4` for both the MQTT client ID and the
  `EspMdnsResolver`'s self-advertised hostname.
- **The LWT (will topic/message) is not optional.** `MqttLink`'s
  constructor requires `willTopic`/`willMessage` unconditionally — there
  is no "skip it" option without modifying `MqttLink` itself, which is
  out of scope. Pass `PresenceTopics::statusTopic(nodeId)` / `"offline"`,
  the same values the panel passes. Without the active
  `MqttPresenceAnnouncer` (excluded from this sub-project), nothing ever
  publishes `"online"` on that topic — it sits dormant until a future
  presence sub-project adds the announcer, reusing the identical topic
  name rather than inventing a new one later.

### New globals

Added where `NullTurnoutCommandPort turnoutCommandPort;` currently sits
(that line is deleted):

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

Two new constants join the existing constants block, same values the
panel already uses: `RETRY_INTERVAL_MS = 5000`,
`BROKER_RESOLVE_INTERVAL_MS = 60000`. Variable names stay unsuffixed —
matching this file's existing convention of suffixing only *type* names
with `4` (e.g. `NodeConfig4 runningConfig`, not `runningConfig4`).

### `setup()` changes

Right after the existing `bootModeIndicator.show(bootMode);` call and
`BootMode::WirelessSetup` check, add the panel's exact gate:

```cpp
if (configValid)
{
    wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
    mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
}
```

No `mqttLink.subscribe(...)` calls — those exist on the panel only for
presence (`macTopic`) and identify (`identifyTopic`), both excluded
here.

### `loop()` changes

Right after the existing `setupTrigger.requested()` block and before the
existing `ledStation.update()` loop, add:

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

The panel's equivalent block also ANDs `!collision` into the feedback
gate — the node board has no collision concept yet, so the gate here is
just `configValid && mqttLink.connected()`.

## Components touched

All reused unmodified: `WiFiLink`, `MqttLink`, `BrokerAddressResolver`,
`EspMdnsResolver`, `Loco2MqttTurnoutCommandAdapter4`,
`Loco2MqttFeedbackSource4`, `PresenceTopics`, `ToggleTurnoutStation`
(already applies feedback and sends commands correctly — confirmed by
reading `ToggleTurnoutControl.cpp` during sub-project #4). Only
`src/esp32node4/main.cpp` changes.

## Testing

No native test for `main.cpp` — composition root, build-verified only,
this project's established convention for every prior `main.cpp`
change. Verification: `pio run -e esp32node4` succeeds; `pio run
-e esp32dev` and `pio run -e megaatmega2560` are unaffected; `pio test
-e native` stays green (nothing here is native-testable, but it's the
final sanity check).

## Net effect

Once commissioned with real WiFi/broker credentials and 4 LocoNet
turnout addresses, a button press on the node board now genuinely
publishes a Loco2MQTT command, and a real state-topic confirmation now
genuinely updates the turnout and its LED — closing sub-project #4's
"zero observable effect" gap for the first time. `clearIndicator()`
still reverts every station's LED to blink/unconfirmed whenever
`configValid` is false or MQTT is disconnected, mirroring the panel's
existing behavior exactly (minus the collision term).

## Non-goals

- Presence/collision detection (`MqttPresenceAnnouncer`/
  `NodeIdentityGuard`) — deferred to a future sub-project.
- Identify-overlay (`IdentifyModeTimer` + the `panel/<nodeId>/identify`
  subscription) — deferred to the same future sub-project, or whichever
  one adds presence detection first.
- Any new commissioning fields — `NodeConfig4` already carries
  `wifiSsid`/`wifiPassword`/`brokerHost`/`brokerPort`/
  `channelTurnoutAddresses`, all already settable via existing serial
  and web-form commissioning.
- Any change to `WiFiLink`, `MqttLink`, `BrokerAddressResolver`,
  `EspMdnsResolver`, `Loco2MqttTurnoutCommandAdapter`,
  `Loco2MqttFeedbackSource`, `PresenceTopics`, `ToggleTurnoutStation`, or
  `src/esp32/main.cpp` — all reused completely unmodified.
