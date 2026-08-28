# ESP32 Turnout Panel — NodeConfig & Bench-Serial Commissioning Design

## Status

Design approved 2026-08-28. Not yet implemented.

## Context

The ESP32 turnout panel (12 turnouts, matrix-scanned buttons, shared-GPIO LED
pairs, MQTT to JMRI) is planned in `docs/ESP32_Turnout_Panel_Implementation.md`
but has no code yet — that doc hardcodes each turnout's JMRI system name in a
compile-time `TurnoutConfig[]` table and leaves several integration questions
open (topic scheme, retained messages, JMRI feedback mode, broker setup).

A sibling project, `MaltbeeTurnoutController` (ESP32 + MQTT + JMRI, driving
Tortoise motors directly via TB6612 H-bridges — a different board, not this
panel), has already built and field-tested almost this entire problem: a
`NodeConfig`/`ConfigStore` field-configuration layer, bench-serial and
wireless-captive-portal commissioning, a resilient `MqttLink` (it found and
fixed a real bug where subscriptions silently died after a reconnect race,
and a status/command self-echo loop), and multi-node identity/collision
detection. This design ports that proven approach into this project.

### Reuse strategy

Port the *design*, adapt the *code* — write fresh classes via TDD, using this
project's existing conventions, directly informed by the sibling's working
code and the bugs it already found and fixed. Not a verbatim copy: the
sibling's ports (`DigitalInput`, `DigitalOutput`, `Clock`) are typed on value
objects (`Level`, `Instant`, `Duration`); this project's are raw primitives
(`bool`, `unsigned long`). The MQTT/config/commissioning layer sits above
that low-level I/O layer and doesn't touch it, so it can be ported without
migrating this project's existing `DigitalInput`/`DigitalOutput`/`Clock` or
the Mega/LocoNet code that depends on them.

Consistent with that, new ESP32-only timing logic (deadlines, blink
patterns, debounce windows once needed) should reuse this project's existing
non-blocking pattern — raw `unsigned long` timestamps against the existing
`Clock` port, the same style `PulsingLocoNetTransport` already uses — rather
than introducing the sibling's `Instant`/`Duration`/`Level` value-object
system as a second, parallel way of doing the same thing.

### Sub-project decomposition

The full ESP32 panel effort decomposes as follows (see conversation history
for the full comparison table); this spec covers only the first slice.

| # | Sub-project |
|---|---|
| 1 | PlatformIO `esp32dev` environment |
| **2a** | **NodeConfig + bench-serial commissioning — this spec** |
| 2b | JMRI/MQTT transport (`WiFiLink`, `MqttLink`, `TopicScheme`, `PayloadCodec`, JMRI turnout command/feedback adapters) |
| 2c | Wireless captive-portal commissioning |
| 2d | Field identification + multi-panel collision safety |
| 3 | Matrix-scan button input |
| 4 | Shared-GPIO LED-pair output |
| 5 | ESP32 hardware adapters for 3/4 |
| 6 | JMRI turnout command/feedback wiring into `TurnoutControl` |
| 7 | Composition root |
| 8 | Hardware bring-up |

2a is foundational (2b/2c depend on it), entirely native-testable, and small
enough to be a single implementation plan. Sub-project #1 (PlatformIO
environment) is folded into this slice since it's mechanical and needed to
build-check this slice's two hardware shims.

## Scope decisions (from user Q&A during brainstorming)

- **Commissioning:** both bench-serial (this spec) and wireless captive
  portal (deferred to slice 2c) are wanted for v1 — not bench-serial-only.
- **NodeConfig contents:** WiFi credentials, broker address, and a JMRI
  system name per turnout channel (1–12). GPIO pin assignments are **not**
  field-configurable — they're fixed by this panel's own PCB design, unlike
  the sibling's flexible per-channel wiring (which exists because their
  boards are built once and reused across arbitrary turnout wiring).
- **Node identity:** included now (not deferred), since this project's own
  docs already assume multiple ESP32 panels may be deployed. Node id here is
  used only for MQTT presence/collision detection and blink-to-identify
  (slice 2d) — unlike the sibling, this panel's per-channel JMRI names are
  free-form strings, not derived from node id via arithmetic.

## Architecture

Pure domain classes (`NodeConfig`, `ParsedCommand`, `CommandLineParser`,
`CommissioningSession`) live in `lib/McsCore/src/domain/`, compiled and
tested under `native` with no `Arduino.h` dependency — same rule as every
other domain class in this project. Two port interfaces (`ConfigStore`,
`UartPort`) are added to `lib/McsCore/src/ports/`. `SerialCommissioningAdapter`
is adapter-layer but still native-testable (it only depends on the new
ports, not on Arduino directly). Only `NvsConfigStore` and `EspUartPort` are
real hardware shims, `#ifdef ARDUINO`-guarded, verified by
`pio run -e esp32dev` build-check rather than a native test — the same
pattern already used for `ArduinoClock`/`ArduinoDigitalInput`.

