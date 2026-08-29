# ESP32 JMRI/MQTT Transport (Slice 2b) — Design

This is sub-project **2b** in the ESP32 panel decomposition (see
`docs/superpowers/specs/2026-08-28-esp32-node-config-commissioning-design.md`'s
sub-project table): `WiFiLink`, `MqttLink`, `TopicScheme`, `PayloadCodec`,
and the JMRI turnout command/feedback adapters. It depends on 2a
(`NodeConfig`, already complete), and its own turnout command/feedback
adapters are what 2b's table entry calls out — but *wiring* those adapters
into `TurnoutStation`/`TurnoutControl` instances inside a running
`src/esp32/main.cpp` stays out of scope, deferred to sub-projects #6 (JMRI
turnout command/feedback wiring into `TurnoutControl`) and #7 (composition
root), matching the precedent 2a set by leaving `NvsConfigStore`/`EspUartPort`
unwired.

## Prep: two deferred cleanup items from 2a's final review

Both were explicitly flagged in 2a's final whole-branch review as in-scope
for "the next slice that touches this code" — done first, before 2b's own
work, since they touch files 2b's testing will otherwise need to route
around:

- Move `CommissioningSession` from `lib/McsEsp32/src/domain/` to
  `lib/McsEsp32/src/application/`. It wires a `ConfigStore&` port directly,
  which is application-layer work per this project's own `CLAUDE.md`
  layering description, not domain work. Update its `#include` in
  `SerialCommissioningAdapter.h`/`.cpp` and in `test/test_commissioning_session/`
  accordingly (relative includes stay `../application/CommissioningSession.h`
  from `adapters/`, unchanged pattern from the existing `../domain/...`
  form).
- Cap `SerialCommissioningAdapter`'s `lineBuffer_`. It currently grows
  unboundedly on a never-terminated line (harmless today since nothing
  constructs a real `EspUartPort`-backed instance yet, but this slice adds
  transport code that moves the project closer to that). Add a
  `kMaxLineLength = 128` constant (comfortably longer than any real
  commissioning command line) and discard/reset the buffer if a line
  exceeds it without a `\n`, rather than growing forever.

## Scope decisions (from user Q&A during brainstorming)

- **Protocol reuse:** the sibling project `../MaltbeeTurnoutController`
  already runs this exact JMRI MQTT integration in production. This slice
  reuses its topic pattern, payload words (`CLOSED`/`THROWN`), and
  retained-publish behavior **verbatim** — the only change is re-keying the
  per-turnout topic by the free-form JMRI system name already stored in
  `NodeConfig.channelJmriNames[]`, instead of the sibling's
  `nodeId * 100 + channel` numeric id scheme (which doesn't apply here —
  this project's channel identity is a JMRI name string per the 2a spec's
  scope decision, not derived arithmetic).
- **Address vs. channel:** this project's existing
  `TurnoutCommandPort::send(int address, TurnoutPosition)` and
  `Turnout::address()` are unchanged. For JMRI/MQTT, that same `int` is
  simply the channel number (1–12) rather than a DCC address — the new
  adapters do the channel→JMRI-name translation internally, exactly as
  `MrrwaLocoNetTurnoutAdapter` translates address→LocoNet packet today. This
  keeps `TurnoutCommandPort`, `Turnout`, `TurnoutStation`, and
  `TurnoutControl` completely unchanged; only new adapters are added beneath
  the existing port.

## Architecture

Mirrors the existing LocoNet send/receive split in `lib/McsLoconet`, adapted
to MQTT's shape (a single publish/subscribe client rather than separate
send/receive hardware).

**Send side:** `TurnoutControl` → `TurnoutCommandPort` (existing, unchanged)
→ `JmriTurnoutCommandAdapter` (new, native-testable translation, parallels
`MrrwaLocoNetTurnoutAdapter`) → `MqttTransport` (new port) → `MqttLink`
(new, `#ifdef ARDUINO`-guarded hardware shim, parallels
`MrrwaLocoNetSwitchDriver`).

**Receive side:** `MqttLink` (guarded shim) → `JmriFeedbackSource` (new,
native-testable) → `main.cpp` broadcasts polled `TurnoutFeedback` to every
station's `TurnoutControl::applyFeedback()`, exactly as the LocoNet receive
path does today. Unlike LocoNet, there's no raw wire-format decode step
distinct from the payload string, so one class combines what
`MrrwaLocoNetFeedbackSource` + `LocoNetFeedbackDecoder` do separately for
LocoNet — splitting further would add a seam with nothing on either side of
it.

Both new adapters resolve a channel number to a JMRI name via
`NodeConfig.channelJmriNames[channel - 1]`:
- `JmriTurnoutCommandAdapter` does this lookup at `send()` time, since (like
  `MrrwaLocoNetTurnoutAdapter`) one shared instance is expected to serve all
  stations, disambiguated per-call by channel.
