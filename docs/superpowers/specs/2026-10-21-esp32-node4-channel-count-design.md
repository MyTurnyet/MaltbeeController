# 4-Turnout Node: Build Target and Channel Count — Design

## Status

Design approved 2026-10-21. Not yet implemented. This is sub-project #1 of
the 4-turnout node board firmware effort; see
`docs/pcb/turnout-node-4ch-pcb-design.md` for the hardware spec (§7 lists
the full firmware impact list this effort works through, §11 lists open
hardware verification items not yet resolved).

## Context

`docs/pcb/turnout-node-4ch-pcb-design.md` specifies a new ESP32 board
family: 4 turnouts per board instead of 12, each board its own MQTT client
with its own `nodeId`, direct-GPIO buttons instead of a matrix, and
independent red/green GPIOs per turnout instead of the panel's shared-GPIO
pair. This is a second, permanent build target alongside the existing
12-channel panel (`src/esp32/main.cpp`, `[env:esp32dev]`) — not a
replacement. The 12-channel panel's behavior, tests, and build must not
change; existing 12-channel code must not be deleted, renamed, or
repurposed.

`NodeConfig::kChannelCount` is a compile-time `12` baked into a fixed
`std::array<int, 12>`, read by `NodeConfig.cpp`, `CommissioningSession.cpp`,
`WebFormCommissioningAdapter.cpp`, and `SetupFormRenderer.h`. `src/esp32/main.cpp`'s
own `TURNOUT_CONFIGS[12]` table and station arrays are **not** part of this
problem — they live only in that composition-root file, and the node gets
its own, so there is no sharing concern there. The actual coupling is
confined to the commissioning chain: `NodeConfig`, `ConfigStore`,
`CommissioningSession`, `WebFormSubmission`, `WebFormCommissioningAdapter`,
`SerialCommissioningAdapter`, `NvsConfigStore`, `CaptivePortalServer`, plus
`SetupFormRenderer`'s `render()`.

`CLAUDE.md` notes that PlatformIO compiles `McsEsp32` as **one shared
library per native test target** — every `test/test_*` binary links
against the same compiled `McsEsp32`. A single global channel-count
constant (a `#define` or build flag) cannot coexist as both `12` and `4` in
that one native test build. The two channel counts must be genuinely
distinct C++ types, not different values of the same symbol.

## Goal

Add a second PlatformIO environment and composition root for the
4-turnout node, with its own channel count, while leaving the 12-channel
panel's environment, composition root, and all 44 existing test suites
byte-for-byte unchanged. Share the commissioning logic between the two
variants rather than duplicating it.

## Design

### Template boundary and naming

The 8 classes listed above become templates parameterized on channel count
`N`, renamed internally to `Basic<X><N>`, with the **existing** name
preserved as a type alias bound to `N=12`:

```cpp
template <int N> struct BasicNodeConfig { static constexpr int kChannelCount = N; /* ... */ };
using NodeConfig  = BasicNodeConfig<12>;  // existing symbol — every current caller/test is untouched
using NodeConfig4 = BasicNodeConfig<4>;   // new node variant
```

The same `Basic<X><N>` / `using X = Basic<X><12>` / `using X4 = Basic<X><4>`
pattern applies to:

| Existing name (stays = N=12 alias) | New N=4 alias | Template class |
|---|---|---|
| `NodeConfig` | `NodeConfig4` | `BasicNodeConfig<N>` |
| `ConfigStore` | `ConfigStore4` | `BasicConfigStore<N>` |
| `CommissioningSession` | `CommissioningSession4` | `BasicCommissioningSession<N>` |
| `WebFormSubmission` | `WebFormSubmission4` | `BasicWebFormSubmission<N>` |
| `WebFormCommissioningAdapter` | `WebFormCommissioningAdapter4` | `BasicWebFormCommissioningAdapter<N>` |
| `SerialCommissioningAdapter` | `SerialCommissioningAdapter4` | `BasicSerialCommissioningAdapter<N>` |
| `NvsConfigStore` | `NvsConfigStore4` | `BasicNvsConfigStore<N>` |
| `CaptivePortalServer` | `CaptivePortalServer4` | `BasicCaptivePortalServer<N>` |

`SetupFormRenderer` stays a non-template class; its static `render()` method
becomes a function template over `N`, taking `const BasicWebFormSubmission<N>&`.
Rendering 4 channel fields instead of 12 falls out of the existing loop body
once `N` flows through — no separate rendering logic is needed.

`CommandLineParser` and `ParsedCommand` are unchanged: neither references
channel count today, confirmed by reading both files.

