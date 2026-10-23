# Loco2MQTT Channel-Count Templating Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Template `Loco2MqttTurnoutCommandAdapter` and `Loco2MqttFeedbackSource` for `N = 4`, using the exact `Basic<X><N>` pattern sub-project #1 already established, so the node board's 4-channel config can eventually be wired to real Loco2MQTT classes.

**Architecture:** Each class becomes `template <int N> class Basic<X><N>` with method bodies as template definitions in its existing `.cpp`, explicit instantiations for `N=12` and `N=4` at the bottom of that `.cpp`, and two aliases at the bottom of its header preserving every existing name exactly — the identical shape `NvsConfigStore`/`CaptivePortalServer` already use.

**Tech Stack:** C++17 (`-std=gnu++17`), PlatformIO, Catch2 (native tests).

## Global Constraints

- Both classes must keep their existing `.cpp` (method bodies as template definitions there, not inlined in the header) — matching `NvsConfigStore`'s/`CaptivePortalServer`'s established shape, not `NodeConfig`'s header-only shape (which only applies to classes that had no `.cpp` to begin with).
- Explicit instantiations go at the very bottom of each `.cpp`: `template class Basic<X><12>;` then `template class Basic<X><4>;`, in that order (12 before 4), matching `NvsConfigStore.cpp`'s existing order.
- Aliases go at the very bottom of each header: `using X = Basic<X><12>;` then `using X4 = Basic<X><4>;`, in that order.
- Both classes drop their `#include "../domain/NodeConfig.h"` entirely once `N` is a template parameter — neither class uses anything else from that header.
- The command adapter's range check becomes `address > N` (was `address > NodeConfig::kChannelCount`); the feedback source's constructor loop becomes `for (int i = 0; i < N; ++i)` (was `for (int i = 0; i < NodeConfig::kChannelCount; ++i)`). No other logic changes — everything else is a byte-for-byte carry-over.
- The 3 existing test suites (`test_loco2mqtt_turnout_command_adapter`, `test_loco2mqtt_feedback_source`, `test_loco2mqtt_turnout_wiring`) must need zero edits — they use the unqualified class names and `NodeConfig::kChannelCount`-sized arrays, both of which still resolve to N=12 after this change.
- No `src/esp32node4/main.cpp` changes in this plan at all. No new fakes — `test/support/FakeMqttTransport.h` already exists and is reused unmodified.
- Run `pio test -e native` via the Bash tool, not PowerShell, per `CLAUDE.md` (PowerShell has produced false ERRORED/crash results on this machine).
- ACN commit notation: each task is new behavior-adding code fully covered by matching tests → `F` intention. Risk: `^` only if the diff is ≤ 8 LoC of production code for F/B — these diffs will exceed that (each `.cpp` body is template-ized plus 5-7 new test cases), so use `!` unless the test coverage is so direct and complete that `^` is still justified; judge this yourself against the actual diff size once written, same as every prior sub-project in this effort.

---

### Task 1: Template `Loco2MqttTurnoutCommandAdapter` + its N=4 test suite

**Files:**
- Modify: `lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.h`
- Modify: `lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.cpp`
- Create: `test/test_loco2mqtt_turnout_command_adapter4/test_main.cpp`