## Components

### `NodeConfig` (domain, `lib/McsCore/src/domain/NodeConfig.h`)

```cpp
struct NodeConfig {
    int nodeId = 0;                    // 0 = unset/factory-default sentinel
    std::string wifiSsid;
    std::string wifiPassword;
    std::string brokerHost;
    int brokerPort = 1883;
    static constexpr int kChannelCount = 12;
    std::array<std::string, kChannelCount> channelJmriNames; // "" = not yet wired to JMRI

    static NodeConfig factoryDefault();
    NodeConfig withNodeId(int id) const;
    NodeConfig withWifi(std::string ssid, std::string password) const;
    NodeConfig withBroker(std::string host, int port) const;
    NodeConfig withChannelName(int channel, std::string jmriName) const; // channel is 1-12

    std::vector<std::string> validate() const; // empty = valid
};
```

Immutable value type; every mutation goes through a `with...()` method
returning a modified copy, matching this project's "explicit state changes"
principle and the sibling's proven `NodeConfig` shape.

`std::string`/`std::vector`/`std::array`, not `FixedString32`/fixed-capacity
arrays: this code only ever targets `native` and `esp32dev`, both of which
have full libstdc++. `FixedString32` exists specifically to work around the
Mega's AVR toolchain having no real STL (see `CLAUDE.md`) — that constraint
doesn't apply here, and introducing it would be needless complexity for
code that never compiles for `megaatmega2560`.

**`validate()` checks:**
- `nodeId` in 1–99 (0 is the invalid factory-default sentinel).
- `wifiSsid` non-empty.
- `brokerHost` non-empty; `brokerPort` in 1–65535.
- No two non-empty `channelJmriNames` entries are equal (duplicate JMRI
  turnout claimed by two channels).
- Does **not** require every channel to be named — a panel may be
  commissioned incrementally as turnouts are wired up. This is a deliberate
  fix for a bug the sibling project hit in production: their equivalent
  (sentinel pin `-1`) silently passed validation *and* silently drove
  nothing, discovered only via manual diagnosis. Here, `show` (below)
  explicitly marks each unconfigured channel so the gap is visible rather
  than silent.

### `ConfigStore` (port, `lib/McsCore/src/ports/ConfigStore.h`)

```cpp
class ConfigStore {
public:
    virtual ~ConfigStore() = default;
    virtual NodeConfig load() = 0;
    virtual void save(const NodeConfig&) = 0;
};
```

`NvsConfigStore` (adapter, ESP32 `Preferences`-backed) and `FakeConfigStore`
(test double, in-memory, returns `factoryDefault()` until `save()` is
called) implement it.

### `ParsedCommand` / `CommandLineParser` (domain)

```cpp
enum class CommandKind { Id, Wifi, Broker, TurnoutName, Show, Save, Reboot, Invalid };

struct ParsedCommand {
    CommandKind kind;
    int intArg = 0;             // id, or channel number (1-12) for TurnoutName
    std::string stringArg1;     // ssid / host / jmriSystemName
    std::string stringArg2;     // password
    int intArg2 = 0;            // broker port
    std::string errorMessage;   // set only when kind == Invalid
};

class CommandLineParser {
public:
    static ParsedCommand parse(const std::string& line); // pure, no I/O
};
```

**Command set:**

| Command | Effect |
|---|---|
| `id <n>` | Set node id (1–99) |
| `wifi <ssid> <password>` | Set WiFi credentials |
| `broker <host> <port>` | Set MQTT broker address |
| `turnout <n> name <jmriSystemName>` | Set channel *n*'s (1–12) JMRI system name |
| `show` | Print the current draft config; each channel explicitly marked `(unconfigured)` if empty |
| `save` | Validate and persist the draft via `ConfigStore`; on failure, list every validation error and do not save |
| `reboot` | Restart so the saved config takes effect |

Arguments are whitespace-delimited tokens (`CommandLineParser::parse()` does
no quoting/escaping). **A WiFi SSID or password containing a space isn't
representable via bench serial in this slice** — an accepted, deliberate
limitation, not an oversight: it lexes as a token count mismatch and
`CommandLineParser` returns `Invalid` with an explanatory `errorMessage`.
Slice 2c's wireless captive-portal form fields aren't whitespace-delimited
and won't share this limitation.

### `CommissioningSession` (domain)

