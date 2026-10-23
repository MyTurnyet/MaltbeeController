# 4-Turnout Node: Loco2MQTT Channel-Count Templating (Sub-project #6) — Design

This is sub-project **#6** of the 4-turnout node board effort — the first
sub-project of a new "Loco2MQTT wiring" phase, which itself started as
one roadmap item ("real WiFi/MQTT/Loco2MQTT wiring on the node board")
flagged as the biggest unblocking gap once sub-project #5 (boot-mode LED
indicator) completed.

## Scope narrowed from the original roadmap item

"Loco2MQTT wiring" actually bundles four things with different risk
profiles, following the same scope-narrowing pattern already used to
split sub-projects #2/#3/#4/#5:

1. **Templating** `Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource`
   for `N = 4` — pure code, fully native-testable, no hardware/WiFi
   dependency at all.
2. **Wiring** real `WiFiLink`/`MqttLink`/`BrokerAddressResolver`/
   `EspMdnsResolver` into `src/esp32node4/main.cpp`, using the templated
   adapters from (1), replacing `NullTurnoutCommandPort` with the real
   command port/feedback source — the first real WiFi/MQTT code on the
   node board, composition-root-only, no native test possible.
3. **Presence/collision detection** (`MqttPresenceAnnouncer`/
   `NodeIdentityGuard`) for the node board — unclear if even needed yet;
   depends on undecided fleet-deployment plans (how many node4 boards
   will ever share one broker, whether `nodeId` collisions are a real
   risk the same way they are for the 12-channel panel).
