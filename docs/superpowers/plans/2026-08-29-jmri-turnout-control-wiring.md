# JMRI Turnout Command/Feedback Wiring (Sub-project #6) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve the JMRI/MQTT topic self-echo risk flagged in slice 2b's
final review (per
`docs/superpowers/specs/2026-08-29-jmri-topic-self-echo-design.md`) by
splitting the command and feedback topics, then prove with an end-to-end
native test that `JmriTurnoutCommandAdapter`, `JmriFeedbackSource`, and
`TurnoutControl` compose correctly through the existing 1–12 channel-as-
address contract, including that a self-published command can never leak
back as feedback.

**Architecture:** No new classes. `TopicScheme` gains a second static method
for the state (feedback) topic; `JmriFeedbackSource` switches its
subscription to that new topic while `JmriTurnoutCommandAdapter` keeps
publishing to the existing command topic, unchanged. `TurnoutStation`
(`lib/McsCore/src/adapters/TurnoutStation.h`) is already fully generic over
any `TurnoutCommandPort` and needs no changes — it becomes usable with
`JmriTurnoutCommandAdapter` for free once sub-project #7 wires it up in
`src/esp32/main.cpp`. This plan's own proof of correctness is a new
integration-style native test that constructs the real classes (no new test
doubles beyond what already exists) end-to-end.

**Tech Stack:** C++17, PlatformIO, Catch2 (native tests). No hardware, no
Arduino-guarded code, and no `esp32dev`/`src/esp32/main.cpp` changes in this
plan.

## Global Constraints

- Domain and port headers must compile under `native` with no `Arduino.h`
  (this plan touches no Arduino-guarded files at all).
- `lib/McsEsp32` targets `native` and `esp32dev` only, both with full
  libstdc++ — `std::string`/`std::array` remain fine throughout, no
  `FixedString32`.
- No mocking framework. `FakeMqttTransport` (`test/support/FakeMqttTransport.h`)
  already exists and is reused unchanged by both tasks below.
- `JmriTurnoutCommandAdapter::send(int address, TurnoutPosition)` treats
  `address` as the 1–12 channel number (not a DCC address) — this plan does
  not change that contract; `Turnout::address()` supplies the same channel
  number when constructing a `Turnout` for the integration test in Task 2,
  exactly like `MrrwaLocoNetTurnoutAdapter`'s existing precedent of reusing
  the same field for a different transport's addressing scheme.
- Commit messages use this project's Arlo's Commit Notation (ACN) —
  `<risk symbol> <intention letter> <description>` — per `CLAUDE.md`.
- This plan does **not** touch `src/esp32/main.cpp`, `TurnoutStation`, or any
  composition-root wiring — that's sub-project #7. It also does not
  implement the corresponding change needed in the sibling
  `../MaltbeeTurnoutController` project (its `MqttPositionReporter` needs to
  additionally publish to the new state topic) — that's an external
  dependency to raise with that project, tracked in
  `docs/superpowers/specs/2026-08-29-jmri-topic-self-echo-design.md`, not
  implemented by this plan.

---

### Task 1: Split the JMRI command and state (feedback) topics

**Files:**
- Modify: `lib/McsEsp32/src/domain/TopicScheme.h`
- Modify: `lib/McsEsp32/src/adapters/JmriFeedbackSource.cpp`
- Modify: `test/test_topic_scheme/test_main.cpp`
- Modify: `test/test_jmri_feedback_source/test_main.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `TopicScheme::stateTopicFor(const std::string& jmriName) ->
  std::string` (new, alongside the existing `TopicScheme::topicFor(...)`,
  which is unchanged and remains the command topic).
  `JmriFeedbackSource`'s public interface (`poll(TurnoutFeedback&)`) is
  unchanged — only which topic it subscribes to internally changes. Task 2
  consumes both `topicFor` and `stateTopicFor`.

- [ ] **Step 1: Write the failing test for `stateTopicFor`**

Add to `test/test_topic_scheme/test_main.cpp` (append after the existing
three `TEST_CASE`s, keep the existing `#include`):