```cpp
class CommissioningSession {
public:
    explicit CommissioningSession(ConfigStore& store);
    std::string apply(const ParsedCommand& cmd);  // response text to echo back
    bool rebootRequested() const;
private:
    ConfigStore& store_;
    NodeConfig draft_;
    bool rebootRequested_ = false;
};
```

Holds a draft `NodeConfig` (loaded from `ConfigStore` at construction),
applies parsed commands to it via the `with...()` methods, and only writes
through `ConfigStore::save()` on an explicit `save` command — never
live-applies. Nothing in this slice has a running object graph yet to
"apply live" to (that arrives in slice 2b), but this keeps the same
draft-then-explicit-save discipline the later slices will need too.

### `UartPort` (port) / `SerialCommissioningAdapter` (adapter)

```cpp
class UartPort {
public:
    virtual ~UartPort() = default;
    virtual bool available() const = 0;
    virtual char read() = 0;
    virtual void write(const std::string& text) = 0;
};

class SerialCommissioningAdapter {
public:
    SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session);
    void poll();  // non-blocking: drains available bytes, buffers into a line, dispatches on '\n'
    bool rebootRequested() const;
private:
    UartPort& uart_;
    CommissioningSession& session_;
    std::string lineBuffer_;
};
```

`EspUartPort` (adapter, wraps `Serial`, `#ifdef ARDUINO`-guarded) and
`FakeUartPort` (test double, queued input / captured output string)
implement `UartPort`.

## File layout

- `lib/McsCore/src/domain/`: `NodeConfig.h`, `ParsedCommand.h`,
  `CommandLineParser.h`, `CommissioningSession.h`
- `lib/McsCore/src/ports/`: `ConfigStore.h`, `UartPort.h`
- `lib/McsCore/src/adapters/`: `SerialCommissioningAdapter.h`,
  `NvsConfigStore.h` (guarded), `EspUartPort.h` (guarded)
- `test/support/`: `FakeConfigStore.h`, `FakeUartPort.h`
- `test/test_node_config/`, `test/test_command_line_parser/`,
  `test/test_commissioning_session/`, `test/test_serial_commissioning_adapter/`

Build order (TDD, each green before the next): `NodeConfig` →
`CommandLineParser`/`ParsedCommand` → `CommissioningSession` (with
`FakeConfigStore`) → `SerialCommissioningAdapter` (with `FakeUartPort`) →
`NvsConfigStore`/`EspUartPort` (build-check only, no native test).

## PlatformIO changes

Add `[env:esp32dev]` to `platformio.ini` now (mechanical, folds in
sub-project #1):

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_ldf_mode = deep+
build_unflags = -std=gnu++11
build_flags = -std=gnu++17
```

This lets `NvsConfigStore`/`EspUartPort` be build-check-verified via
`pio run -e esp32dev` as they're written, even with no `main.cpp`
composition root wiring them up yet (that arrives in slice 2b). No `src/`
split is needed for this slice — `test_build_src = false` already keeps
`src/main.cpp` out of native test builds, and this slice adds no code to
`src/main.cpp` at all.

## Error handling

- Malformed commands produce `ParsedCommand{kind: Invalid, errorMessage: "..."}`;
  `CommissioningSession::apply()` echoes the error text back over serial
  without touching the draft.
- `save` with a failing `validate()` echoes every validation error and does
  not persist — the operator fixes and retries `save`.
- No exceptions anywhere in this layer; `CommandLineParser::parse()` and
  `NodeConfig::validate()` are both pure functions returning data, not
  throwing.

## Testing

All-native except the two hardware shims:
- `NodeConfig`: `factoryDefault()` fails validation; each `with...()` method
  returns a modified copy without mutating the original; `validate()`
  catches each of its listed cases (bad id, empty ssid, empty host, bad
  port, duplicate channel name); partial channel configuration passes.
- `CommandLineParser`: one test per command form (valid and malformed), plus
  whitespace/argument-count edge cases.
- `CommissioningSession`: applying each command kind produces the expected
  draft change and response text; `save` persists only on valid config;
  `reboot` sets the flag; `FakeConfigStore` round-trips.
- `SerialCommissioningAdapter`: byte-at-a-time input assembles into a line
  and dispatches on `\n`; multiple commands in sequence; partial line
  (no `\n` yet) doesn't dispatch.
- `NvsConfigStore`/`EspUartPort`: build-check only via `pio run -e esp32dev`,
  no native equivalent (same convention as `ArduinoClock`).

## Out of scope for this slice

WiFi/MQTT connectivity, `src/esp32/main.cpp` composition root, wireless
captive-portal commissioning, node presence/collision detection, and
identify-blink. These are slices 2b, 2c, and 2d respectively, each to be
brainstormed and spec'd separately once this slice is implemented.
