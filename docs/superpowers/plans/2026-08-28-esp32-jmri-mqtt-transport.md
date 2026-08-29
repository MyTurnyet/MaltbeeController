# ESP32 JMRI/MQTT Transport (Slice 2b) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the ESP32 turnout panel a JMRI/MQTT transport layer — pure
topic/payload encoding, native-testable turnout command/feedback adapters,
and the WiFi/MQTT hardware shims underneath them — reusing the sibling
project `../MaltbeeTurnoutController`'s already-production-proven protocol,
re-keyed to this project's free-form JMRI-name-per-channel config.

**Architecture:** Mirrors the existing LocoNet send/receive split in
`lib/McsLoconet`. Send side: `TurnoutControl` → `TurnoutCommandPort`
(existing, unchanged) → `JmriTurnoutCommandAdapter` (new, native-testable) →
`MqttTransport` (new port) → `MqttLink` (new, `#ifdef ARDUINO`-guarded
shim). Receive side: `MqttLink` → `JmriFeedbackSource` (new,
native-testable, poll-based) → callers broadcast polled `TurnoutFeedback`
the same way `main.cpp` already does for LocoNet feedback. Two pure domain
classes (`TopicScheme`, `PayloadCodec`) underpin both sides. This plan does
**not** wire any of this into `src/esp32/main.cpp` — that's sub-projects
#6/#7, per the design spec's decomposition table.

**Tech Stack:** C++17, PlatformIO, Catch2 (native tests), ESP32 Arduino core
(`WiFi.h`) plus `PubSubClient` (already in `platformio.ini`'s `esp32dev`
`lib_deps`) for the two hardware shims.

## Global Constraints

- Domain and port headers must compile under `native` with no `Arduino.h`
  (mechanically enforced: if a file includes `<Arduino.h>`, it isn't
  domain/port code).
- `std::string`/`std::array`/`std::optional`/`std::deque` are fine
  throughout this slice — `lib/McsEsp32` only ever targets `native` and
  `esp32dev`, both with full libstdc++. Do not use `FixedString32` or
  fixed-capacity containers here; that type exists specifically to work
  around the Mega's AVR toolchain having no real STL, which doesn't apply
  to ESP32-only code.
- New files go under `lib/McsEsp32/src/{domain,ports,adapters}/`. A file in
  `McsEsp32` depending on a `McsCore` header uses a rooted include
  (`"ports/TurnoutCommandPort.h"`, `"domain/Turnout.h"`), matching the
  existing convention `McsLoconet` already uses for the same kind of
  cross-library dependency. Files depending on a header from the same
  library (`McsEsp32`) use relative includes (`"../domain/NodeConfig.h"`).
- `TurnoutCommandPort::send(int address, TurnoutPosition)` and
  `Turnout::address()` are **not modified** by this plan. For the JMRI
  adapters, that `int` is the 1–12 channel number, not a DCC address — the
  new adapters translate channel → JMRI name internally via
  `NodeConfig::channelJmriNames`, exactly as `MrrwaLocoNetTurnoutAdapter`
  translates address → LocoNet packet today. `NodeConfig::kChannelCount`
  (currently `12`) is the single source of truth for the channel count —
  never hardcode `12` in a new file; reference the constant.
- Both new JMRI adapters silently no-op / drop on anything they can't
  resolve (unconfigured channel, out-of-range channel, unrecognized MQTT
  payload) — same posture `MrrwaLocoNetTurnoutAdapter`/
  `LocoNetFeedbackDecoder` already have for addresses/reports they don't
  recognize. Never throw or assert in this layer.
- Hardware shims (`WiFiLink`, `MqttLink`) are wrapped in `#ifdef ARDUINO` /
  `#endif` in both their `.h` and `.cpp` files, `final` classes where they
  implement a port, matching `NvsConfigStore`/`EspUartPort`'s existing style
  exactly (not the Mega adapters' cpp-only-guard style — `McsEsp32`'s own
  precedent wins here since these files live in that library).
