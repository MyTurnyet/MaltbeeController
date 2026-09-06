# ESP32 Loco2MQTT Turnout Bridge (Sub-project #9a) — Design

This is sub-project **#9a**, the first of three pieces in a larger pivot:
swapping the ESP32 panel's MQTT integration from "publish/subscribe to a
JMRI instance running on a computer, over an external broker" to "publish/
subscribe directly to Loco2MQTT" — a new companion ESP32 device that runs
its own on-device MQTT broker and bridges straight to the layout's LocoNet
bus. No computer, no JMRI, no external broker required. The full contract
Loco2MQTT exposes (topics, payload words, broker limitations) was supplied
by the user as a context document during brainstorming and is treated here
as authoritative for what the device actually does today.

**Full pivot, three sub-projects:**
- **#9a (this spec):** turnout command/feedback adapters, `NodeConfig`
  addressing model, commissioning/web-form changes, mDNS broker discovery,
  firmware version display. Makes the panel work against Loco2MQTT.
- **#9b (future):** redesign presence/collision detection (#2d-a) as a
  periodic heartbeat, since it currently depends on retained MQTT messages
  and Loco2MQTT's broker ignores the retained flag entirely.
- **#9c (future):** decommission `jmri/panel_mqtt_turnout_bridge.py` and its
  docs now that nothing talks to JMRI anymore.

This is a **full replacement**, not a dual-path system — `JmriTurnoutCommandAdapter`/
`JmriFeedbackSource` and the `track/turnout/<jmriName>` topic scheme are
retired outright, not kept behind a switch.

## Scope decisions (from user Q&A during brainstorming)

- **Full replacement**, confirmed above — no switchable JMRI/Loco2MQTT mode.
- **Addressing model:** `NodeConfig`'s per-channel field changes from a JMRI
  system name (`std::string`, e.g. `"LT5"`) to a plain LocoNet turnout
  address (`int`, 1–2048, matching Loco2MQTT's contract exactly — no offset,
  no remapping). `0` is the "channel unconfigured" sentinel, replacing the
  empty-string sentinel the JMRI-name field used.