- `JmriFeedbackSource` does this lookup once, at construction, subscribing
  one MQTT topic per *configured* channel (empty `channelJmriNames[i]`
  entries are skipped — unconfigured channels never subscribe). Each
  subscription's callback closure already captures its own channel number,
  so incoming messages need no reverse name→channel lookup — they decode
  the payload and emit `TurnoutFeedback{channel, position}` directly.

### `TopicScheme` (domain, `lib/McsEsp32/src/domain/TopicScheme.h`)

Trimmed from the sibling's version: no `parse()`, since nothing here needs
to recover a channel from a topic string (see above).

```cpp
#pragma once

#include <string>

class TopicScheme
{
public:
    static std::string topicFor(const std::string& jmriName)
    {
        return "track/turnout/" + jmriName;
    }
};
```

### `PayloadCodec` (domain, `lib/McsEsp32/src/domain/PayloadCodec.h`)

Same wire words as the sibling (`CLOSED`/`THROWN`), adapted to this
project's `TurnoutPosition::Closed`/`Thrown` enum values (the sibling uses
`TurnoutPosition::closed()`/`thrown()` static methods — different enum
shape, same two states).

```cpp
#pragma once

#include <optional>
#include <string>

#include "domain/Turnout.h"

class PayloadCodec
{
public:
    static std::string encode(TurnoutPosition position)
    {
        return position == TurnoutPosition::Closed ? "CLOSED" : "THROWN";
    }

    static std::optional<TurnoutPosition> decode(const std::string& payload)
    {
        if (payload == "CLOSED") return TurnoutPosition::Closed;
        if (payload == "THROWN") return TurnoutPosition::Thrown;
        return std::nullopt;
    }
};
```

(`#include "domain/Turnout.h"` is a rooted cross-library include into
`McsCore`, following the convention already used by `McsLoconet`.)

### `MqttTransport` (port, `lib/McsEsp32/src/ports/MqttTransport.h`)

```cpp
#pragma once

#include <functional>
#include <string>

class MqttTransport
{
public:
    virtual ~MqttTransport() = default;

    virtual void publish(const std::string& topic, const std::string& payload, bool retained) = 0;
    virtual void subscribe(const std::string& topic, std::function<void(const std::string&)> handler) = 0;
};
```

### `JmriTurnoutCommandAdapter` (adapter, native-testable)

```cpp
#pragma once

#include <array>
#include <string>

#include "ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

class JmriTurnoutCommandAdapter final : public TurnoutCommandPort
{
public:
    JmriTurnoutCommandAdapter(MqttTransport& transport, const std::array<std::string, 12>& channelJmriNames);

    void send(int address, TurnoutPosition position) override;

private:
    MqttTransport& transport_;
    const std::array<std::string, 12>& channelJmriNames_;
};
```

`send()` treats `address` as a 1–12 channel number. If
`channelJmriNames_[address - 1]` is empty (channel not configured) or
`address` is out of `[1, 12]`, `send()` is a no-op — same "silently drop
what can't be resolved" posture as the LocoNet path has for addresses no
station claims.

### `JmriFeedbackSource` (adapter, native-testable)

```cpp
#pragma once

#include <array>
#include <string>
#include <vector>

#include "ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

class JmriFeedbackSource
{
public:
    JmriFeedbackSource(MqttTransport& transport, const std::array<std::string, 12>& channelJmriNames);

    bool poll(TurnoutFeedback& outFeedback);

private:
    std::vector<TurnoutFeedback> pending_;
};
```

The constructor subscribes one topic per non-empty `channelJmriNames[i]`
(channel `i + 1`). Each subscription's handler decodes the payload via
`PayloadCodec::decode`; a successful decode appends
`TurnoutFeedback{i + 1, *position}` to `pending_`. `poll()` pops and returns
the oldest pending entry (FIFO), returning `false` when empty — same
poll-until-empty-per-tick contract `LocoNetFeedbackSource::poll()` already
has.

### `WiFiLink` (adapter, `#ifdef ARDUINO`-guarded hardware shim)

Ported from the sibling, adapted to this project's `Clock` port shape (raw
`unsigned long` milliseconds via `nowMilliseconds()`, not the sibling's
`Instant`/`Duration` value types — this project has no such types and
doesn't need them here).

```cpp
#pragma once

#ifdef ARDUINO

#include <WiFi.h>

#include <string>

#include "ports/Clock.h"

class WiFiLink
{
public:
    WiFiLink(Clock& clock, unsigned long retryIntervalMs);

    void begin(const std::string& ssid, const std::string& password);
    void poll();
    bool connected() const;

private:
    void connect();

    Clock& clock_;
    unsigned long retryIntervalMs_;
    unsigned long lastAttemptMs_ = 0;
    std::string ssid_;
    std::string password_;
};

#endif
```

### `MqttLink` (adapter, `#ifdef ARDUINO`-guarded hardware shim)