- Every new header uses `#pragma once`.
- PlatformIO's Library Dependency Finder (`lib_ldf_mode = deep+`) compiles
  every `.cpp` physically present in a library once that library is
  "used" by an environment — confirmed by inspecting
  `.pio/build/esp32dev/lib14e/McsEsp32/adapters/*.cpp.o`, which already
  contains object files for `NvsConfigStore.cpp`/`EspUartPort.cpp` despite
  `src/esp32/main.cpp` never including them. `McsEsp32` is already declared
  in `esp32dev`'s `lib_deps` in `platformio.ini`. This means Task 5's
  `WiFiLink.cpp`/`MqttLink.cpp` will be build-checked by a plain
  `pio run -e esp32dev` as soon as they exist on disk — **no temporary
  wiring into `src/esp32/main.cpp` is needed** (unlike slice 2a's Task 5,
  which predates this discovery).
- No mocking framework. Test doubles are hand-written classes implementing
  the same port interface, living in `test/support/`.
- Commit messages use this project's Arlo's Commit Notation (ACN) —
  `<risk symbol> <intention letter> <description>` — per `CLAUDE.md`. A
  mechanical `git mv` + include-path-only change with full existing test
  coverage passing unchanged is `. r`; a new class with full test coverage
  but more than 8 lines of change is `! F` (real change, not fully proven by
  a formal technique); a small (≤ 8 LoC), test-covered bug fix is `^ B`.

---

### Task 1: Cleanup — move `CommissioningSession` to `application/`, cap `SerialCommissioningAdapter`'s line buffer

Two deferred items from slice 2a's final whole-branch review, done first
since later tasks in this plan add more files to the same directories.

**Files:**
- Move: `lib/McsEsp32/src/domain/CommissioningSession.h` →
  `lib/McsEsp32/src/application/CommissioningSession.h`
- Move: `lib/McsEsp32/src/domain/CommissioningSession.cpp` →
  `lib/McsEsp32/src/application/CommissioningSession.cpp`
- Modify: `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h`
- Modify: `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.cpp`
- Modify: `test/test_commissioning_session/test_main.cpp`
- Modify: `test/test_serial_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes: nothing new — `CommissioningSession`'s own public interface
  (`apply`, `rebootRequested`) and `SerialCommissioningAdapter`'s
  (`poll`, `rebootRequested`) are unchanged by this task, only their
  location/internals change.
- Produces: `CommissioningSession` now lives in
  `lib/McsEsp32/src/application/`; `SerialCommissioningAdapter` gains a
  public `static constexpr size_t kMaxLineLength = 128;`.

- [ ] **Step 1: Move `CommissioningSession` into `application/`**

```bash
mkdir -p lib/McsEsp32/src/application
git mv lib/McsEsp32/src/domain/CommissioningSession.h lib/McsEsp32/src/application/CommissioningSession.h
git mv lib/McsEsp32/src/domain/CommissioningSession.cpp lib/McsEsp32/src/application/CommissioningSession.cpp
```

- [ ] **Step 2: Fix the one file that includes it by relative path**

In `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h`, change:

```cpp
#include "../domain/CommissioningSession.h"
```

to:

```cpp
#include "../application/CommissioningSession.h"
```

- [ ] **Step 3: Fix the one test that includes it by rooted path**

In `test/test_commissioning_session/test_main.cpp`, change:

```cpp
#include "domain/CommissioningSession.h"
```

to:

```cpp
#include "application/CommissioningSession.h"
```

- [ ] **Step 4: Run the moved suite and the full native suite to confirm nothing broke**

Run: `pio test -e native -f test_commissioning_session`
Expected: PASS, all 11 test cases green, unchanged from before the move.

Run: `pio test -e native`
Expected: PASS, every suite green (18 suites before this task).

- [ ] **Step 5: Commit the move**

```bash
git add lib/McsEsp32/src/application/CommissioningSession.h lib/McsEsp32/src/application/CommissioningSession.cpp lib/McsEsp32/src/domain/CommissioningSession.h lib/McsEsp32/src/domain/CommissioningSession.cpp lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h test/test_commissioning_session/test_main.cpp
git commit -m "$(cat <<'EOF'
. r Move CommissioningSession from domain/ to application/

It wires a ConfigStore& port directly, which is application-layer
work per this project's own layering rule, not domain work.
EOF
)"
```

- [ ] **Step 6: Write the failing test for the line-length cap**

Add to `test/test_serial_commissioning_adapter/test_main.cpp` (append after
the existing test cases, keeping the existing `#include`s):

```cpp
TEST_CASE("a line far exceeding the max length does not break subsequent commands")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    const std::string overlong(SerialCommissioningAdapter::kMaxLineLength * 2, 'x');
    uart.queueInput(overlong + "\nid 5\n");
    adapter.poll();

    REQUIRE(uart.written.find("OK\n") != std::string::npos);
}
```