```cpp
TEST_CASE("stateTopicFor builds the expected state-suffixed topic")
{
    REQUIRE(TopicScheme::stateTopicFor("LT5") == "track/turnout/LT5/state");
}

TEST_CASE("stateTopicFor differs from topicFor for the same name")
{
    REQUIRE(TopicScheme::stateTopicFor("LT1") != TopicScheme::topicFor("LT1"));
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_topic_scheme`
Expected: FAIL to compile — `TopicScheme::stateTopicFor` does not exist yet.

- [ ] **Step 3: Add `stateTopicFor` to `TopicScheme`**

Replace the full contents of `lib/McsEsp32/src/domain/TopicScheme.h` with:

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

    static std::string stateTopicFor(const std::string& jmriName)
    {
        return "track/turnout/" + jmriName + "/state";
    }
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_topic_scheme`
Expected: PASS, all 5 test cases green (3 existing + 2 new).

- [ ] **Step 5: Write the failing test for `JmriFeedbackSource` subscribing to the state topic**

In `test/test_jmri_feedback_source/test_main.cpp`, update every expected
topic string from the command-topic form to the state-topic form. Replace
the full file contents with:

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

TEST_CASE("construction subscribes only the configured channels' state topics")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");

    JmriFeedbackSource source(transport, names);

    REQUIRE(transport.subscribedTopics.size() == 1);
    REQUIRE(transport.subscribedTopics[0] == "track/turnout/LT1/state");
}

TEST_CASE("construction subscribes each configured channel's state topic among several")
{
    FakeMqttTransport transport;
    auto names = namesWithChannel(1, "LT1");
    names[4] = "LT5";

    JmriFeedbackSource source(transport, names);

    REQUIRE(transport.subscribedTopics.size() == 2);
}

TEST_CASE("a valid incoming payload on the state topic becomes a pollable TurnoutFeedback")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(3, "LT3");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT3/state", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE(feedback.address == 3);
    REQUIRE(feedback.position == TurnoutPosition::Thrown);
}

TEST_CASE("an unrecognized payload on the state topic produces nothing")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT1/state", "GARBAGE");

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("a message on the plain command topic is not picked up as feedback")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT1", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("poll returns false once the queue is drained")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT1/state", "CLOSED");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("multiple queued messages drain in FIFO order")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT1/state", "CLOSED");
    transport.deliver("track/turnout/LT1/state", "THROWN");

    TurnoutFeedback first{};
    TurnoutFeedback second{};
    REQUIRE(source.poll(first));
    REQUIRE(source.poll(second));
    REQUIRE(first.position == TurnoutPosition::Closed);
    REQUIRE(second.position == TurnoutPosition::Thrown);
}
```

Note the new test case "a message on the plain command topic is not picked
up as feedback" — this is the direct regression test for the self-echo risk:
it proves `JmriFeedbackSource` never subscribes to the topic
`JmriTurnoutCommandAdapter` publishes to.

- [ ] **Step 6: Run the test to verify it fails**

Run: `pio test -e native -f test_jmri_feedback_source`
Expected: FAIL — the existing implementation still subscribes to
`TopicScheme::topicFor(...)` (the command topic), so
`transport.subscribedTopics[0]` will be `"track/turnout/LT1"`, not
`"track/turnout/LT1/state"`, and the `deliver("track/turnout/LT3/state",
...)`-based cases will find no matching subscription.

- [ ] **Step 7: Switch `JmriFeedbackSource` to subscribe to the state topic**

In `lib/McsEsp32/src/adapters/JmriFeedbackSource.cpp`, change the single
line:

```cpp
        transport.subscribe(TopicScheme::topicFor(jmriName), [this, channel](const std::string& payload) {
```

to:

```cpp
        transport.subscribe(TopicScheme::stateTopicFor(jmriName), [this, channel](const std::string& payload) {
```