Every existing file outside this boundary (`ButtonSetupModeTrigger`,
`MatrixScanner`, `LedPairDriver`, `LedPairStation`, `ToggleTurnoutStation`,
`GatedDigitalInput`, `PresenceTopics`, `NodeIdentityGuard`,
`MqttPresenceAnnouncer`, `IdentifyModeTimer`, `BrokerAddressResolver`,
`WiFiLink`, `MqttLink`, `Loco2MqttTurnoutCommandAdapter`,
`Loco2MqttFeedbackSource`, `TopicScheme`, `PayloadCodec`, and everything in
`src/esp32/main.cpp`) is untouched by this sub-project — none of it depends
on channel count.

`NvsConfigStore4`'s NVS namespace and key names stay the literal same
strings as `NvsConfigStore` (`"mcsnode"`, `"nodeId"`, `"ch0"`...`"ch3"`,
etc.) — safe because a 4-channel node and a 12-channel panel are always
different physical devices running different firmware images, never
sharing an NVS partition. This does not touch the 12-channel build's NVS
keys/namespace, satisfying the hard constraint on that point.

### PlatformIO environment and composition root

A new environment, mirroring `[env:esp32dev]`, pointed at a new source
directory:

```ini
[env:esp32node4]
build_unflags = -std=gnu++11
build_flags = -std=gnu++17
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_ldf_mode = deep+
lib_deps =
    knolleary/PubSubClient@^2.8
    McsCore
    McsEsp32
lib_ignore = McsLoconet
build_src_filter = -<*> +<esp32node4/*>
```

`board = esp32dev` is the PlatformIO board ID for the physical ELEGOO
devkit — the same hardware the panel uses — and is unrelated to the
`[env:esp32dev]` environment *name*; there is no collision. `default_envs`
stays `megaatmega2560`. The existing `[env:esp32dev]` and
`[env:megaatmega2560]` environments are untouched. `[env:native]`'s
`lib_deps` already pulls in the whole `McsEsp32` library, so no change is
needed there — new `test/test_*4` directories are picked up automatically.

New composition root: `src/esp32node4/main.cpp`. For this sub-project it is
a **minimal placeholder** — it wires only what exists today for the N=4
instantiation: `NvsConfigStore4`, `CommissioningSession4`,
`SerialCommissioningAdapter4`, `WebFormCommissioningAdapter4`,
`CaptivePortalServer4`, boot-mode selection (reusing `BootMode`/
`BootModeSelector`, which are channel-count-agnostic), and serial
commissioning. It does not yet wire turnout stations, LEDs, or buttons —
those don't have 4-channel-ready types until sub-projects #2-#4. The board
will build, boot, and be commissionable over serial/wireless, but will not
yet drive any turnout hardware. This keeps #1 focused on proving the
channel-count/build-target plumbing works end to end; later sub-projects
grow this same file rather than reworking it.

### Testing

All 44 existing test suites keep using the bare names (`NodeConfig`,
`CommissioningSession`, `WebFormSubmission`, etc.) completely unchanged —
this is the proof that N=12 behavior is unaffected, since every existing
assertion still runs against the exact same generated code.

New sibling test directories cover the N=4 instantiation:
`test_node_config4`, `test_commissioning_session4`,
`test_web_form_commissioning_adapter4`, and (if warranted once
`SetupFormRenderer`'s template signature is written) a focused addition to
`test_setup_form_renderer` for the N=4 field count. These do not re-prove
every behavior the N=12 suites already cover generically (it is the same
template code) — they focus on what is plausibly N-sensitive: channel
count is 4 not 12, channel 5 is out of range where channel 13 used to be,
array/loop bounds, and (for the web form) that exactly 4 channel fields
render.

Written test-first, per the project's TDD rule: for each of the 8 classes,
the existing test file is first run unchanged against the new template
form to confirm nothing broke, then the N=4 test file is written before the
`*4` alias is exercised.

## Verification

- `pio test -e native` — all 44 existing suites pass unchanged, plus new
  suites for the N=4 instantiations.
- `pio run -e esp32dev` — builds with no behavior change (12-channel panel).
- `pio run -e esp32node4` — builds (new environment, placeholder
  `main.cpp`).
- `pio run -e megaatmega2560` — unaffected, builds as today.

## Out of scope

Turnout stations, independent-output LED indicator, direct-GPIO buttons,
composite setup trigger, status/activity LEDs, and the 4-channel
commissioning web form's actual field rendering beyond what
`SetupFormRenderer`'s template signature requires — all later
sub-projects. The `esp32-turnout-node-rev-a/` KiCad reconciliation and
every item in spec §11 (hardware verification: current draw, 30-pin kit
row spacing, 3.3V rail under load, USB back-feed behavior, RJ45 part
selection) are hardware tasks, not firmware, and are out of scope for this
whole effort's firmware sub-projects.