- [ ] **Step 7: Run test to verify it fails**

Run: `pio test -e native -f test_serial_commissioning_adapter`
Expected: FAIL to compile — `kMaxLineLength` doesn't exist yet.

- [ ] **Step 8: Add the cap to `SerialCommissioningAdapter`**

In `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h`, add the
constant as a public member (add `#include <cstddef>` alongside the
existing `#include <string>`):

```cpp
#pragma once

#include <cstddef>
#include <string>

#include "../application/CommissioningSession.h"
#include "../ports/UartPort.h"

class SerialCommissioningAdapter
{
public:
    static constexpr size_t kMaxLineLength = 128;

    SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session);

    void poll();

    [[nodiscard]] bool rebootRequested() const;

private:
    UartPort& uart_;
    CommissioningSession& session_;
    std::string lineBuffer_;
};
```

In `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.cpp`, change
`poll()` to:

```cpp
void SerialCommissioningAdapter::poll()
{
    while (uart_.available())
    {
        const char c = uart_.read();
        if (c == '\n')
        {
            if (!lineBuffer_.empty() && lineBuffer_.back() == '\r')
            {
                lineBuffer_.pop_back();
            }
            const ParsedCommand command = CommandLineParser::parse(lineBuffer_);
            uart_.write(session_.apply(command));
            lineBuffer_.clear();
        }
        else if (lineBuffer_.size() >= kMaxLineLength)
        {
            lineBuffer_.clear();
        }
        else
        {
            lineBuffer_ += c;
        }
    }
}
```

- [ ] **Step 9: Run test to verify it passes**

Run: `pio test -e native -f test_serial_commissioning_adapter`
Expected: PASS, all 7 test cases green (6 existing + 1 new).

- [ ] **Step 10: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every suite green.

- [ ] **Step 11: Commit the cap**

```bash
git add lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h lib/McsEsp32/src/adapters/SerialCommissioningAdapter.cpp test/test_serial_commissioning_adapter/test_main.cpp
git commit -m "^ B Cap SerialCommissioningAdapter's unbounded line buffer"
```

---

### Task 2: `TopicScheme` and `PayloadCodec`

**Files:**
- Create: `lib/McsEsp32/src/domain/TopicScheme.h`
- Create: `lib/McsEsp32/src/domain/PayloadCodec.h`
- Test: `test/test_topic_scheme/test_main.cpp`
- Test: `test/test_payload_codec/test_main.cpp`

**Interfaces:**
- Consumes: `TurnoutPosition` (existing, `lib/McsCore/src/domain/Turnout.h`
  — rooted include `"domain/Turnout.h"`).
- Produces: `class TopicScheme { static std::string topicFor(const
  std::string& jmriName); }`; `class PayloadCodec { static std::string
  encode(TurnoutPosition); static std::optional<TurnoutPosition>
  decode(const std::string&); }`.

- [ ] **Step 1: Write the failing tests**

Create `test/test_topic_scheme/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/TopicScheme.h"

TEST_CASE("topicFor builds the expected prefixed topic")
{
    REQUIRE(TopicScheme::topicFor("LT5") == "track/turnout/LT5");
}

TEST_CASE("topicFor handles a different name")
{
    REQUIRE(TopicScheme::topicFor("Yard Ladder 2") == "track/turnout/Yard Ladder 2");
}

TEST_CASE("topicFor handles an empty name")
{
    REQUIRE(TopicScheme::topicFor("") == "track/turnout/");
}
```

Create `test/test_payload_codec/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/PayloadCodec.h"

TEST_CASE("encode Closed produces CLOSED")
{
    REQUIRE(PayloadCodec::encode(TurnoutPosition::Closed) == "CLOSED");
}

TEST_CASE("encode Thrown produces THROWN")
{
    REQUIRE(PayloadCodec::encode(TurnoutPosition::Thrown) == "THROWN");
}

TEST_CASE("decode CLOSED produces Closed")
{
    const std::optional<TurnoutPosition> position = PayloadCodec::decode("CLOSED");

    REQUIRE(position.has_value());
    REQUIRE(*position == TurnoutPosition::Closed);
}

TEST_CASE("decode THROWN produces Thrown")
{
    const std::optional<TurnoutPosition> position = PayloadCodec::decode("THROWN");

    REQUIRE(position.has_value());
    REQUIRE(*position == TurnoutPosition::Thrown);
}

TEST_CASE("decode an unrecognized payload produces nothing")
{
    REQUIRE_FALSE(PayloadCodec::decode("GARBAGE").has_value());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_topic_scheme`