**Interfaces:**
- Consumes: `MqttTransport` port (`lib/McsEsp32/src/ports/MqttTransport.h` — `virtual void publish(const std::string& topic, const std::string& payload, bool retained) = 0;`), `TurnoutCommandPort` (`lib/McsCore/src/ports/TurnoutCommandPort.h` — `virtual void send(int address, TurnoutPosition position) = 0;`), `TopicScheme::topicFor(int)`, `PayloadCodec::encode(TurnoutPosition)`, `test/support/FakeMqttTransport.h`.
- Produces: `Loco2MqttTurnoutCommandAdapter4` (alias for `BasicLoco2MqttTurnoutCommandAdapter<4>`) — constructor `BasicLoco2MqttTurnoutCommandAdapter(MqttTransport& transport, const std::array<int, 4>& channelTurnoutAddresses)`, method `void send(int address, TurnoutPosition position) override;`. A future sub-project (#7) will construct this with `N=4` and `NodeConfig4`'s `channelTurnoutAddresses` in `src/esp32node4/main.cpp`.

The current full content of `lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.h` is:

```cpp
#pragma once

#include <array>

#include "../domain/NodeConfig.h"
#include "../ports/MqttTransport.h"
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

The current full content of `lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.cpp` is:

```cpp
#include "Loco2MqttTurnoutCommandAdapter.h"

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

Loco2MqttTurnoutCommandAdapter::Loco2MqttTurnoutCommandAdapter(
    MqttTransport& transport, const std::array<int, NodeConfig::kChannelCount>& channelTurnoutAddresses)
    : transport_(transport), channelTurnoutAddresses_(channelTurnoutAddresses)
{
}

void Loco2MqttTurnoutCommandAdapter::send(const int address, const TurnoutPosition position)
{
    if (address < 1 || address > NodeConfig::kChannelCount)
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
```

The current full content of `test/test_loco2mqtt_turnout_command_adapter/test_main.cpp` (do not modify this file — it is reproduced here only so you can see exactly what Step 1's new N=4 file must mirror):

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/Loco2MqttTurnoutCommandAdapter.h"
#include "domain/NodeConfig.h"
#include "support/FakeMqttTransport.h"

namespace
{
    std::array<int, NodeConfig::kChannelCount> addressesWithChannel(int channel, int address)
    {
        std::array<int, NodeConfig::kChannelCount> addresses{};
        addresses[channel - 1] = address;
        return addresses;
    }
}

TEST_CASE("send publishes the expected topic, payload, and retained flag")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttTurnoutCommandAdapter adapter(transport, addresses);

    adapter.send(1, TurnoutPosition::Closed);

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "loconet/turnout/5/set");
    REQUIRE(transport.published[0].payload == "CLOSED");
    REQUIRE_FALSE(transport.published[0].retained);
}

TEST_CASE("send encodes Thrown correctly")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttTurnoutCommandAdapter adapter(transport, addresses);

    adapter.send(1, TurnoutPosition::Thrown);

    REQUIRE(transport.published[0].payload == "THROWN");
}

TEST_CASE("send resolves the correct channel out of several configured")
{
    FakeMqttTransport transport;
    auto addresses = addressesWithChannel(1, 5);
    addresses[4] = 17;
    Loco2MqttTurnoutCommandAdapter adapter(transport, addresses);

    adapter.send(5, TurnoutPosition::Closed);

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "loconet/turnout/17/set");
}

TEST_CASE("send is a no-op for an unconfigured channel")
{
    FakeMqttTransport transport;
    const std::array<int, NodeConfig::kChannelCount> addresses{};
    Loco2MqttTurnoutCommandAdapter adapter(transport, addresses);

    adapter.send(1, TurnoutPosition::Closed);

    REQUIRE(transport.published.empty());
}

TEST_CASE("send is a no-op for an out-of-range channel")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttTurnoutCommandAdapter adapter(transport, addresses);

    adapter.send(0, TurnoutPosition::Closed);
    adapter.send(NodeConfig::kChannelCount + 1, TurnoutPosition::Closed);

    REQUIRE(transport.published.empty());
}
```

- [ ] **Step 1: Write the failing N=4 test file**

Create `test/test_loco2mqtt_turnout_command_adapter4/test_main.cpp` with this exact content — a 1:1 mirror of the file above, using `std::array<int, 4>` directly (no `NodeConfig`/`NodeConfig4` dependency needed, since the test can just hardcode `4`) and the `Loco2MqttTurnoutCommandAdapter4` alias. Two test cases needed index adjustments to stay within a 4-element array's valid indices (0-3): the third case now uses channel 4 (index 3) instead of channel 5 (index 4), and the fifth case's out-of-range probe is `5` (`N + 1`) instead of `NodeConfig::kChannelCount + 1`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/Loco2MqttTurnoutCommandAdapter.h"
#include "support/FakeMqttTransport.h"

namespace
{
    std::array<int, 4> addressesWithChannel(int channel, int address)
    {
        std::array<int, 4> addresses{};
        addresses[channel - 1] = address;
        return addresses;
    }
}

TEST_CASE("send publishes the expected topic, payload, and retained flag (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttTurnoutCommandAdapter4 adapter(transport, addresses);

    adapter.send(1, TurnoutPosition::Closed);

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "loconet/turnout/5/set");
    REQUIRE(transport.published[0].payload == "CLOSED");
    REQUIRE_FALSE(transport.published[0].retained);
}

TEST_CASE("send encodes Thrown correctly (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttTurnoutCommandAdapter4 adapter(transport, addresses);

    adapter.send(1, TurnoutPosition::Thrown);

    REQUIRE(transport.published[0].payload == "THROWN");
}

TEST_CASE("send resolves the correct channel out of several configured (N=4)")
{
    FakeMqttTransport transport;
    auto addresses = addressesWithChannel(1, 5);
    addresses[3] = 17;
    Loco2MqttTurnoutCommandAdapter4 adapter(transport, addresses);

    adapter.send(4, TurnoutPosition::Closed);

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "loconet/turnout/17/set");
}