4. **Identify-overlay wiring** (`IdentifyModeTimer` + the MQTT
   `panel/<nodeId>/identify` subscription + a `setIdentifying()` loop) —
   explicitly deferred to this point by sub-project #5's design spec
   ("the identify-overlay moves to whichever future sub-project adds real
   Loco2MQTT wiring to the node board").

Discussed with Paige: **this sub-project is (1) alone.** It's isolated,
fully testable, and unblocks everything else without touching `main.cpp`
at all. (2), (3), and (4) are each their own future sub-project, with (2)
needing its own scope discussion once we get there.

## Problem

`Loco2MqttTurnoutCommandAdapter` and `Loco2MqttFeedbackSource`
(`lib/McsEsp32/src/adapters/`) are hard-bound to
`std::array<int, NodeConfig::kChannelCount>` (`NodeConfig::kChannelCount`
is `12`, the panel's channel count) in both their headers and `.cpp`
files — a gap sub-project #1's own forward-looking note already flagged:
*"`Loco2MqttTurnoutCommandAdapter` and `Loco2MqttFeedbackSource` ... were
not part of this templating effort and still hard-bind to
`std::array<int, NodeConfig::kChannelCount>` ... both classes need the
same `Basic<X><N>` treatment ... before the node board can talk to
Loco2MQTT."* `NodeConfig4::channelTurnoutAddresses` is a
`std::array<int, 4>` and will not bind to a reference parameter expecting
`std::array<int, 12>`, so neither class can be constructed with the node
board's 4-channel config today.

## Decision

Both classes get the identical `Basic<X><N>` treatment sub-project #1
already used for the two prior classes that have a `.cpp`
(`NvsConfigStore`, `CaptivePortalServer`): the class becomes a
`template <int N>` whose method bodies live in the `.cpp` as template
definitions, with explicit instantiations for both `N` values at the
bottom of the `.cpp`, plus two aliases at the bottom of the header
preserving every existing name exactly (zero behavior change for N=12,
the same guarantee every one of sub-project #1's 8 classes already
carries).

### `Loco2MqttTurnoutCommandAdapter` → `BasicLoco2MqttTurnoutCommandAdapter<N>`

`lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.h`:

```cpp
#pragma once

#include <array>

#include "../ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

template <int N>
class BasicLoco2MqttTurnoutCommandAdapter final : public TurnoutCommandPort
{
public:
    BasicLoco2MqttTurnoutCommandAdapter(MqttTransport& transport, const std::array<int, N>& channelTurnoutAddresses);

    void send(int address, TurnoutPosition position) override;

private:
    MqttTransport& transport_;
    const std::array<int, N>& channelTurnoutAddresses_;
};

using Loco2MqttTurnoutCommandAdapter = BasicLoco2MqttTurnoutCommandAdapter<12>;
using Loco2MqttTurnoutCommandAdapter4 = BasicLoco2MqttTurnoutCommandAdapter<4>;
```

Note the `domain/NodeConfig.h` include is dropped entirely — once `N` is
a template parameter, this class has no remaining dependency on
`NodeConfig` at all; it never used anything from that header except the
`kChannelCount` constant that's now `N`.

`lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.cpp`:

```cpp
#include "Loco2MqttTurnoutCommandAdapter.h"

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

template <int N>
BasicLoco2MqttTurnoutCommandAdapter<N>::BasicLoco2MqttTurnoutCommandAdapter(
    MqttTransport& transport, const std::array<int, N>& channelTurnoutAddresses)
    : transport_(transport), channelTurnoutAddresses_(channelTurnoutAddresses)
{
}

template <int N>
void BasicLoco2MqttTurnoutCommandAdapter<N>::send(const int address, const TurnoutPosition position)
{
    if (address < 1 || address > N)
    {
        return;
    }

    const int turnoutAddress = channelTurnoutAddresses_[address - 1];
    if (turnoutAddress == 0)
    {
        return;
    }

    transport_.publish(TopicScheme::topicFor(turnoutAddress), PayloadCodec::encode(position), false);
}

template class BasicLoco2MqttTurnoutCommandAdapter<12>;
template class BasicLoco2MqttTurnoutCommandAdapter<4>;
```

The only logic change is `address > NodeConfig::kChannelCount` becoming
`address > N` — otherwise byte-for-byte identical.

### `Loco2MqttFeedbackSource` → `BasicLoco2MqttFeedbackSource<N>`

Same treatment. `lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.h`:

```cpp
#pragma once

#include <array>
#include <deque>

#include "../ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

template <int N>
class BasicLoco2MqttFeedbackSource
{
public:
    BasicLoco2MqttFeedbackSource(MqttTransport& transport, const std::array<int, N>& channelTurnoutAddresses);

    bool poll(TurnoutFeedback& outFeedback);

private:
    std::deque<TurnoutFeedback> pending_;
};

using Loco2MqttFeedbackSource = BasicLoco2MqttFeedbackSource<12>;
using Loco2MqttFeedbackSource4 = BasicLoco2MqttFeedbackSource<4>;
```

`lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.cpp`: the constructor's
`for (int i = 0; i < NodeConfig::kChannelCount; ++i)` loop becomes
`for (int i = 0; i < N; ++i)`; everything else (the subscription lambda,
`poll()`) is unchanged. Explicit instantiations
`template class BasicLoco2MqttFeedbackSource<12>;` / `<4>;` at the bottom,
same as the command adapter.

## Testing

- The 3 existing suites — `test/test_loco2mqtt_turnout_command_adapter`,
  `test/test_loco2mqtt_feedback_source`, `test/test_loco2mqtt_turnout_wiring`
  — need **no edits**. They reference the unqualified class names
  (`Loco2MqttTurnoutCommandAdapter`, `Loco2MqttFeedbackSource`) and
  `std::array<int, NodeConfig::kChannelCount>`, both of which still
  resolve to N=12 via the alias and `NodeConfig::kChannelCount == 12`
  respectively.
- Two new suites, mirroring their N=12 counterparts' test cases 1:1 with
  `std::array<int, 4>` and the `...4` aliases:
  - `test/test_loco2mqtt_turnout_command_adapter4` — mirrors all 5 cases
    from `test_loco2mqtt_turnout_command_adapter`.
  - `test/test_loco2mqtt_feedback_source4` — mirrors all 7 cases from
    `test_loco2mqtt_feedback_source`.
- No N=4 mirror of `test_loco2mqtt_turnout_wiring` — its cases (button
  press → publish, real state-topic confirmation, heartbeat-repeat
  idempotence) don't exercise anything that differs between N=12 and
  N=4; the cross-class logic under test is identical regardless of `N`.
  A 4-channel copy would be pure duplication with zero new coverage.

## Non-goals

- No `src/esp32node4/main.cpp` changes — `NullTurnoutCommandPort` stays
  wired there. Real WiFi/MQTT/Loco2MQTT composition-root wiring is
  sub-project #7.
- No presence/collision detection, no identify-overlay — both remain
  blocked on #7 landing first.
- No change to `MqttTransport`, `TopicScheme`, `PayloadCodec`,
  `WiFiLink`, `MqttLink`, `BrokerAddressResolver`, `EspMdnsResolver`, or
  `src/esp32/main.cpp` — all reused completely unmodified.