Expected: FAIL to compile — `domain/TopicScheme.h` does not exist yet.

Run: `pio test -e native -f test_payload_codec`
Expected: FAIL to compile — `domain/PayloadCodec.h` does not exist yet.

- [ ] **Step 3: Write `lib/McsEsp32/src/domain/TopicScheme.h`**

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

- [ ] **Step 4: Write `lib/McsEsp32/src/domain/PayloadCodec.h`**

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
        if (payload == "CLOSED")
        {
            return TurnoutPosition::Closed;
        }
        if (payload == "THROWN")
        {
            return TurnoutPosition::Thrown;
        }
        return std::nullopt;
    }
};
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_topic_scheme`
Expected: PASS, all 3 test cases green.

Run: `pio test -e native -f test_payload_codec`
Expected: PASS, all 5 test cases green.

- [ ] **Step 6: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every existing suite plus the 2 new ones green.

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/domain/TopicScheme.h lib/McsEsp32/src/domain/PayloadCodec.h test/test_topic_scheme/test_main.cpp test/test_payload_codec/test_main.cpp
git commit -m "! F Add TopicScheme and PayloadCodec"
```

---

### Task 3: `MqttTransport` port, `FakeMqttTransport`, and `JmriTurnoutCommandAdapter`

**Files:**
- Create: `lib/McsEsp32/src/ports/MqttTransport.h`
- Create: `test/support/FakeMqttTransport.h`
- Create: `lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.h`
- Create: `lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.cpp`
- Test: `test/test_jmri_turnout_command_adapter/test_main.cpp`

**Interfaces:**
- Consumes: `TopicScheme`/`PayloadCodec` (Task 2);
  `TurnoutCommandPort`/`TurnoutFeedback` (existing,
  `lib/McsCore/src/ports/TurnoutCommandPort.h`, rooted include
  `"ports/TurnoutCommandPort.h"`); `NodeConfig::kChannelCount` (existing,
  `lib/McsEsp32/src/domain/NodeConfig.h`, relative include
  `"../domain/NodeConfig.h"`).
- Produces: `class MqttTransport { virtual void publish(const std::string&
  topic, const std::string& payload, bool retained) = 0; virtual void
  subscribe(const std::string& topic, std::function<void(const
  std::string&)> handler) = 0; }`; `class FakeMqttTransport final : public
  MqttTransport` with public `std::vector<PublishedMessage> published`
  (`struct PublishedMessage { std::string topic; std::string payload; bool
  retained; }`), public `std::vector<std::string> subscribedTopics`, and
  `void deliver(const std::string& topic, const std::string& payload)`;
  `class JmriTurnoutCommandAdapter final : public TurnoutCommandPort` —
  `JmriTurnoutCommandAdapter(MqttTransport&, const std::array<std::string,
  NodeConfig::kChannelCount>&)`; `void send(int address, TurnoutPosition)
  override`. Later tasks (4) reuse `FakeMqttTransport`.

- [ ] **Step 1: Write the failing test**