TEST_CASE("send is a no-op for an unconfigured channel (N=4)")
{
    FakeMqttTransport transport;
    const std::array<int, 4> addresses{};
    Loco2MqttTurnoutCommandAdapter4 adapter(transport, addresses);

    adapter.send(1, TurnoutPosition::Closed);

    REQUIRE(transport.published.empty());
}

TEST_CASE("send is a no-op for an out-of-range channel (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttTurnoutCommandAdapter4 adapter(transport, addresses);

    adapter.send(0, TurnoutPosition::Closed);
    adapter.send(5, TurnoutPosition::Closed);

    REQUIRE(transport.published.empty());
}
```

- [ ] **Step 2: Run the test to verify it fails to compile**

Run (from the repo root, via Bash, not PowerShell):
```bash
pio test -e native -f test_loco2mqtt_turnout_command_adapter4
```
Expected: build failure — `Loco2MqttTurnoutCommandAdapter4` is not declared (the alias doesn't exist yet).

- [ ] **Step 3: Template the header**

Replace the full content of `lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.h` with:

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

- [ ] **Step 4: Template the .cpp**

Replace the full content of `lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.cpp` with:

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

- [ ] **Step 5: Run the new test to verify it passes**

Run:
```bash
pio test -e native -f test_loco2mqtt_turnout_command_adapter4
```
Expected: `5 test cases: 5 succeeded`.

- [ ] **Step 6: Run the existing N=12 test suite to confirm zero regressions**

Run:
```bash
pio test -e native -f test_loco2mqtt_turnout_command_adapter
```
Expected: `5 test cases: 5 succeeded` — unchanged from before this task, proving the alias preserves exact N=12 behavior.

- [ ] **Step 7: Run the full native suite**

Run:
```bash
pio test -e native
```
Expected: every existing suite still passes, plus the new `test_loco2mqtt_turnout_command_adapter4` suite (53 suites total, up from 52).

- [ ] **Step 8: Commit**

Stage exactly these three files:
```bash
git add lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.h lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.cpp test/test_loco2mqtt_turnout_command_adapter4/test_main.cpp
```

Commit using Arlo's Commit Notation. This is new behavior (a new N=4 instantiation becomes constructible) fully covered by 5 new matching test cases, and the diff exceeds 8 LoC of production code, so use `!` risk, `F` intention — unless, once you see the actual diff, you judge the test coverage direct and complete enough to justify `^` instead (the Global Constraints section above leaves this judgment to you, same as every prior sub-project):

```
! F Template Loco2MqttTurnoutCommandAdapter for N=4

Mirrors sub-project #1's Basic<X><N> pattern (NvsConfigStore's shape,
since this class also has a .cpp). Zero behavior change for the existing
N=12 panel code - verified by the unchanged existing test suite still
passing. 5 new N=4 test cases mirror the N=12 suite 1:1.
```

---

### Task 2: Template `Loco2MqttFeedbackSource` + its N=4 test suite

**Files:**
- Modify: `lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.h`
- Modify: `lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.cpp`
- Create: `test/test_loco2mqtt_feedback_source4/test_main.cpp`

**Interfaces:**
- Consumes: `MqttTransport` port (`virtual void subscribe(const std::string& topic, std::function<void(const std::string&)> handler) = 0;`), `TopicScheme::stateTopicFor(int)`, `PayloadCodec::decode(const std::string&)` (returns `std::optional<TurnoutPosition>`), `TurnoutFeedback` (`lib/McsCore/src/ports/TurnoutCommandPort.h` — `struct TurnoutFeedback { int address; TurnoutPosition position; };`), `test/support/FakeMqttTransport.h`.
- Produces: `Loco2MqttFeedbackSource4` (alias for `BasicLoco2MqttFeedbackSource<4>`) — constructor `BasicLoco2MqttFeedbackSource(MqttTransport& transport, const std::array<int, 4>& channelTurnoutAddresses)`, method `bool poll(TurnoutFeedback& outFeedback);`. A future sub-project (#7) will construct this with `N=4` in `src/esp32node4/main.cpp`.

The current full content of `lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.h` is:

```cpp
#pragma once