- [ ] **Step 8: Run the test to verify it passes**

Run: `pio test -e native -f test_jmri_feedback_source`
Expected: PASS, all 7 test cases green (6 existing, reworded to the state
topic, plus 1 new regression case).

- [ ] **Step 9: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every suite green (22 suites, unchanged count — this task
only modifies two existing suites, no new file yet).

- [ ] **Step 10: Build for the ESP32 and Mega targets to confirm both are unaffected**

Run: `pio run -e esp32dev`
Expected: SUCCESS.

Run: `pio run -e megaatmega2560`
Expected: SUCCESS — `McsEsp32` is `lib_ignore`d there, so this task's files
are never compiled for that target.

- [ ] **Step 11: Commit**

```bash
git add lib/McsEsp32/src/domain/TopicScheme.h lib/McsEsp32/src/adapters/JmriFeedbackSource.cpp test/test_topic_scheme/test_main.cpp test/test_jmri_feedback_source/test_main.cpp
git commit -m "$(cat <<'EOF'
^ B Split JMRI command/state topics to eliminate self-echo

JmriFeedbackSource subscribed the same topic
JmriTurnoutCommandAdapter publishes commands to, so a button press
would deliver the panel's own command back to itself as feedback.
Add TopicScheme::stateTopicFor() and subscribe there instead; the
driver-only publish that lands on it is added in the sibling
MaltbeeTurnoutController project, tracked separately.
EOF
)"
```

---

### Task 2: End-to-end integration test proving the wiring is correct and self-echo-immune

**Files:**
- Create: `test/test_jmri_turnout_wiring/test_main.cpp`

**Interfaces:**
- Consumes: `JmriTurnoutCommandAdapter`, `JmriFeedbackSource`,
  `TopicScheme` (all from Task 1 and 2b, unchanged by this task);
  `TurnoutControl`, `Button`, `Indicator`, `Turnout`, `TurnoutIndicator`
  (existing, `lib/McsCore/src/{application,domain}/`); `FakeClock`,
  `FakeDigitalInput`, `FakeDigitalOutput`, `FakeMqttTransport`,
  `FakeTurnoutCommandPort` (existing, `test/support/`).
- Produces: nothing new — this is a proof, not a new component. No later
  task depends on this file.

- [ ] **Step 1: Write the integration test**

Create `test/test_jmri_turnout_wiring/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/JmriFeedbackSource.h"
#include "adapters/JmriTurnoutCommandAdapter.h"
#include "application/TurnoutControl.h"
#include "domain/Button.h"
#include "domain/Indicator.h"
#include "domain/NodeConfig.h"
#include "domain/Turnout.h"
#include "domain/TurnoutIndicator.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeDigitalOutput.h"
#include "support/FakeMqttTransport.h"
#include "support/FakeTurnoutCommandPort.h"

namespace
{
    constexpr unsigned long DEBOUNCE_MS = 30;
    constexpr int CHANNEL = 1;

    std::array<std::string, NodeConfig::kChannelCount> namesWithChannel(int channel, const std::string& name)
    {
        std::array<std::string, NodeConfig::kChannelCount> names;
        names[channel - 1] = name;
        return names;
    }

    void pressButton(Button& button, FakeDigitalInput& input, FakeClock& clock)
    {
        input.active = true;
        button.update();
        clock.advanceBy(DEBOUNCE_MS);
        button.update();
    }
}

TEST_CASE("a button press publishes a command that does not leak back as feedback, even when the broker echoes it verbatim")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(CHANNEL, "LT1");
    JmriTurnoutCommandAdapter commandAdapter(transport, names);
    JmriFeedbackSource feedbackSource(transport, names);

    FakeDigitalInput throwInput;
    FakeDigitalInput closeInput;
    FakeClock clock;
    Button throwButton(throwInput, clock, DEBOUNCE_MS);
    Button closeButton(closeInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(CHANNEL, "LT1", TurnoutPosition::Closed, false, false);
    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandAdapter);

    pressButton(throwButton, throwInput, clock);
    control.update();

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "track/turnout/LT1");
    REQUIRE(transport.published[0].payload == "THROWN");

    // Simulate a real MQTT broker faithfully echoing the panel's own
    // publish back to it, exactly as MQTT 3.1.1 does with no "No Local"
    // option available.
    transport.deliver(transport.published[0].topic, transport.published[0].payload);

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(feedbackSource.poll(feedback));
    REQUIRE(turnout.position() == TurnoutPosition::Closed);
    REQUIRE_FALSE(thrownIndicator.isOn());
}

TEST_CASE("the real driver's confirmation on the dedicated state topic updates the turnout and indicator")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(CHANNEL, "LT1");
    JmriFeedbackSource feedbackSource(transport, names);

    FakeDigitalInput throwInput;
    FakeDigitalInput closeInput;
    FakeClock clock;
    Button throwButton(throwInput, clock, DEBOUNCE_MS);
    Button closeButton(closeInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(CHANNEL, "LT1", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;
    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    transport.deliver("track/turnout/LT1/state", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE(feedbackSource.poll(feedback));
    REQUIRE(feedback.address == CHANNEL);
    REQUIRE(feedback.position == TurnoutPosition::Thrown);

    control.applyFeedback(feedback);

    REQUIRE(turnout.position() == TurnoutPosition::Thrown);
    REQUIRE(thrownIndicator.isOn());
    REQUIRE_FALSE(closedIndicator.isOn());
}
```