- **Broker discovery:** Loco2MQTT advertises itself via mDNS as
  `loco2mqtt.local`. The panel resolves this once at boot, before connecting;
  on success it uses the resolved IP, on failure it falls back to the
  existing manually-configured `brokerHost`/`brokerPort` fields (kept in
  commissioning as the escape hatch — the contract doc's own advice to make
  the broker address "a configurable setting, not a hardcoded constant"
  still holds for the fallback path). Re-resolution after boot (e.g. if
  Loco2MQTT's IP changes mid-session) is explicitly **out of scope** — see
  "Future enhancements" below.
- **Firmware version:** a manually-bumped semver constant, displayed on the
  setup web page, so a fleet of panels can be checked against each other for
  "did I flash the latest firmware everywhere."
- **Presence/collision detection (#2d-a) and JMRI decommissioning** are
  explicitly deferred to #9b/#9c — not touched by this spec.

## Architecture

Mirrors the existing JMRI adapter shape (`docs/superpowers/specs/2026-08-28-esp32-jmri-mqtt-transport-design.md`)
almost exactly — same send/receive split behind the same `MqttTransport`
port — just re-keyed from a JMRI name lookup to an int address, plus one new
piece (mDNS resolution) ahead of the existing `MqttLink::begin()` call.

**Send side:** `TurnoutControl` → `TurnoutCommandPort` (unchanged) →
`Loco2MqttTurnoutCommandAdapter` (renamed from `JmriTurnoutCommandAdapter`,
same shape) → `MqttTransport` (unchanged port) → `MqttLink` (unchanged
hardware shim).

**Receive side:** `MqttLink` → `Loco2MqttFeedbackSource` (renamed from
`JmriFeedbackSource`, same shape) → `main.cpp` broadcasts polled
`TurnoutFeedback` to every station's `TurnoutControl::applyFeedback()`,
unchanged.

**New: broker discovery.** A new `MdnsResolver` port, implemented by
`EspMdnsResolver` (`#ifdef ARDUINO`-guarded, wraps `ESPmDNS`) on hardware and
a `FakeMdnsResolver` in native tests. The boot-time resolve-or-fallback
policy is a small, pure, native-testable piece — not inlined into
`main.cpp` — so it can be unit tested without a real network.

### `TopicScheme` (domain, `lib/McsEsp32/src/domain/TopicScheme.h`)

Same class, new signature and topic template. The `/set` suffix on the
command topic is new — JMRI's `track/turnout/<name>` topic had no suffix,
but Loco2MQTT's command topic does.

```cpp
#pragma once

#include <string>

class TopicScheme
{
public:
    static std::string topicFor(int address)
    {
        return "loconet/turnout/" + std::to_string(address) + "/set";
    }

    static std::string stateTopicFor(int address)
    {
        return "loconet/turnout/" + std::to_string(address) + "/state";
    }
};
```

### `PayloadCodec` (domain) — unchanged

`CLOSED`/`THROWN` already matches Loco2MQTT's payload contract exactly, case
and all. No change needed.

### `NodeConfig` (domain, `lib/McsEsp32/src/domain/NodeConfig.h`/`.cpp`)

```cpp
struct NodeConfig
{
    static constexpr int kChannelCount = 12;
    static constexpr int kMinNodeId = 1;
    static constexpr int kMaxNodeId = 99;
    static constexpr int kMinBrokerPort = 1;
    static constexpr int kMaxBrokerPort = 65535;
    static constexpr int kMinTurnoutAddress = 1;
    static constexpr int kMaxTurnoutAddress = 2048;

    int nodeId = 0;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string brokerHost;
    int brokerPort = 1883;
    std::array<int, kChannelCount> channelTurnoutAddresses{};   // 0 = unconfigured

    static NodeConfig factoryDefault();

    [[nodiscard]] NodeConfig withNodeId(int id) const;
    [[nodiscard]] NodeConfig withWifi(std::string ssid, std::string password) const;
    [[nodiscard]] NodeConfig withBroker(std::string host, int port) const;
    [[nodiscard]] NodeConfig withChannelAddress(int channel, int address) const;

    [[nodiscard]] std::vector<std::string> validate() const;
};
```

`validate()` changes:
- Drops the duplicate-JMRI-name check, adds a duplicate-address check
  (skipping `0`/unconfigured entries, same as today skips empty names).
- Adds a per-channel range check: any non-zero `channelTurnoutAddresses[i]`
  outside `[kMinTurnoutAddress, kMaxTurnoutAddress]` is an error
  (`"channel N address must be between 1 and 2048"`).

### `Loco2MqttTurnoutCommandAdapter` (adapter, native-testable, renamed from `JmriTurnoutCommandAdapter`)

```cpp
#pragma once

#include <array>

#include "ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

class Loco2MqttTurnoutCommandAdapter final : public TurnoutCommandPort
{
public:
    Loco2MqttTurnoutCommandAdapter(MqttTransport& transport,
                                    const std::array<int, NodeConfig::kChannelCount>& channelTurnoutAddresses);

    void send(int address, TurnoutPosition position) override;

private:
    MqttTransport& transport_;
    const std::array<int, NodeConfig::kChannelCount>& channelTurnoutAddresses_;
};
```

`send()` still treats its `address` parameter as a 1–12 **channel** number
(unchanged contract with `TurnoutCommandPort`/`TurnoutStation` — this is the
existing channel-vs-address distinction the JMRI slice established, see that
spec's "Address vs. channel" note). It looks up
`channelTurnoutAddresses_[channel - 1]`; if that's `0` (unconfigured) or the
channel is out of `[1, 12]`, `send()` no-ops — same silent-drop posture as
before. Otherwise it publishes `TopicScheme::topicFor(turnoutAddress)` with
the encoded payload, **not retained** (Loco2MQTT's broker ignores the flag
either way, and there's no reason to set it).

### `Loco2MqttFeedbackSource` (adapter, native-testable, renamed from `JmriFeedbackSource`)

```cpp
#pragma once

#include <array>
#include <vector>

#include "ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

class Loco2MqttFeedbackSource
{
public:
    Loco2MqttFeedbackSource(MqttTransport& transport,
                             const std::array<int, NodeConfig::kChannelCount>& channelTurnoutAddresses);

    bool poll(TurnoutFeedback& outFeedback);

private:
    std::vector<TurnoutFeedback> pending_;
};
```

Same shape as before: subscribes one state topic per non-zero configured
channel at construction, each closure capturing its own channel number, a
successful `PayloadCodec::decode()` appends to `pending_`, `poll()` drains
FIFO. Loco2MQTT re-publishes every known turnout's state every 30 seconds
regardless of change (its own workaround for not honoring retained
messages) — this needs no special handling here; a repeated identical
`TurnoutFeedback` flowing into `TurnoutControl::applyFeedback()` is already
a no-op there.

### `MdnsResolver` (port, `lib/McsEsp32/src/ports/MdnsResolver.h`) — new

```cpp
#pragma once

#include <optional>
#include <string>

class MdnsResolver
{
public:
    virtual ~MdnsResolver() = default;

    // hostname is the bare mDNS name without ".local", e.g. "loco2mqtt".
    virtual std::optional<std::string> resolveHost(const std::string& hostname) = 0;
};
```

### `EspMdnsResolver` (adapter, `#ifdef ARDUINO`-guarded hardware shim) — new

Wraps `ESPmDNS`'s `MDNS.begin()` (once) + `MDNS.queryHost(hostname)`. Returns
`std::nullopt` on a failed/timed-out query (the underlying call blocks for
up to a few seconds — acceptable here since this only ever runs once, during
`setup()`, before anything time-sensitive is happening).

```cpp
#pragma once

#ifdef ARDUINO

#include <ESPmDNS.h>

#include <optional>
#include <string>

#include "ports/MdnsResolver.h"

class EspMdnsResolver final : public MdnsResolver
{
public:
    std::optional<std::string> resolveHost(const std::string& hostname) override;
};

#endif
```

### `BrokerAddressResolver` (application, `lib/McsEsp32/src/application/BrokerAddressResolver.h`/`.cpp`) — new, native-testable

The pure "mDNS first, manual fallback" policy, kept out of `main.cpp` so
it's unit-testable against a `FakeMdnsResolver`.

```cpp
#pragma once

#include <string>

#include "../ports/MdnsResolver.h"

class BrokerAddressResolver
{
public:
    explicit BrokerAddressResolver(MdnsResolver& resolver);

    // Tries mDNS first; falls back to fallbackHost if resolution fails.
    [[nodiscard]] std::string resolve(const std::string& mdnsHostname, const std::string& fallbackHost) const;

private:
    MdnsResolver& resolver_;
};
```

`resolve()` calls `resolver_.resolveHost(mdnsHostname)`; returns the
resolved value if present, otherwise `fallbackHost` unchanged (including
when `fallbackHost` itself is empty — `NodeConfig::validate()` already
requires a non-empty `brokerHost` before `save()` succeeds, so this
function doesn't need its own empty-string handling).

### `FirmwareVersion` (domain, `lib/McsEsp32/src/domain/FirmwareVersion.h`) — new

```cpp
#pragma once

// Bump by hand whenever a release worth tracking across the fleet goes out.
inline constexpr const char* kFirmwareVersion = "1.0.0";
```

`SetupFormRenderer::render()` includes this header and prints
`kFirmwareVersion` directly (e.g. `<p class='subtitle'>v1.0.0</p>` under the
existing "MaltBee Panel Setup" heading) — a compile-time constant, so no
signature change to `render()` is needed.

## Commissioning & web-form changes

- `ParsedCommand`: `CommandKind::TurnoutName` → `CommandKind::TurnoutAddress`.
  Channel stays in `intArg`; the address (previously `stringArg1`) moves to
  `intArg2`, mirroring the existing `Broker` command's `stringArg1`/`intArg2`
  split.
- `CommandLineParser`: `turnout <n> name <jmriSystemName>` →
  `turnout <n> address <address>`. Since addresses are bare integers with no
  spaces, this command no longer needs quote-handling workarounds — it goes
  through the normal `CommandLineParser::parse()` path. `WebFormCommissioningAdapter`
  drops its direct-`ParsedCommand`-construction bypass for this one command
  and calls `CommandLineParser::parse("turnout " + n + " address " + addr)`
  like every other field; the `wifi` command keeps its bypass, since
  SSIDs/passwords can still contain spaces.
- `CommissioningSession::apply()`: validates the channel range as today
  (1–12), and now also surfaces `NodeConfig::validate()`'s new 1–2048
  address-range error at `save` time; `formatShow()` prints
  `turnout N: <address or (unconfigured)>`.
- `WebFormSubmission`: `channelJmriNames` (`array<string,12>`) →
  `channelTurnoutAddresses` (`array<string,12>`) — still string-typed here
  since it's raw HTTP form input; parsed to int the same way `brokerPort`
  already is (via `CommandLineParser`'s internal `parseInt`), so non-numeric
  input becomes a normal command-parse error rather than silently
  defaulting to `0`.
- `SetupFormRenderer`: field name `t{n}_name` → `t{n}_address`, input
  `type='number'`, label "Turnout N JMRI Name" → "Turnout N Address",
  `<details>` heading "Turnout JMRI Names" → "Turnout Addresses". Blank
  still means "leave unconfigured" (blank → `0`), same UX as before.
- `CaptivePortalServer`: reads form field `t{n}_address` instead of
  `t{n}_name` — one-line change, same pattern (`webServer_.arg(...)`).

## `src/esp32/main.cpp` wiring changes

- Construct `EspMdnsResolver mdnsResolver;` and
  `BrokerAddressResolver brokerAddressResolver(mdnsResolver);` alongside the
  other globals.
- Before `mqttLink.begin(...)`, compute
  `const std::string brokerHost = brokerAddressResolver.resolve("loco2mqtt", runningConfig.brokerHost);`
  and pass that (with `runningConfig.brokerPort`, unchanged) to `begin()`.
  This resolution happens once, synchronously, during `setup()`.
- Replace `JmriTurnoutCommandAdapter`/`JmriFeedbackSource` construction with
  `Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource`, passing
  `runningConfig.channelTurnoutAddresses`.
- No other wiring changes — stations, presence/identify globals, matrix
  scanning, and LED pairs are all untouched by this slice.

## File layout

- `lib/McsEsp32/src/domain/`: `TopicScheme.h` (modified), `PayloadCodec.h`
  (unchanged), `NodeConfig.h`/`.cpp` (modified), `FirmwareVersion.h` (new)
- `lib/McsEsp32/src/ports/`: `MdnsResolver.h` (new)
- `lib/McsEsp32/src/application/`: `BrokerAddressResolver.h`/`.cpp` (new)
- `lib/McsEsp32/src/adapters/`:
  `Loco2MqttTurnoutCommandAdapter.h`/`.cpp` (renamed from
  `JmriTurnoutCommandAdapter`), `Loco2MqttFeedbackSource.h`/`.cpp` (renamed
  from `JmriFeedbackSource`), `EspMdnsResolver.h`/`.cpp` (new, guarded shim)
- `test/support/`: `FakeMdnsResolver.h` (new)
- `test/test_topic_scheme/`, `test/test_node_config/`,
  `test/test_command_line_parser/`, `test/test_commissioning_session/`,
  `test/test_web_form_commissioning_adapter/`, `test/test_setup_form_renderer/`
  (all modified in place); `test/test_loco2mqtt_turnout_command_adapter/`,
  `test/test_loco2mqtt_feedback_source/` (renamed from the `jmri_*`
  equivalents); `test/test_broker_address_resolver/` (new)

## Error handling

- `Loco2MqttTurnoutCommandAdapter::send()` silently no-ops for an
  unconfigured channel or out-of-range channel number — same posture as
  today.
- `Loco2MqttFeedbackSource` silently drops any payload `PayloadCodec::decode`
  doesn't recognize — unchanged.
- `EspMdnsResolver::resolveHost()` returns `std::nullopt` on any failure
  (not found, timeout) — no exceptions, no crash; `BrokerAddressResolver`
  treats that identically to "mDNS unavailable" and falls back.
- `NodeConfig::validate()`'s new address-range/duplicate checks surface as
  ordinary validation errors through the existing `formatErrors()`/web-form
  error-response path — no new error-handling shape needed.

## Testing

All-native except `EspMdnsResolver` (build-check only, same convention as
`MqttLink`/`WiFiLink`):

- `TopicScheme`: `topicFor()`/`stateTopicFor()` produce the expected
  `loconet/turnout/<address>/set` and `.../state` strings for a handful of
  addresses.
- `NodeConfig`: `withChannelAddress()`; `validate()` catches an out-of-range
  address (0 is valid/unconfigured, 1 and 2048 are valid boundaries, -1 and
  2049 are errors) and a duplicate address across two channels.
- `Loco2MqttTurnoutCommandAdapter`: `send()` for a configured channel
  publishes the right topic/payload to a `FakeMqttTransport` (not retained);
  no-op for an unconfigured channel (address `0`) and for out-of-range
  channel numbers (0, 13).
- `Loco2MqttFeedbackSource`: construction subscribes only configured
  channels' state topics; a subscribed handler firing with a valid payload
  makes the next `poll()` return the right `TurnoutFeedback`; an invalid
  payload produces nothing; FIFO draining, same as the JMRI-era suite.
- `CommandLineParser`: `turnout <n> address <addr>` parses correctly;
  non-numeric address is a parse error; wrong token count is a usage error.
- `CommissioningSession`: `TurnoutAddress` command applies correctly,
  channel-range and (at `save`) address-range errors surface as expected.
- `WebFormCommissioningAdapter`/`SetupFormRenderer`: field name/round-trip
  tests updated for `t{n}_address`; a `SetupFormRenderer` test asserts the
  rendered page contains `kFirmwareVersion`'s current value.
- `BrokerAddressResolver`: `resolve()` returns the mDNS result when the fake
  resolver finds one; falls back to the given host when the fake resolver
  returns `std::nullopt`.

## Out of scope for this slice

- Presence/collision-detection heartbeat redesign (#9b) — `MqttPresenceAnnouncer`/
  `NodeIdentityGuard`/retained-message reliance untouched here, still
  non-functional against Loco2MQTT's broker until #9b lands.
- Decommissioning `jmri/panel_mqtt_turnout_bridge.py` and its docs (#9c).
- Command-burst throttling for Loco2MQTT's 32-entry drop-oldest inbound
  queue — not relevant until a routes/multi-turnout-burst feature exists
  (Milestone 12 territory, not this pivot).

## Future enhancements (explicitly not this slice)

- **Periodic mDNS re-resolution.** Today `BrokerAddressResolver` runs once
  at boot; if Loco2MQTT's IP changes mid-session (its own reboot picking up
  a new DHCP lease, for example), a panel that already resolved the old IP
  keeps retrying a dead address until manually power-cycled. A DHCP
  reservation for the Loco2MQTT board (which the contract doc already
  recommends) makes this rare in practice, but re-resolving on a slow
  timer — or after N consecutive failed `MqttLink` reconnect attempts —
  would close the gap. Deferred because doing this without blocking
  `loop()`'s button/LED servicing needs its own non-blocking design (an
  `MDNS.queryHost()` call can block for several seconds).