#include <array>
#include <deque>

#include "../domain/NodeConfig.h"
#include "../ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

class Loco2MqttFeedbackSource
{
public:
    Loco2MqttFeedbackSource(MqttTransport& transport,
                             const std::array<int, NodeConfig::kChannelCount>& channelTurnoutAddresses);

    bool poll(TurnoutFeedback& outFeedback);

private:
    std::deque<TurnoutFeedback> pending_;
};
```

The current full content of `lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.cpp` is:

```cpp
#include "Loco2MqttFeedbackSource.h"

#include <optional>

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

Loco2MqttFeedbackSource::Loco2MqttFeedbackSource(
    MqttTransport& transport, const std::array<int, NodeConfig::kChannelCount>& channelTurnoutAddresses)
{
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const int turnoutAddress = channelTurnoutAddresses[i];
        if (turnoutAddress == 0)
        {
            continue;
        }

        const int channel = i + 1;
        transport.subscribe(TopicScheme::stateTopicFor(turnoutAddress), [this, channel](const std::string& payload) {
            const std::optional<TurnoutPosition> position = PayloadCodec::decode(payload);
            if (!position.has_value())
            {
                return;
            }
            pending_.push_back(TurnoutFeedback{channel, *position});
        });
    }
}

bool Loco2MqttFeedbackSource::poll(TurnoutFeedback& outFeedback)
{
    if (pending_.empty())
    {
        return false;
    }

    outFeedback = pending_.front();
    pending_.pop_front();
    return true;
}
```

The current full content of `test/test_loco2mqtt_feedback_source/test_main.cpp` (do not modify this file — reproduced only so you can see exactly what Step 1's new N=4 file must mirror):

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/Loco2MqttFeedbackSource.h"
#include "domain/NodeConfig.h"
#include "support/FakeMqttTransport.h"

namespace
{
    std::array<int, NodeConfig::kChannelCount> addressesWithChannel(int channel, int address)
    {
        std::array<int, NodeConfig::kChannelCount> addresses{};
        addresses[channel - 1] = address;
        return addresses;
    }
}

TEST_CASE("construction subscribes only the configured channels' state topics")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);

    Loco2MqttFeedbackSource source(transport, addresses);

    REQUIRE(transport.subscribedTopics.size() == 1);
    REQUIRE(transport.subscribedTopics[0] == "loconet/turnout/5/state");
}

TEST_CASE("construction subscribes each configured channel's state topic among several")
{
    FakeMqttTransport transport;
    auto addresses = addressesWithChannel(1, 5);
    addresses[4] = 17;

    Loco2MqttFeedbackSource source(transport, addresses);

    REQUIRE(transport.subscribedTopics.size() == 2);
}

TEST_CASE("a valid incoming payload on the state topic becomes a pollable TurnoutFeedback")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(3, 9);
    Loco2MqttFeedbackSource source(transport, addresses);

    transport.deliver("loconet/turnout/9/state", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE(feedback.address == 3);
    REQUIRE(feedback.position == TurnoutPosition::Thrown);
}

TEST_CASE("an unrecognized payload on the state topic produces nothing")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource source(transport, addresses);

    transport.deliver("loconet/turnout/5/state", "GARBAGE");

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("a message on the command topic is not picked up as feedback")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource source(transport, addresses);

    transport.deliver("loconet/turnout/5/set", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("poll returns false once the queue is drained")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource source(transport, addresses);

    transport.deliver("loconet/turnout/5/state", "CLOSED");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("multiple queued messages drain in FIFO order")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource source(transport, addresses);

    transport.deliver("loconet/turnout/5/state", "CLOSED");
    transport.deliver("loconet/turnout/5/state", "THROWN");

    TurnoutFeedback first{};
    TurnoutFeedback second{};
    REQUIRE(source.poll(first));
    REQUIRE(source.poll(second));
    REQUIRE(first.position == TurnoutPosition::Closed);
    REQUIRE(second.position == TurnoutPosition::Thrown);
}
```

- [ ] **Step 1: Write the failing N=4 test file**