Create `test/test_jmri_turnout_command_adapter/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/JmriTurnoutCommandAdapter.h"
#include "domain/NodeConfig.h"
#include "support/FakeMqttTransport.h"

namespace
{
    std::array<std::string, NodeConfig::kChannelCount> namesWithChannel(int channel, const std::string& name)
    {
        std::array<std::string, NodeConfig::kChannelCount> names;
        names[channel - 1] = name;
        return names;
    }
}

TEST_CASE("send publishes the expected topic, payload, and retained flag")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriTurnoutCommandAdapter adapter(transport, names);

    adapter.send(1, TurnoutPosition::Closed);

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "track/turnout/LT1");
    REQUIRE(transport.published[0].payload == "CLOSED");
    REQUIRE(transport.published[0].retained);
}

TEST_CASE("send encodes Thrown correctly")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriTurnoutCommandAdapter adapter(transport, names);

    adapter.send(1, TurnoutPosition::Thrown);

    REQUIRE(transport.published[0].payload == "THROWN");
}

TEST_CASE("send resolves the correct channel out of several configured")
{
    FakeMqttTransport transport;
    auto names = namesWithChannel(1, "LT1");
    names[4] = "LT5";
    JmriTurnoutCommandAdapter adapter(transport, names);

    adapter.send(5, TurnoutPosition::Closed);

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "track/turnout/LT5");
}

TEST_CASE("send is a no-op for an unconfigured channel")
{
    FakeMqttTransport transport;
    const std::array<std::string, NodeConfig::kChannelCount> names{};
    JmriTurnoutCommandAdapter adapter(transport, names);

    adapter.send(1, TurnoutPosition::Closed);

    REQUIRE(transport.published.empty());
}

TEST_CASE("send is a no-op for an out-of-range channel")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriTurnoutCommandAdapter adapter(transport, names);

    adapter.send(0, TurnoutPosition::Closed);
    adapter.send(13, TurnoutPosition::Closed);

    REQUIRE(transport.published.empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_jmri_turnout_command_adapter`
Expected: FAIL to compile — none of this task's files exist yet.

- [ ] **Step 3: Write `lib/McsEsp32/src/ports/MqttTransport.h`**

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

- [ ] **Step 4: Write `test/support/FakeMqttTransport.h`**

```cpp
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ports/MqttTransport.h"

struct PublishedMessage
{
    std::string topic;
    std::string payload;
    bool retained;
};

class FakeMqttTransport final : public MqttTransport
{
public:
    std::vector<PublishedMessage> published;
    std::vector<std::string> subscribedTopics;

    void publish(const std::string& topic, const std::string& payload, const bool retained) override
    {
        published.push_back({topic, payload, retained});
    }

    void subscribe(const std::string& topic, std::function<void(const std::string&)> handler) override
    {
        subscribedTopics.push_back(topic);
        handlers_.emplace_back(topic, std::move(handler));
    }

    // Test helper: simulate an incoming message on `topic`, invoking
    // whichever handler(s) subscribed to it. No-op if nothing is
    // subscribed there.
    void deliver(const std::string& topic, const std::string& payload)
    {
        for (const auto& entry : handlers_)
        {
            if (entry.first == topic)
            {
                entry.second(payload);
            }
        }
    }

private:
    std::vector<std::pair<std::string, std::function<void(const std::string&)>>> handlers_;
};
```

- [ ] **Step 5: Write `lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.h`**

```cpp
#pragma once

#include <array>
#include <string>

#include "../domain/NodeConfig.h"
#include "ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

class JmriTurnoutCommandAdapter final : public TurnoutCommandPort
{
public:
    JmriTurnoutCommandAdapter(MqttTransport& transport,
                               const std::array<std::string, NodeConfig::kChannelCount>& channelJmriNames);

    void send(int address, TurnoutPosition position) override;

private:
    MqttTransport& transport_;
    const std::array<std::string, NodeConfig::kChannelCount>& channelJmriNames_;
};
```

- [ ] **Step 6: Write `lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.cpp`**

```cpp
#include "JmriTurnoutCommandAdapter.h"

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

JmriTurnoutCommandAdapter::JmriTurnoutCommandAdapter(
    MqttTransport& transport, const std::array<std::string, NodeConfig::kChannelCount>& channelJmriNames)
    : transport_(transport), channelJmriNames_(channelJmriNames)
{
}

void JmriTurnoutCommandAdapter::send(const int address, const TurnoutPosition position)
{
    if (address < 1 || address > NodeConfig::kChannelCount)
    {
        return;
    }

    const std::string& jmriName = channelJmriNames_[address - 1];
    if (jmriName.empty())
    {
        return;
    }

    transport_.publish(TopicScheme::topicFor(jmriName), PayloadCodec::encode(position), true);
}
```

- [ ] **Step 7: Run test to verify it passes**

Run: `pio test -e native -f test_jmri_turnout_command_adapter`
Expected: PASS, all 5 test cases green.

- [ ] **Step 8: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every existing suite plus the 3 new ones from Tasks 2-3
green.

- [ ] **Step 9: Commit**

```bash
git add lib/McsEsp32/src/ports/MqttTransport.h test/support/FakeMqttTransport.h lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.h lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.cpp test/test_jmri_turnout_command_adapter/test_main.cpp
git commit -m "! F Add MqttTransport port and JmriTurnoutCommandAdapter"
```