- [ ] **Step 2: Run the test to verify it passes**

Run: `pio test -e native -f test_jmri_turnout_wiring`
Expected: PASS, both test cases green. (No implementation step is needed
here — Task 1 already made this correct. This step exists to prove that,
not to drive new code.)

- [ ] **Step 3: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every suite green — 23 suites total (22 from before this
plan + this new one).

- [ ] **Step 4: Commit**

```bash
git add test/test_jmri_turnout_wiring/test_main.cpp
git commit -m "$(cat <<'EOF'
. r Add integration test proving JMRI command/feedback wiring is self-echo-immune

Fully-tested, dev-only addition with no production change: composes
JmriTurnoutCommandAdapter, JmriFeedbackSource, and TurnoutControl
end-to-end through the existing 1-12 channel-as-address contract, and
proves a broker echoing the panel's own command back verbatim never
reaches TurnoutControl as feedback.
EOF
)"
```

---

## Definition of Done for this Plan

- [ ] `pio test -e native` passes, including the 2 modified suites
      (`test_topic_scheme`, `test_jmri_feedback_source`) and the 1 new
      suite (`test_jmri_turnout_wiring`) — 23 suites total (22 existing +
      1 new).
- [ ] `pio run -e esp32dev` builds cleanly (this plan's files are compiled
      there via `McsEsp32`'s existing `lib_deps` entry — verify rather than
      assume).
- [ ] `pio run -e megaatmega2560` still builds cleanly (unaffected —
      `McsEsp32` is `lib_ignore`d there).
- [ ] Two commits on `main` (or a feature branch): `^ B` for Task 1's topic
      split, `. r` for Task 2's integration test.
- [ ] Nothing in this plan touches `src/esp32/main.cpp`, `TurnoutStation`,
      or any composition-root wiring — that remains sub-project #7.
- [ ] The corresponding additive change in `../MaltbeeTurnoutController`'s
      `MqttPositionReporter` (publish state to the new `.../state` topic in
      addition to the existing shared topic) is **not** implemented by this
      plan — it's an external dependency on a separate project, tracked in
      `docs/superpowers/specs/2026-08-29-jmri-topic-self-echo-design.md`.
      Until it lands there, `JmriFeedbackSource` will receive no real
      feedback on hardware (turnouts stay in "unconfirmed" blink mode) —
      a safe degraded state, not a defect in this plan's own scope.