Create `test/test_loco2mqtt_feedback_source4/test_main.cpp` with this exact content — a 1:1 mirror, using `std::array<int, 4>` and the `Loco2MqttFeedbackSource4` alias. One case needed an index adjustment to stay within a 4-element array's valid indices (0-3): the second case now uses channel 4 (index 3) instead of channel 5 (index 4):

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/Loco2MqttFeedbackSource.h"
#include "support/FakeMqttTransport.h"

namespace
{
    std::array<int, 4> addressesWithChannel(int channel, int address)
    {
        std::array<int, 4> addresses{};
        addresses[channel - 1] = address;
        return addresses;
    }
}

TEST_CASE("construction subscribes only the configured channels' state topics (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);

    Loco2MqttFeedbackSource4 source(transport, addresses);

    REQUIRE(transport.subscribedTopics.size() == 1);
    REQUIRE(transport.subscribedTopics[0] == "loconet/turnout/5/state");
}

TEST_CASE("construction subscribes each configured channel's state topic among several (N=4)")
{
    FakeMqttTransport transport;
    auto addresses = addressesWithChannel(1, 5);
    addresses[3] = 17;

    Loco2MqttFeedbackSource4 source(transport, addresses);

    REQUIRE(transport.subscribedTopics.size() == 2);
}