---

### Task 4: `JmriFeedbackSource`

**Files:**
- Create: `lib/McsEsp32/src/adapters/JmriFeedbackSource.h`
- Create: `lib/McsEsp32/src/adapters/JmriFeedbackSource.cpp`
- Test: `test/test_jmri_feedback_source/test_main.cpp`

**Interfaces:**
- Consumes: `MqttTransport`/`FakeMqttTransport` (Task 3);
  `TopicScheme`/`PayloadCodec` (Task 2); `TurnoutFeedback` (existing,
  `lib/McsCore/src/ports/TurnoutCommandPort.h`); `NodeConfig::kChannelCount`
  (existing).
- Produces: `class JmriFeedbackSource { JmriFeedbackSource(MqttTransport&,
  const std::array<std::string, NodeConfig::kChannelCount>&); bool
  poll(TurnoutFeedback& outFeedback); }`.

- [ ] **Step 1: Write the failing test**

Create `test/test_jmri_feedback_source/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/JmriFeedbackSource.h"
#include "domain/NodeConfig.h"
#include "support/FakeMqttTransport.h"

namespace
{
    std::array<std::string, NodeConfig::kChannelCount> namesWithChannel(int channel, const std::string& name)
    {
        std::array<std::string, NodeConfig::kChannelCount> names;
        names[channel - 1] = name;
        return names;
    }
}

TEST_CASE("construction subscribes only the configured channels' topics")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");

    JmriFeedbackSource source(transport, names);

    REQUIRE(transport.subscribedTopics.size() == 1);
    REQUIRE(transport.subscribedTopics[0] == "track/turnout/LT1");
}

TEST_CASE("construction subscribes each configured channel among several")
{
    FakeMqttTransport transport;
    auto names = namesWithChannel(1, "LT1");
    names[4] = "LT5";

    JmriFeedbackSource source(transport, names);

    REQUIRE(transport.subscribedTopics.size() == 2);
}

TEST_CASE("a valid incoming payload becomes a pollable TurnoutFeedback")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(3, "LT3");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT3", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE(feedback.address == 3);
    REQUIRE(feedback.position == TurnoutPosition::Thrown);
}

TEST_CASE("an unrecognized payload produces nothing")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT1", "GARBAGE");

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("poll returns false once the queue is drained")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT1", "CLOSED");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("multiple queued messages drain in FIFO order")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT1", "CLOSED");
    transport.deliver("track/turnout/LT1", "THROWN");

    TurnoutFeedback first{};
    TurnoutFeedback second{};
    REQUIRE(source.poll(first));
    REQUIRE(source.poll(second));
    REQUIRE(first.position == TurnoutPosition::Closed);
    REQUIRE(second.position == TurnoutPosition::Thrown);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_jmri_feedback_source`
Expected: FAIL to compile — none of this task's files exist yet.

- [ ] **Step 3: Write `lib/McsEsp32/src/adapters/JmriFeedbackSource.h`**

```cpp
#pragma once

#include <array>
#include <deque>
#include <string>

#include "../domain/NodeConfig.h"
#include "ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

class JmriFeedbackSource
{
public:
    JmriFeedbackSource(MqttTransport& transport,
                        const std::array<std::string, NodeConfig::kChannelCount>& channelJmriNames);

    bool poll(TurnoutFeedback& outFeedback);

private:
    std::deque<TurnoutFeedback> pending_;
};
```

- [ ] **Step 4: Write `lib/McsEsp32/src/adapters/JmriFeedbackSource.cpp`**

```cpp
#include "JmriFeedbackSource.h"

#include <optional>

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

JmriFeedbackSource::JmriFeedbackSource(MqttTransport& transport,
                                        const std::array<std::string, NodeConfig::kChannelCount>& channelJmriNames)
{
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string& jmriName = channelJmriNames[i];
        if (jmriName.empty())
        {
            continue;
        }

        const int channel = i + 1;
        transport.subscribe(TopicScheme::topicFor(jmriName), [this, channel](const std::string& payload) {
            const std::optional<TurnoutPosition> position = PayloadCodec::decode(payload);
            if (!position.has_value())
            {
                return;
            }
            pending_.push_back(TurnoutFeedback{channel, *position});
        });
    }
}

bool JmriFeedbackSource::poll(TurnoutFeedback& outFeedback)
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

- [ ] **Step 5: Run test to verify it passes**

Run: `pio test -e native -f test_jmri_feedback_source`
Expected: PASS, all 6 test cases green.

- [ ] **Step 6: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every existing suite plus the 4 new ones from Tasks 2-4
green.

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/adapters/JmriFeedbackSource.h lib/McsEsp32/src/adapters/JmriFeedbackSource.cpp test/test_jmri_feedback_source/test_main.cpp
git commit -m "! F Add JmriFeedbackSource"
```