Implements `MqttTransport`. Ported from the sibling's `MqttLink` +
`MqttTopicRouter` combined (the router's job — dispatching an incoming
`(topic, payload)` to the right registered handler — folds directly into
`MqttTransport::subscribe`'s contract, so a separate router class isn't
needed here).

```cpp
#pragma once

#ifdef ARDUINO

#include <WiFiClient.h>
#include <PubSubClient.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ports/Clock.h"
#include "ports/MqttTransport.h"

class MqttLink final : public MqttTransport
{
public:
    MqttLink(Clock& clock, unsigned long retryIntervalMs, std::string clientId,
              std::string willTopic, std::string willMessage);

    void begin(const std::string& host, int port);
    void poll();
    bool connected();

    void publish(const std::string& topic, const std::string& payload, bool retained) override;
    void subscribe(const std::string& topic, std::function<void(const std::string&)> handler) override;

private:
    void connect();

    Clock& clock_;
    unsigned long retryIntervalMs_;
    std::string clientId_;
    std::string willTopic_;
    std::string willMessage_;
    WiFiClient wifiClient_;
    PubSubClient client_;
    unsigned long lastAttemptMs_ = 0;
    std::vector<std::pair<std::string, std::function<void(const std::string&)>>> handlers_;
};

#endif
```

Same reconnect-replays-all-subscriptions behavior as the sibling's
`MqttLink::connect()` (PubSubClient forgets subscriptions across a dropped
session).

## File layout

- `lib/McsEsp32/src/domain/`: `TopicScheme.h`, `PayloadCodec.h`
- `lib/McsEsp32/src/ports/`: `MqttTransport.h`
- `lib/McsEsp32/src/adapters/`: `JmriTurnoutCommandAdapter.h`/`.cpp`,
  `JmriFeedbackSource.h`/`.cpp` (native-testable), `WiFiLink.h`/`.cpp`,
  `MqttLink.h`/`.cpp` (guarded shims — `.h`+`.cpp` split, matching
  `EspUartPort`/`NvsConfigStore`)
- `test/support/`: `FakeMqttTransport.h`
- `test/test_topic_scheme/`, `test/test_payload_codec/`,
  `test/test_jmri_turnout_command_adapter/`, `test/test_jmri_feedback_source/`

Prep-task moves: `lib/McsEsp32/src/domain/CommissioningSession.h`/`.cpp` →
`lib/McsEsp32/src/application/CommissioningSession.h`/`.cpp` (via `git mv`).

## Error handling

- `JmriTurnoutCommandAdapter::send()` silently no-ops for an unconfigured or
  out-of-range channel — same posture as LocoNet addresses no station
  claims.
- `JmriFeedbackSource` silently drops any payload `PayloadCodec::decode`
  doesn't recognize — same posture as `LocoNetFeedbackDecoder` today.
- No exceptions anywhere in this layer; `PayloadCodec::decode` returns
  `std::optional`, not throwing.
- `SerialCommissioningAdapter`'s new line-length cap discards the
  in-progress buffer and continues reading (doesn't crash, doesn't grow
  unboundedly) — an operator typing a too-long line just needs to retype it.

## Testing

All-native except `WiFiLink`/`MqttLink`:
- `TopicScheme`: `topicFor()` produces the expected prefixed string for a
  handful of names, including edge cases like an empty name.
- `PayloadCodec`: `encode()` for both positions; `decode()` for both valid
  words and at least one unrecognized payload.
- `JmriTurnoutCommandAdapter`: `send()` for a configured channel publishes
  the right topic/payload/retained flag to a `FakeMqttTransport`; a no-op
  for an unconfigured channel and for out-of-range addresses (0, 13).
- `JmriFeedbackSource`: construction subscribes only the configured
  channels' topics on a `FakeMqttTransport`; invoking a subscribed handler
  with a valid payload makes the next `poll()` return the right
  `TurnoutFeedback`; an invalid payload produces nothing; multiple queued
  messages drain in FIFO order via repeated `poll()` calls returning
  `false` once empty.
- `CommissioningSession` (moved, not behavior-changed): existing test suite
  moves with it, `#include` path updated, otherwise unchanged.
- `SerialCommissioningAdapter`: existing tests plus one new case — a line
  exceeding the cap without `\n` is discarded rather than growing forever,
  and a subsequent well-formed line still dispatches correctly.
- `WiFiLink`/`MqttLink`: build-check only via `pio run -e esp32dev`, same
  convention as `NvsConfigStore`/`EspUartPort`.

## Out of scope for this slice

Wiring `JmriTurnoutCommandAdapter`/`JmriFeedbackSource`/`WiFiLink`/`MqttLink`
into `src/esp32/main.cpp`, constructing real `TurnoutStation`s against them,
subscribing at boot, node presence/collision detection (slice 2d), wireless
captive-portal commissioning (slice 2c), and anything involving matrix-scan
button input or shared-GPIO LED output (slices 3–5).