TEST_CASE("a valid incoming payload on the state topic becomes a pollable TurnoutFeedback (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(3, 9);
    Loco2MqttFeedbackSource4 source(transport, addresses);

    transport.deliver("loconet/turnout/9/state", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE(feedback.address == 3);
    REQUIRE(feedback.position == TurnoutPosition::Thrown);
}

TEST_CASE("an unrecognized payload on the state topic produces nothing (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource4 source(transport, addresses);

    transport.deliver("loconet/turnout/5/state", "GARBAGE");

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("a message on the command topic is not picked up as feedback (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource4 source(transport, addresses);

    transport.deliver("loconet/turnout/5/set", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("poll returns false once the queue is drained (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource4 source(transport, addresses);

    transport.deliver("loconet/turnout/5/state", "CLOSED");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("multiple queued messages drain in FIFO order (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource4 source(transport, addresses);

    transport.deliver("loconet/turnout/5/state", "CLOSED");
    transport.deliver("loconet/turnout/5/state", "THROWN");

    TurnoutFeedback first{};
    TurnoutFeedback second{};
    REQUIRE(source.poll(first));
    REQUIRE(source.poll(second));
    REQUIRE(first.position == TurnoutPosition::Closed);
    REQUIRE(second.position == TurnoutPosition::Thrown);
}
```

- [ ] **Step 2: Run the test to verify it fails to compile**

Run:
```bash
pio test -e native -f test_loco2mqtt_feedback_source4
```
Expected: build failure — `Loco2MqttFeedbackSource4` is not declared.

- [ ] **Step 3: Template the header**

Replace the full content of `lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.h` with:

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

- [ ] **Step 4: Template the .cpp**

Replace the full content of `lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.cpp` with:

```cpp
#include "Loco2MqttFeedbackSource.h"

#include <optional>

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

template <int N>
BasicLoco2MqttFeedbackSource<N>::BasicLoco2MqttFeedbackSource(
    MqttTransport& transport, const std::array<int, N>& channelTurnoutAddresses)
{
    for (int i = 0; i < N; ++i)
    {
        const int turnoutAddress = channelTurnoutAddresses[i];
        if (turnoutAddress == 0)
        {
            continue;
        }

        const int channel = i + 1;
        transport.subscribe(TopicScheme::stateTopicFor(turnoutAddress), [this, channel](const std::string& payload) {
            const std::optional<TurnoutPosition> position = PayloadCodec::decode(payload);
            if (!position.has_value())
            {
                return;
            }
            pending_.push_back(TurnoutFeedback{channel, *position});
        });
    }
}

template <int N>
bool BasicLoco2MqttFeedbackSource<N>::poll(TurnoutFeedback& outFeedback)
{
    if (pending_.empty())
    {
        return false;
    }

    outFeedback = pending_.front();
    pending_.pop_front();
    return true;
}

template class BasicLoco2MqttFeedbackSource<12>;
template class BasicLoco2MqttFeedbackSource<4>;
```

- [ ] **Step 5: Run the new test to verify it passes**

Run:
```bash
pio test -e native -f test_loco2mqtt_feedback_source4
```
Expected: `7 test cases: 7 succeeded`.

- [ ] **Step 6: Run the existing N=12 test suite to confirm zero regressions**

Run:
```bash
pio test -e native -f test_loco2mqtt_feedback_source
```
Expected: `7 test cases: 7 succeeded` — unchanged from before this task.

- [ ] **Step 7: Run the wiring/integration suite to confirm zero regressions**

Run:
```bash
pio test -e native -f test_loco2mqtt_turnout_wiring
```
Expected: `2 test cases: 2 succeeded` — unchanged (this suite constructs both `Loco2MqttTurnoutCommandAdapter` and `Loco2MqttFeedbackSource` at N=12 and must still work identically after both classes are templated).

- [ ] **Step 8: Run the full native suite**

Run:
```bash
pio test -e native
```
Expected: every suite passes, including both new N=4 suites from Tasks 1 and 2 (54 suites total, up from 52).

- [ ] **Step 9: Verify the hardware build targets are unaffected**

Run:
```bash
pio run -e esp32dev
pio run -e megaatmega2560
pio run -e esp32node4
```
Expected: all three still build successfully — `esp32dev` is the panel that actually uses these two classes at N=12, so this is the one build that proves the templating didn't break the real hardware target; the other two don't reference these classes at all but are checked anyway per this project's standing convention of verifying all build targets after any `lib/` change.

- [ ] **Step 10: Commit**

Stage exactly these three files:
```bash
git add lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.h lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.cpp test/test_loco2mqtt_feedback_source4/test_main.cpp
```

Commit using Arlo's Commit Notation, same reasoning as Task 1's commit:

```
! F Template Loco2MqttFeedbackSource for N=4

Mirrors sub-project #1's Basic<X><N> pattern. Zero behavior change for
the existing N=12 panel code - verified by the unchanged existing suite
and the cross-class wiring suite both still passing. 7 new N=4 test
cases mirror the N=12 suite 1:1.
```

---

### Task 3: Update `CLAUDE.md` and the PCB design doc

**Files:**
- Modify: `CLAUDE.md`
- Modify: `docs/pcb/turnout-node-4ch-pcb-design.md`

**Interfaces:**
- Consumes: the finished `BasicLoco2MqttTurnoutCommandAdapter<N>`/`BasicLoco2MqttFeedbackSource<N>` classes from Tasks 1-2. No code interfaces — documentation only.
- Produces: nothing downstream.

- [ ] **Step 1: Add a new section to `CLAUDE.md`**

Find the "### Boot-mode LED indicator (sub-project #5)" section in `CLAUDE.md` (it should be the section immediately before "### Loco2MQTT turnout bridge (sub-project #9a)" — if a different section now sits between them, insert after whichever is last before #9a's heading). Insert a new section immediately after it:

```markdown
### Loco2MQTT channel-count templating (sub-project #6)

Closes the gap sub-project #1's own forward-looking note flagged:
`Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource`
(`lib/McsEsp32/src/adapters/`) were hard-bound to
`std::array<int, NodeConfig::kChannelCount>` and could not be constructed
with `NodeConfig4`'s 4-element `channelTurnoutAddresses` array. This is
the first of what the roadmap originally called "real WiFi/MQTT/Loco2MQTT
wiring on the node board" — narrowed, with Paige, to templating alone:
isolated, fully testable, no hardware dependency, no `main.cpp` changes.
Wiring these classes into `src/esp32node4/main.cpp` alongside real
`WiFiLink`/`MqttLink`/`BrokerAddressResolver`/`EspMdnsResolver` is its own
future sub-project (#7), as is presence/collision detection and the
identify-overlay (both still blocked on #7 landing first).

Both classes got the identical `Basic<X><N>` treatment sub-project #1
used for `NvsConfigStore`/`CaptivePortalServer` (the two prior classes
with a `.cpp`): `BasicLoco2MqttTurnoutCommandAdapter<N>` and
`BasicLoco2MqttFeedbackSource<N>`, method bodies as template definitions
in each `.cpp`, explicit instantiations for `N=12`/`N=4` at the bottom of
each `.cpp`, and `using X = Basic...<12>;`/`using X4 = Basic...<4>;`
aliases at the bottom of each header — zero behavior change for the
existing N=12 panel code. Both classes dropped their
`domain/NodeConfig.h` include entirely, since `N` as a template parameter
removed their only dependency on it. The command adapter's range check
became `address > N` (was `address > NodeConfig::kChannelCount`); the
feedback source's constructor loop became `for (int i = 0; i < N; ++i)`
(was `... < NodeConfig::kChannelCount; ...`).

**Test coverage added:** `test/test_loco2mqtt_turnout_command_adapter4`
(5 cases, mirroring `test_loco2mqtt_turnout_command_adapter`'s 5 cases)
and `test/test_loco2mqtt_feedback_source4` (7 cases, mirroring
`test_loco2mqtt_feedback_source`'s 7 cases) — 52 → 54 suites. No N=4
mirror of `test_loco2mqtt_turnout_wiring` was added: its cases exercise
cross-class logic that doesn't differ between N=12 and N=4, so a copy
would add no coverage.
```

- [ ] **Step 2: Update the rooted-include tally if this sub-project changed it**

Both `.h` files still use relative includes (`../ports/MqttTransport.h`, `ports/TurnoutCommandPort.h`, etc. — unchanged by this plan) — this sub-project adds and removes no rooted includes. Confirm by running:

```bash
grep -rn '#include "ports/\|#include "domain/\|#include "adapters/\|#include "application/"' lib/McsEsp32/src lib/McsLoconet/src --include='*.h' --include='*.cpp' | grep -v 'EspMdnsResolver.h.*ports/MdnsResolver.h'
```

If the count differs from 44 (the figure `CLAUDE.md` currently states), update the "Include convention" paragraph's stated counts to match — do not guess or increment blindly. If unchanged at 44, make no edit here.

- [ ] **Step 3: Update `docs/pcb/turnout-node-4ch-pcb-design.md`'s §7 status line**

Find the status line near the top of §7 (search for "Status as of sub-project #4"):

```markdown
**Status as of sub-project #4 (2026-10-22):** items 1 (sub-project #1), 2
(sub-project #4), 3 (sub-projects #2 and #4 together) and 6 (sub-project #3)
are done; items 4-5 are not yet started.
```

Replace it with:

```markdown
**Status as of sub-project #6 (2026-10-22):** items 1 (sub-project #1), 2
(sub-project #4), 3 (sub-projects #2 and #4 together), 4's boot/setup half
(sub-project #5), and 6 (sub-project #3) are done; item 4's identify half
and item 5 remain open, both blocked on real WiFi/MQTT wiring landing on
the node board (sub-project #6 templated the Loco2MQTT classes that
wiring will use; the wiring itself is sub-project #7).
```

- [ ] **Step 4: Verify the native suite is still green**

Run:
```bash
pio test -e native
```
Expected: unaffected (this task touches only documentation), all 54 suites pass.

- [ ] **Step 5: Commit**

Stage exactly these two files:
```bash
git add CLAUDE.md docs/pcb/turnout-node-4ch-pcb-design.md
```

Commit using Arlo's Commit Notation (`. d` — documentation only, zero behavior change):

```
. d Document sub-project #6 Loco2MQTT channel-count templating

Adds the CLAUDE.md section and updates the PCB design doc's §7 status
line to reflect item 4's boot/setup half being done and item 5/identify
still blocked on sub-project #7's WiFi/MQTT wiring.
```

---

## Self-Review Notes

**Spec coverage:** every element of the approved design spec
(`docs/superpowers/specs/2026-10-22-esp32-node4-loco2mqtt-channel-count-design.md`)
maps to a task — the `Basic<X><N>` treatment for both classes (Tasks 1-2),
the dropped `NodeConfig.h` include and updated range checks (Tasks 1-2),
the 2 new N=4 test suites with no edits to the 3 existing suites (Tasks
1-2, verified by Task 2's Step 7 explicitly re-running the wiring suite),
and documentation (Task 3, following the same pattern every prior
sub-project in this effort has needed).

**Placeholder scan:** no TBD/TODO; every code block is complete and
exact, reproducing the full current content of all 4 modified files so
the implementer has unambiguous before/after context.

**Type consistency:** `BasicLoco2MqttTurnoutCommandAdapter<N>` (Task 1)
and `BasicLoco2MqttFeedbackSource<N>` (Task 2) each get exactly the two
aliases the spec specifies (`X`/`X4`), and Task 2's `.cpp` body matches
its header's declared `poll`/constructor signatures exactly. Task 3's
CLAUDE.md section references both classes' final names correctly.