---

### Task 5: `WiFiLink` and `MqttLink` hardware shims

**Files:**
- Create: `lib/McsEsp32/src/adapters/WiFiLink.h`
- Create: `lib/McsEsp32/src/adapters/WiFiLink.cpp`
- Create: `lib/McsEsp32/src/adapters/MqttLink.h`
- Create: `lib/McsEsp32/src/adapters/MqttLink.cpp`

**Interfaces:**
- Consumes: `Clock` (existing, `lib/McsCore/src/ports/Clock.h`, rooted
  include `"ports/Clock.h"`); `MqttTransport` (Task 3).
- Produces: `class WiFiLink { WiFiLink(Clock&, unsigned long
  retryIntervalMs); void begin(const std::string& ssid, const std::string&
  password); void poll(); bool connected() const; }`; `class MqttLink final
  : public MqttTransport { MqttLink(Clock&, unsigned long retryIntervalMs,
  std::string clientId, std::string willTopic, std::string willMessage);
  void begin(const std::string& host, int port); void poll(); bool
  connected(); void publish(...) override; void subscribe(...) override; }`.

No native test exists for these — both are `#ifdef ARDUINO`-guarded
hardware shims with no logic beyond wrapping ESP32/`PubSubClient` APIs,
verified instead by a build-check against the real `esp32dev` target,
matching this project's existing convention for
`NvsConfigStore`/`EspUartPort`. Per this plan's Global Constraints section,
no temporary `main.cpp` wiring is needed — the PlatformIO LDF already
compiles every `.cpp` in `lib/McsEsp32` for `esp32dev` since that library is
already in `esp32dev`'s `lib_deps`.

- [ ] **Step 1: Write `lib/McsEsp32/src/adapters/WiFiLink.h`**

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
    [[nodiscard]] bool connected() const;

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

- [ ] **Step 2: Write `lib/McsEsp32/src/adapters/WiFiLink.cpp`**

```cpp
#ifdef ARDUINO

#include "WiFiLink.h"

WiFiLink::WiFiLink(Clock& clock, const unsigned long retryIntervalMs)
    : clock_(clock), retryIntervalMs_(retryIntervalMs)
{
}

void WiFiLink::begin(const std::string& ssid, const std::string& password)
{
    ssid_ = ssid;
    password_ = password;

    // A prior wireless-setup session (slice 2c's captive portal) may call
    // WiFi.softAP(), and esp_wifi persists mode/config to flash by default -
    // so a leftover AP interface can still be active on a later boot,
    // including this one. Force STA-only before the first connect attempt.
    WiFi.mode(WIFI_STA);

    connect();
}

void WiFiLink::poll()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    if (clock_.nowMilliseconds() - lastAttemptMs_ >= retryIntervalMs_)
    {
        connect();
    }
}

bool WiFiLink::connected() const
{
    return WiFi.status() == WL_CONNECTED;
}

void WiFiLink::connect()
{
    WiFi.begin(ssid_.c_str(), password_.c_str());
    lastAttemptMs_ = clock_.nowMilliseconds();
}

#endif
```

- [ ] **Step 3: Write `lib/McsEsp32/src/adapters/MqttLink.h`**

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
    void dispatch(const std::string& topic, const std::string& payload);

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

- [ ] **Step 4: Write `lib/McsEsp32/src/adapters/MqttLink.cpp`**

```cpp
#ifdef ARDUINO

#include "MqttLink.h"

MqttLink::MqttLink(Clock& clock, const unsigned long retryIntervalMs, std::string clientId,
                    std::string willTopic, std::string willMessage)
    : clock_(clock),
      retryIntervalMs_(retryIntervalMs),
      clientId_(std::move(clientId)),
      willTopic_(std::move(willTopic)),
      willMessage_(std::move(willMessage)),
      client_(wifiClient_)
{
    client_.setCallback([this](char* topic, byte* payload, unsigned int length) {
        dispatch(topic, std::string(reinterpret_cast<char*>(payload), length));
    });
}

void MqttLink::begin(const std::string& host, const int port)
{
    client_.setServer(host.c_str(), port);
    connect();
}

void MqttLink::poll()
{
    if (client_.connected())
    {
        client_.loop();
        return;
    }

    if (clock_.nowMilliseconds() - lastAttemptMs_ >= retryIntervalMs_)
    {
        connect();
    }
}

bool MqttLink::connected()
{
    return client_.connected();
}

void MqttLink::publish(const std::string& topic, const std::string& payload, const bool retained)
{
    client_.publish(topic.c_str(), payload.c_str(), retained);
}

void MqttLink::subscribe(const std::string& topic, std::function<void(const std::string&)> handler)
{
    handlers_.emplace_back(topic, std::move(handler));
    client_.subscribe(topic.c_str());
}

void MqttLink::dispatch(const std::string& topic, const std::string& payload)
{
    for (const auto& entry : handlers_)
    {
        if (entry.first == topic)
        {
            entry.second(payload);
        }
    }
}

void MqttLink::connect()
{
    client_.connect(clientId_.c_str(), willTopic_.c_str(), 1, true, willMessage_.c_str());
    lastAttemptMs_ = clock_.nowMilliseconds();

    // PubSubClient forgets subscriptions across a dropped session - replay
    // every topic this link has ever subscribed to on every successful
    // (re)connect.
    if (client_.connected())
    {
        for (const auto& entry : handlers_)
        {
            client_.subscribe(entry.first.c_str());
        }
    }
}

#endif
```

- [ ] **Step 5: Run the full native suite to confirm nothing broke**

Run: `pio test -e native`
Expected: PASS — these two files are Arduino-only and contribute nothing to
the native build, so this just confirms Tasks 1-4 are still green.

- [ ] **Step 6: Build for the ESP32 target**

Run: `pio run -e esp32dev`
Expected: SUCCESS — confirms `WiFiLink` and `MqttLink` compile cleanly
against the real `WiFi.h`/`PubSubClient` APIs. Optionally confirm the two
new object files exist:
`find .pio/build/esp32dev -iname "WiFiLink.cpp.o" -o -iname "MqttLink.cpp.o"`.

- [ ] **Step 7: Build for the Mega target to confirm it's still unaffected**

Run: `pio run -e megaatmega2560`
Expected: SUCCESS — `lib/McsEsp32` is `lib_ignore`d for `megaatmega2560` in
`platformio.ini`, so this task's new files should never even be considered
for that build.

- [ ] **Step 8: Commit**

```bash
git add lib/McsEsp32/src/adapters/WiFiLink.h lib/McsEsp32/src/adapters/WiFiLink.cpp lib/McsEsp32/src/adapters/MqttLink.h lib/McsEsp32/src/adapters/MqttLink.cpp
git commit -m "! F Add WiFiLink and MqttLink ESP32 hardware adapters"
```

---

## Definition of Done for this Plan

- [ ] `pio test -e native` passes, including the 4 new suites
      (`test_topic_scheme`, `test_payload_codec`,
      `test_jmri_turnout_command_adapter`, `test_jmri_feedback_source`) plus
      the 2 modified ones (`test_commissioning_session`,
      `test_serial_commissioning_adapter`) — 22 suites total (18 existing +
      4 new).
- [ ] `pio run -e megaatmega2560` still builds cleanly (unaffected —
      `McsEsp32` is `lib_ignore`d there — verify rather than assume).
- [ ] `pio run -e esp32dev` builds cleanly, and the resulting build includes
      compiled object files for `WiFiLink.cpp` and `MqttLink.cpp` (proving
      they were actually build-checked, not just present on disk).
- [ ] Seven commits on `main` (or a feature branch, per whatever the
      executor chooses): Task 1's two (`. r` move, `^ B` cap), then one `! F`
      each for Tasks 2-5.
- [ ] Nothing in this plan touches `src/esp32/main.cpp`, wires the new
      adapters into `TurnoutStation`/`TurnoutControl`, or implements node
      presence/collision detection or wireless captive-portal commissioning
      — all explicitly deferred to slices 2c/2d and sub-projects #6/#7 per
      `docs/superpowers/specs/2026-08-28-esp32-jmri-mqtt-transport-design.md`.
