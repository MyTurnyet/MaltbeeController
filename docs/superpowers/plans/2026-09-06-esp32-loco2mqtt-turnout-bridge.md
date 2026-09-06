# ESP32 Loco2MQTT Turnout Bridge (Sub-project #9a) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Swap the ESP32 panel's turnout MQTT integration from JMRI-over-MQTT to Loco2MQTT's direct LocoNet-bridge contract, add mDNS broker discovery, and display a firmware version on the setup web page.

**Architecture:** Same hexagonal shape as the JMRI integration it replaces — a `TurnoutCommandPort`-implementing send adapter and a polling feedback-source adapter, both behind the existing `MqttTransport` port, re-keyed from a JMRI system-name string lookup to a plain LocoNet integer address lookup. One new port (`MdnsResolver`) and one new application class (`BrokerAddressResolver`) add a "try mDNS, fall back to manual config" policy ahead of the existing `MqttLink::begin()` call.

**Tech Stack:** C++17, PlatformIO (`native` Catch2 tests, `esp32dev` Arduino/ESP32 build), PubSubClient (MQTT), ESPmDNS (Arduino-ESP32 core, bundled — no `lib_deps` entry needed).

**Design doc:** `docs/superpowers/specs/2026-09-06-esp32-loco2mqtt-turnout-bridge-design.md` — read it for the full rationale; this plan is the executable breakdown of it.

## Global Constraints

- Domain and application code must compile under `native` without `Arduino.h`. Only `#ifdef ARDUINO`-guarded adapter files may include Arduino/ESP32 headers.
- No STL restriction on this target (McsEsp32 runs on ESP32, which has real libstdc++) — `std::string`, `std::array`, `std::optional`, `std::deque` are all fine here, unlike the AVR (`McsCore`)/Mega path.
- Within a library, includes stay relative (`../domain/X.h`). A cross-library include (into `McsCore`) uses a rooted form (`"ports/X.h"`, `"domain/X.h"`) — follow the existing pattern, don't invent a new one.
- Run `pio test -e native` (and any `-f <suite>` filtered run) via **Bash/Git Bash, not PowerShell** — PowerShell on this Windows machine produces false `ERRORED` results on commits that pass cleanly under Bash (a MinGW runtime DLL/PATH mismatch, not a real failure).
- `pio run -e esp32dev` is a **build-check only** for `#ifdef ARDUINO`-guarded files — there is no way to unit-test them natively. This mirrors the existing convention for `MqttLink`/`WiFiLink`/`NvsConfigStore`.
- LocoNet turnout address range is **1–2048**; `0` is the "channel unconfigured" sentinel (replacing the old empty-string sentinel).
- Loco2MQTT's payload words are exactly `CLOSED`/`THROWN` (already what `PayloadCodec` produces — do not change `PayloadCodec`).
- Loco2MQTT's topics are `loconet/turnout/<address>/set` (command) and `loconet/turnout/<address>/state` (feedback).
- Every commit in this repo goes through the **`arlo-commits` skill**, never a raw `git commit` — each task's final step says "commit via arlo-commits" rather than giving a literal `git commit` command.
- Prefer `git mv` over delete+recreate when a file/directory is being renamed with modified content, to preserve history (matches this project's own prior-slice convention).
- No comments in code unless explaining a non-obvious "why" — none of the code in this plan needs any; don't add any.

---

### Task 1: `TopicScheme` — address-based topics

**Files:**
- Modify: `lib/McsEsp32/src/domain/TopicScheme.h`
- Test: `test/test_topic_scheme/test_main.cpp`

**Interfaces:**
- Produces: `TopicScheme::topicFor(int address) -> std::string`, `TopicScheme::stateTopicFor(int address) -> std::string`. Every later task that needs a Loco2MQTT topic string uses these two static methods.

- [ ] **Step 1: Replace the test file with address-based expectations**

Replace the full contents of `test/test_topic_scheme/test_main.cpp` with:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/TopicScheme.h"

TEST_CASE("topicFor builds the expected command topic")
{
    REQUIRE(TopicScheme::topicFor(5) == "loconet/turnout/5/set");
}

TEST_CASE("topicFor handles a different address")
{
    REQUIRE(TopicScheme::topicFor(2048) == "loconet/turnout/2048/set");
}

TEST_CASE("stateTopicFor builds the expected state topic")
{
    REQUIRE(TopicScheme::stateTopicFor(5) == "loconet/turnout/5/state");
}

TEST_CASE("stateTopicFor differs from topicFor for the same address")
{
    REQUIRE(TopicScheme::stateTopicFor(17) != TopicScheme::topicFor(17));
}
```

- [ ] **Step 2: Run the test to see it fail to compile**

Run (in Bash/Git Bash): `pio test -e native -f test_topic_scheme`
Expected: build FAILS — `TopicScheme::topicFor(int)` doesn't exist yet (current signature takes `const std::string&`).

- [ ] **Step 3: Update `TopicScheme.h`**

Replace the full contents of `lib/McsEsp32/src/domain/TopicScheme.h` with:

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

- [ ] **Step 4: Run the test to see it pass**

Run: `pio test -e native -f test_topic_scheme`
Expected: PASS (4 test cases).

- [ ] **Step 5: Commit**

Commit via the `arlo-commits` skill (message should reflect: swap `TopicScheme` from JMRI-name to Loco2MQTT-address topics).

---

### Task 2: `NodeConfig` — integer turnout addresses

**Files:**
- Modify: `lib/McsEsp32/src/domain/NodeConfig.h`
- Modify: `lib/McsEsp32/src/domain/NodeConfig.cpp`
- Test: `test/test_node_config/test_main.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `NodeConfig::channelTurnoutAddresses` (`std::array<int, NodeConfig::kChannelCount>`, `0` = unconfigured), `NodeConfig::withChannelAddress(int channel, int address) -> NodeConfig`, `NodeConfig::kMinTurnoutAddress` (`1`), `NodeConfig::kMaxTurnoutAddress` (`2048`). Every later task touching per-channel config (`CommissioningSession`, `WebFormCommissioningAdapter`, `SetupFormRenderer`, `CaptivePortalServer`, `main.cpp`) uses this field/method.

- [ ] **Step 1: Replace the test file**

Replace the full contents of `test/test_node_config/test_main.cpp` with:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/NodeConfig.h"

TEST_CASE("Factory-default NodeConfig fails validation")
{
    const NodeConfig config = NodeConfig::factoryDefault();

    REQUIRE(config.nodeId == 0);
    REQUIRE(config.wifiSsid.empty());
    REQUIRE(config.brokerHost.empty());
    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("withNodeId returns a modified copy without mutating the original")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withNodeId(5);

    REQUIRE(updated.nodeId == 5);
    REQUIRE(original.nodeId == 0);
}

TEST_CASE("withWifi returns a modified copy without mutating the original")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withWifi("MyLayoutWifi", "hunter2");

    REQUIRE(updated.wifiSsid == "MyLayoutWifi");
    REQUIRE(updated.wifiPassword == "hunter2");
    REQUIRE(original.wifiSsid.empty());
}

TEST_CASE("withBroker returns a modified copy without mutating the original")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withBroker("192.168.1.50", 1883);

    REQUIRE(updated.brokerHost == "192.168.1.50");
    REQUIRE(updated.brokerPort == 1883);
    REQUIRE(original.brokerHost.empty());
}

TEST_CASE("withChannelAddress sets the addressed channel (1-based) without mutating the original")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withChannelAddress(1, 5);

    REQUIRE(updated.channelTurnoutAddresses[0] == 5);
    REQUIRE(original.channelTurnoutAddresses[0] == 0);
}

TEST_CASE("withChannelAddress ignores an out-of-range channel number")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withChannelAddress(13, 5);

    for (const int address : updated.channelTurnoutAddresses)
    {
        REQUIRE(address == 0);
    }
}

TEST_CASE("A fully valid config passes validation")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883);

    REQUIRE(config.validate().empty());
}

TEST_CASE("A valid config with only some channels addressed still passes validation")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelAddress(1, 5)
                                   .withChannelAddress(2, 6);

    REQUIRE(config.validate().empty());
}

TEST_CASE("validate rejects a node id outside 1-99")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(0)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate rejects an empty wifi ssid")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withBroker("192.168.1.50", 1883);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate rejects an empty broker host")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2");

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate rejects a broker port outside 1-65535")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 0);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate rejects two channels claiming the same turnout address")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelAddress(1, 5)
                                   .withChannelAddress(2, 5);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate accepts the address range boundaries 1 and 2048")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelAddress(1, 1)
                                   .withChannelAddress(2, 2048);

    REQUIRE(config.validate().empty());
}

TEST_CASE("validate rejects a channel address below 1")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelAddress(1, -1);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate rejects a channel address above 2048")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelAddress(1, 2049);

    REQUIRE_FALSE(config.validate().empty());
}
```

- [ ] **Step 2: Run the test to see it fail to compile**

Run: `pio test -e native -f test_node_config`
Expected: build FAILS — `channelTurnoutAddresses`/`withChannelAddress` don't exist yet.

- [ ] **Step 3: Update `NodeConfig.h`**

Replace the full contents of `lib/McsEsp32/src/domain/NodeConfig.h` with:

```cpp
#pragma once

#include <array>
#include <string>
#include <vector>

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
    std::array<int, kChannelCount> channelTurnoutAddresses{};

    static NodeConfig factoryDefault();

    [[nodiscard]] NodeConfig withNodeId(int id) const;
    [[nodiscard]] NodeConfig withWifi(std::string ssid, std::string password) const;
    [[nodiscard]] NodeConfig withBroker(std::string host, int port) const;
    [[nodiscard]] NodeConfig withChannelAddress(int channel, int address) const;

    [[nodiscard]] std::vector<std::string> validate() const;
};
```

- [ ] **Step 4: Update `NodeConfig.cpp`**

Replace the full contents of `lib/McsEsp32/src/domain/NodeConfig.cpp` with:

```cpp
#include "NodeConfig.h"

NodeConfig NodeConfig::factoryDefault()
{
    return NodeConfig{};
}

NodeConfig NodeConfig::withNodeId(const int id) const
{
    NodeConfig copy = *this;
    copy.nodeId = id;
    return copy;
}

NodeConfig NodeConfig::withWifi(std::string ssid, std::string password) const
{
    NodeConfig copy = *this;
    copy.wifiSsid = std::move(ssid);
    copy.wifiPassword = std::move(password);
    return copy;
}

NodeConfig NodeConfig::withBroker(std::string host, const int port) const
{
    NodeConfig copy = *this;
    copy.brokerHost = std::move(host);
    copy.brokerPort = port;
    return copy;
}

NodeConfig NodeConfig::withChannelAddress(const int channel, const int address) const
{
    NodeConfig copy = *this;
    if (channel >= 1 && channel <= kChannelCount)
    {
        copy.channelTurnoutAddresses[channel - 1] = address;
    }
    return copy;
}

std::vector<std::string> NodeConfig::validate() const
{
    std::vector<std::string> errors;

    if (nodeId < kMinNodeId || nodeId > kMaxNodeId)
    {
        errors.push_back("node id must be between " + std::to_string(kMinNodeId) +
                          " and " + std::to_string(kMaxNodeId));
    }

    if (wifiSsid.empty())
    {
        errors.push_back("wifi ssid must not be empty");
    }

    if (brokerHost.empty())
    {
        errors.push_back("broker host must not be empty");
    }

    if (brokerPort < kMinBrokerPort || brokerPort > kMaxBrokerPort)
    {
        errors.push_back("broker port must be between " + std::to_string(kMinBrokerPort) +
                          " and " + std::to_string(kMaxBrokerPort));
    }

    for (int i = 0; i < kChannelCount; ++i)
    {
        const int address = channelTurnoutAddresses[i];
        if (address == 0)
        {
            continue;
        }
        if (address < kMinTurnoutAddress || address > kMaxTurnoutAddress)
        {
            errors.push_back("channel " + std::to_string(i + 1) + " address must be between " +
                              std::to_string(kMinTurnoutAddress) + " and " + std::to_string(kMaxTurnoutAddress));
        }
        for (int j = i + 1; j < kChannelCount; ++j)
        {
            if (channelTurnoutAddresses[j] == address)
            {
                errors.push_back("channels " + std::to_string(i + 1) + " and " +
                                  std::to_string(j + 1) + " both claim turnout address " +
                                  std::to_string(address));
            }
        }
    }

    return errors;
}
```

- [ ] **Step 5: Run the test to see it pass**

Run: `pio test -e native -f test_node_config`
Expected: PASS (15 test cases).

- [ ] **Step 6: Commit**

Commit via the `arlo-commits` skill (message should reflect: `NodeConfig` per-channel field becomes an int LocoNet address with range/duplicate validation, replacing the JMRI-name string).

---

### Task 3: `ParsedCommand` + `CommandLineParser` — `turnout <n> address <addr>`

**Files:**
- Modify: `lib/McsEsp32/src/domain/ParsedCommand.h`
- Modify: `lib/McsEsp32/src/domain/CommandLineParser.cpp`
- Test: `test/test_command_line_parser/test_main.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `CommandKind::TurnoutAddress` (replaces `CommandKind::TurnoutName`). A parsed `turnout <n> address <addr>` command has `kind == CommandKind::TurnoutAddress`, `intArg == <n>` (channel), `intArg2 == <addr>`. `CommissioningSession` (Task 8) consumes this.

- [ ] **Step 1: Replace the test file**

Replace the full contents of `test/test_command_line_parser/test_main.cpp` with:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/CommandLineParser.h"

TEST_CASE("parses id command")
{
    const ParsedCommand command = CommandLineParser::parse("id 5");

    REQUIRE(command.kind == CommandKind::Id);
    REQUIRE(command.intArg == 5);
}

TEST_CASE("id command with a non-numeric argument is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("id abc");

    REQUIRE(command.kind == CommandKind::Invalid);
    REQUIRE_FALSE(command.errorMessage.empty());
}

TEST_CASE("id command with no argument is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("id");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("parses wifi command")
{
    const ParsedCommand command = CommandLineParser::parse("wifi MyLayoutWifi hunter2");

    REQUIRE(command.kind == CommandKind::Wifi);
    REQUIRE(command.stringArg1 == "MyLayoutWifi");
    REQUIRE(command.stringArg2 == "hunter2");
}

TEST_CASE("wifi command with a missing password is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("wifi MyLayoutWifi");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("parses broker command")
{
    const ParsedCommand command = CommandLineParser::parse("broker 192.168.1.50 1883");

    REQUIRE(command.kind == CommandKind::Broker);
    REQUIRE(command.stringArg1 == "192.168.1.50");
    REQUIRE(command.intArg2 == 1883);
}

TEST_CASE("broker command with a non-numeric port is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("broker 192.168.1.50 abc");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("parses turnout address command")
{
    const ParsedCommand command = CommandLineParser::parse("turnout 3 address 17");

    REQUIRE(command.kind == CommandKind::TurnoutAddress);
    REQUIRE(command.intArg == 3);
    REQUIRE(command.intArg2 == 17);
}

TEST_CASE("turnout command missing the address keyword is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("turnout 3 17");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("turnout command with a non-numeric channel is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("turnout x address 17");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("turnout command with a non-numeric address is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("turnout 3 address xyz");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("parses show command")
{
    const ParsedCommand command = CommandLineParser::parse("show");

    REQUIRE(command.kind == CommandKind::Show);
}

TEST_CASE("parses save command")
{
    const ParsedCommand command = CommandLineParser::parse("save");

    REQUIRE(command.kind == CommandKind::Save);
}

TEST_CASE("parses reboot command")
{
    const ParsedCommand command = CommandLineParser::parse("reboot");

    REQUIRE(command.kind == CommandKind::Reboot);
}

TEST_CASE("unknown command is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("frobnicate");

    REQUIRE(command.kind == CommandKind::Invalid);
    REQUIRE_FALSE(command.errorMessage.empty());
}

TEST_CASE("empty line is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("blank line (whitespace only) is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("   ");

    REQUIRE(command.kind == CommandKind::Invalid);
}
```

- [ ] **Step 2: Run the test to see it fail to compile**

Run: `pio test -e native -f test_command_line_parser`
Expected: build FAILS — `CommandKind::TurnoutAddress` doesn't exist yet.

- [ ] **Step 3: Update `ParsedCommand.h`**

In `lib/McsEsp32/src/domain/ParsedCommand.h`, change the enum value `TurnoutName` to `TurnoutAddress`:

```cpp
enum class CommandKind
{
    Id,
    Wifi,
    Broker,
    TurnoutAddress,
    Show,
    Save,
    Reboot,
    Invalid
};
```

(The rest of the file — the `ParsedCommand` struct itself, including `intArg2` — is unchanged.)

- [ ] **Step 4: Update `CommandLineParser.cpp`**

In `lib/McsEsp32/src/domain/CommandLineParser.cpp`, replace the `if (verb == "turnout")` block with:

```cpp
    if (verb == "turnout")
    {
        if (tokens.size() != 4 || tokens[2] != "address")
        {
            return invalid("usage: turnout <n> address <address>");
        }
        int channel = 0;
        if (!parseInt(tokens[1], channel))
        {
            return invalid("turnout channel must be a number");
        }
        int address = 0;
        if (!parseInt(tokens[3], address))
        {
            return invalid("turnout address must be a number");
        }
        ParsedCommand command;
        command.kind = CommandKind::TurnoutAddress;
        command.intArg = channel;
        command.intArg2 = address;
        return command;
    }
```

- [ ] **Step 5: Run the test to see it pass**

Run: `pio test -e native -f test_command_line_parser`
Expected: PASS (17 test cases).

- [ ] **Step 6: Commit**

Commit via the `arlo-commits` skill (message should reflect: commissioning command syntax changes from `turnout <n> name <jmriName>` to `turnout <n> address <address>`).

---

### Task 4: `MdnsResolver` port + `BrokerAddressResolver` — mDNS-first broker discovery

**Files:**
- Create: `lib/McsEsp32/src/ports/MdnsResolver.h`
- Create: `test/support/FakeMdnsResolver.h`
- Create: `lib/McsEsp32/src/application/BrokerAddressResolver.h`
- Create: `lib/McsEsp32/src/application/BrokerAddressResolver.cpp`
- Test: `test/test_broker_address_resolver/test_main.cpp` (new directory)

**Interfaces:**
- Produces: `MdnsResolver` (pure virtual port, `resolveHost(const std::string& hostname) -> std::optional<std::string>`), `FakeMdnsResolver` (test double, settable `result` field, records `lastRequestedHostname`), `BrokerAddressResolver(MdnsResolver&)` with `resolve(const std::string& mdnsHostname, const std::string& fallbackHost) const -> std::string`. Task 11 (`EspMdnsResolver`) implements the `MdnsResolver` port for real hardware. Task 12 (`main.cpp` wiring) constructs and uses `BrokerAddressResolver`.

- [ ] **Step 1: Create the port**

Create `lib/McsEsp32/src/ports/MdnsResolver.h`:

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

- [ ] **Step 2: Create the fake test double**

Create `test/support/FakeMdnsResolver.h`:

```cpp
#pragma once

#include <optional>
#include <string>

#include "ports/MdnsResolver.h"

class FakeMdnsResolver final : public MdnsResolver
{
public:
    std::optional<std::string> result;
    std::string lastRequestedHostname;

    std::optional<std::string> resolveHost(const std::string& hostname) override
    {
        lastRequestedHostname = hostname;
        return result;
    }
};
```

- [ ] **Step 3: Write the failing test**

Create `test/test_broker_address_resolver/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "application/BrokerAddressResolver.h"
#include "support/FakeMdnsResolver.h"

TEST_CASE("resolve returns the mDNS result when the resolver finds one")
{
    FakeMdnsResolver resolver;
    resolver.result = "192.168.1.77";
    BrokerAddressResolver brokerAddressResolver(resolver);

    const std::string host = brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(host == "192.168.1.77");
}

TEST_CASE("resolve falls back to the given host when the resolver finds nothing")
{
    FakeMdnsResolver resolver;
    resolver.result = std::nullopt;
    BrokerAddressResolver brokerAddressResolver(resolver);

    const std::string host = brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(host == "192.168.1.50");
}

TEST_CASE("resolve queries the exact hostname it was given")
{
    FakeMdnsResolver resolver;
    resolver.result = "192.168.1.77";
    BrokerAddressResolver brokerAddressResolver(resolver);

    brokerAddressResolver.resolve("loco2mqtt", "192.168.1.50");

    REQUIRE(resolver.lastRequestedHostname == "loco2mqtt");
}
```

- [ ] **Step 4: Run the test to see it fail to compile**

Run: `pio test -e native -f test_broker_address_resolver`
Expected: build FAILS — `application/BrokerAddressResolver.h` doesn't exist yet.

- [ ] **Step 5: Implement `BrokerAddressResolver`**

Create `lib/McsEsp32/src/application/BrokerAddressResolver.h`:

```cpp
#pragma once

#include <string>

#include "../ports/MdnsResolver.h"

class BrokerAddressResolver
{
public:
    explicit BrokerAddressResolver(MdnsResolver& resolver);

    [[nodiscard]] std::string resolve(const std::string& mdnsHostname, const std::string& fallbackHost) const;

private:
    MdnsResolver& resolver_;
};
```

Create `lib/McsEsp32/src/application/BrokerAddressResolver.cpp`:

```cpp
#include "BrokerAddressResolver.h"

BrokerAddressResolver::BrokerAddressResolver(MdnsResolver& resolver) : resolver_(resolver)
{
}

std::string BrokerAddressResolver::resolve(const std::string& mdnsHostname, const std::string& fallbackHost) const
{
    const std::optional<std::string> resolved = resolver_.resolveHost(mdnsHostname);
    return resolved.value_or(fallbackHost);
}
```

- [ ] **Step 6: Run the test to see it pass**

Run: `pio test -e native -f test_broker_address_resolver`
Expected: PASS (3 test cases).

- [ ] **Step 7: Commit**

Commit via the `arlo-commits` skill (message should reflect: add `MdnsResolver` port and `BrokerAddressResolver`'s mDNS-first-manual-fallback policy).

---

### Task 5: `Loco2MqttTurnoutCommandAdapter` (renamed, retyped from `JmriTurnoutCommandAdapter`)

**Files:**
- Rename+modify (via `git mv` then edit): `lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.h` → `lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.h`
- Rename+modify: `lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.cpp` → `lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.cpp`
- Rename+modify (directory, via `git mv`): `test/test_jmri_turnout_command_adapter/` → `test/test_loco2mqtt_turnout_command_adapter/`

**Interfaces:**
- Consumes: `TopicScheme::topicFor(int)` (Task 1), `NodeConfig::kChannelCount`/`channelTurnoutAddresses` shape (Task 2), `PayloadCodec::encode` (unchanged), `TurnoutCommandPort` (unchanged), `MqttTransport` (unchanged).
- Produces: `Loco2MqttTurnoutCommandAdapter(MqttTransport&, const std::array<int, NodeConfig::kChannelCount>&)`, implementing `TurnoutCommandPort::send(int channel, TurnoutPosition)`. Task 7 (integration test) and Task 12 (`main.cpp` wiring) construct this.

- [ ] **Step 1: Rename the source files**

```bash
git mv lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.h lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.h
git mv lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.cpp lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.cpp
git mv test/test_jmri_turnout_command_adapter test/test_loco2mqtt_turnout_command_adapter
```

- [ ] **Step 2: Replace the test file with the failing (address-based) version**

Replace the full contents of `test/test_loco2mqtt_turnout_command_adapter/test_main.cpp` with:

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

- [ ] **Step 3: Run the test to see it fail to compile**

Run: `pio test -e native -f test_loco2mqtt_turnout_command_adapter`
Expected: build FAILS — the class is still named `JmriTurnoutCommandAdapter` and keyed by string.

- [ ] **Step 4: Update the header**

Replace the full contents of `lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.h` with:

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

- [ ] **Step 5: Update the implementation**

Replace the full contents of `lib/McsEsp32/src/adapters/Loco2MqttTurnoutCommandAdapter.cpp` with:

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

(`send`'s `address` parameter is the 1–12 panel *channel*, not a LocoNet address — same channel-vs-address split the JMRI adapter it replaces already had; `turnoutAddress` looked up from `channelTurnoutAddresses_` is the real LocoNet address.)

- [ ] **Step 6: Run the test to see it pass**

Run: `pio test -e native -f test_loco2mqtt_turnout_command_adapter`
Expected: PASS (5 test cases).

- [ ] **Step 7: Commit**

Commit via the `arlo-commits` skill (message should reflect: rename `JmriTurnoutCommandAdapter` to `Loco2MqttTurnoutCommandAdapter`, re-key by LocoNet address).

---

### Task 6: `Loco2MqttFeedbackSource` (renamed, retyped from `JmriFeedbackSource`)

**Files:**
- Rename+modify: `lib/McsEsp32/src/adapters/JmriFeedbackSource.h` → `lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.h`
- Rename+modify: `lib/McsEsp32/src/adapters/JmriFeedbackSource.cpp` → `lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.cpp`
- Rename+modify (directory): `test/test_jmri_feedback_source/` → `test/test_loco2mqtt_feedback_source/`

**Interfaces:**
- Consumes: `TopicScheme::stateTopicFor(int)` (Task 1), `NodeConfig::kChannelCount`/`channelTurnoutAddresses` shape (Task 2), `PayloadCodec::decode` (unchanged), `MqttTransport` (unchanged), `TurnoutFeedback` (unchanged, from `ports/TurnoutCommandPort.h`).
- Produces: `Loco2MqttFeedbackSource(MqttTransport&, const std::array<int, NodeConfig::kChannelCount>&)`, `poll(TurnoutFeedback&) -> bool`. Task 7 and Task 12 construct this.

- [ ] **Step 1: Rename the source files**

```bash
git mv lib/McsEsp32/src/adapters/JmriFeedbackSource.h lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.h
git mv lib/McsEsp32/src/adapters/JmriFeedbackSource.cpp lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.cpp
git mv test/test_jmri_feedback_source test/test_loco2mqtt_feedback_source
```

- [ ] **Step 2: Replace the test file with the failing (address-based) version**

Replace the full contents of `test/test_loco2mqtt_feedback_source/test_main.cpp` with:

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

- [ ] **Step 3: Run the test to see it fail to compile**

Run: `pio test -e native -f test_loco2mqtt_feedback_source`
Expected: build FAILS — the class is still named `JmriFeedbackSource` and keyed by string.

- [ ] **Step 4: Update the header**

Replace the full contents of `lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.h` with:

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

- [ ] **Step 5: Update the implementation**

Replace the full contents of `lib/McsEsp32/src/adapters/Loco2MqttFeedbackSource.cpp` with:

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

- [ ] **Step 6: Run the test to see it pass**

Run: `pio test -e native -f test_loco2mqtt_feedback_source`
Expected: PASS (7 test cases).

- [ ] **Step 7: Commit**

Commit via the `arlo-commits` skill (message should reflect: rename `JmriFeedbackSource` to `Loco2MqttFeedbackSource`, re-key by LocoNet address).

---

### Task 7: End-to-end wiring test (renamed, retyped from `test_jmri_turnout_wiring`)

**Files:**
- Rename+modify (directory): `test/test_jmri_turnout_wiring/` → `test/test_loco2mqtt_turnout_wiring/`

**Interfaces:**
- Consumes: `Loco2MqttTurnoutCommandAdapter` (Task 5), `Loco2MqttFeedbackSource` (Task 6), and the unchanged `TurnoutControl`/`Button`/`Turnout`/`TurnoutIndicator`/`Indicator` domain classes plus `FakeClock`/`FakeDigitalInput`/`FakeDigitalOutput`/`FakeTurnoutCommandPort`/`FakeMqttTransport` test doubles.
- Produces: nothing consumed by later tasks — this is a top-of-stack regression test.

- [ ] **Step 1: Rename the directory**

```bash
git mv test/test_jmri_turnout_wiring test/test_loco2mqtt_turnout_wiring
```

- [ ] **Step 2: Replace the test file**

Replace the full contents of `test/test_loco2mqtt_turnout_wiring/test_main.cpp` with:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/Loco2MqttFeedbackSource.h"
#include "adapters/Loco2MqttTurnoutCommandAdapter.h"
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
    constexpr int TURNOUT_ADDRESS = 5;

    std::array<int, NodeConfig::kChannelCount> addressesWithChannel(int channel, int address)
    {
        std::array<int, NodeConfig::kChannelCount> addresses{};
        addresses[channel - 1] = address;
        return addresses;
    }

    void pressButton(Button& button, FakeDigitalInput& input, FakeClock& clock)
    {
        input.active = true;
        button.update();
        clock.advanceBy(DEBOUNCE_MS);
        button.update();
    }
}

TEST_CASE("a button press publishes a command on the set topic that is not picked up as feedback")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(CHANNEL, TURNOUT_ADDRESS);
    Loco2MqttTurnoutCommandAdapter commandAdapter(transport, addresses);
    Loco2MqttFeedbackSource feedbackSource(transport, addresses);

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

    Turnout turnout(CHANNEL, "T1", TurnoutPosition::Closed, false, false);
    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandAdapter);

    pressButton(throwButton, throwInput, clock);
    control.update();

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "loconet/turnout/5/set");
    REQUIRE(transport.published[0].payload == "THROWN");

    // The command and state topics are structurally distinct
    // (".../set" vs ".../state") - even delivering the exact outgoing
    // command message back on its own topic must never be picked up as
    // feedback, since the feedback source only ever subscribed the state
    // topic.
    transport.deliver(transport.published[0].topic, transport.published[0].payload);

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(feedbackSource.poll(feedback));
    REQUIRE(turnout.position() == TurnoutPosition::Closed);
    REQUIRE_FALSE(thrownIndicator.isOn());
}

TEST_CASE("Loco2MQTT's real state-topic confirmation updates the turnout and indicator")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(CHANNEL, TURNOUT_ADDRESS);
    Loco2MqttFeedbackSource feedbackSource(transport, addresses);

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

    Turnout turnout(CHANNEL, "T1", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;
    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    transport.deliver("loconet/turnout/5/state", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE(feedbackSource.poll(feedback));
    REQUIRE(feedback.address == CHANNEL);
    REQUIRE(feedback.position == TurnoutPosition::Thrown);

    control.applyFeedback(feedback);

    REQUIRE(turnout.position() == TurnoutPosition::Thrown);
    REQUIRE(thrownIndicator.isOn());
    REQUIRE_FALSE(closedIndicator.isOn());
    REQUIRE(commandPort.sentCommands.empty());
}

TEST_CASE("a repeated identical state re-publish (Loco2MQTT's 30s heartbeat) is a harmless no-op")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(CHANNEL, TURNOUT_ADDRESS);
    Loco2MqttFeedbackSource feedbackSource(transport, addresses);

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

    Turnout turnout(CHANNEL, "T1", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;
    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    transport.deliver("loconet/turnout/5/state", "THROWN");
    TurnoutFeedback first{};
    REQUIRE(feedbackSource.poll(first));
    control.applyFeedback(first);

    transport.deliver("loconet/turnout/5/state", "THROWN");
    TurnoutFeedback second{};
    REQUIRE(feedbackSource.poll(second));
    control.applyFeedback(second);

    REQUIRE(turnout.position() == TurnoutPosition::Thrown);
    REQUIRE(thrownIndicator.isOn());
    REQUIRE_FALSE(closedIndicator.isOn());
}
```

- [ ] **Step 3: Run the test**

Run: `pio test -e native -f test_loco2mqtt_turnout_wiring`
Expected: PASS (3 test cases). (This is a pure rename+retype of already-working logic, so this should go green immediately once it compiles — no separate "watch it fail" step is meaningful here beyond the rename itself no longer compiling against the old `Jmri*` names, which Task 5/6 already proved out.)

- [ ] **Step 4: Commit**

Commit via the `arlo-commits` skill (message should reflect: rename the JMRI end-to-end wiring test to Loco2MQTT, add heartbeat-repeat regression coverage).

---

### Task 8: `CommissioningSession` — `TurnoutAddress` command handling

**Files:**
- Modify: `lib/McsEsp32/src/application/CommissioningSession.cpp`
- Test: `test/test_commissioning_session/test_main.cpp`

**Interfaces:**
- Consumes: `CommandKind::TurnoutAddress` (Task 3), `NodeConfig::withChannelAddress`/`channelTurnoutAddresses` (Task 2).
- Produces: `CommissioningSession::apply()` handling `CommandKind::TurnoutAddress` (validates channel 1–12, calls `withChannelAddress`); `formatShow()` printing `turnout N: <address>` or `turnout N: (unconfigured)`. Task 9 (`WebFormCommissioningAdapter`) relies on this behavior.

- [ ] **Step 1: Update the two affected test cases and add none new (existing suite otherwise stands)**

In `test/test_commissioning_session/test_main.cpp`, replace the test case `"show reports a configured turnout name"` with:

```cpp
TEST_CASE("show reports a configured turnout address")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand addressCommand;
    addressCommand.kind = CommandKind::TurnoutAddress;
    addressCommand.intArg = 1;
    addressCommand.intArg2 = 5;
    session.apply(addressCommand);

    ParsedCommand showCommand;
    showCommand.kind = CommandKind::Show;
    const std::string response = session.apply(showCommand);

    REQUIRE(response.find("turnout 1: 5") != std::string::npos);
}
```

Replace the test case `"an out-of-range turnout channel reports an error and stores nothing"` with:

```cpp
TEST_CASE("an out-of-range turnout channel reports an error and stores nothing")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand addressCommand;
    addressCommand.kind = CommandKind::TurnoutAddress;
    addressCommand.intArg = 13;
    addressCommand.intArg2 = 99;

    const std::string response = session.apply(addressCommand);

    REQUIRE(response.rfind("error:", 0) == 0);

    ParsedCommand showCommand;
    showCommand.kind = CommandKind::Show;
    const std::string showResponse = session.apply(showCommand);

    REQUIRE(showResponse.find("99") == std::string::npos);
}
```

(Every other test case in this file is unaffected and stays as-is.)

- [ ] **Step 2: Run the test to see it fail to compile**

Run: `pio test -e native -f test_commissioning_session`
Expected: build FAILS — `CommandKind::TurnoutName` no longer exists (Task 3 already renamed it), and `apply()` doesn't yet handle `TurnoutAddress`.

- [ ] **Step 3: Update `CommissioningSession.cpp`**

In `lib/McsEsp32/src/application/CommissioningSession.cpp`, replace the `formatShow()` channel loop:

```cpp
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const int address = draft_.channelTurnoutAddresses[i];
        result += "turnout " + std::to_string(i + 1) + ": " +
                  (address == 0 ? std::string("(unconfigured)") : std::to_string(address)) + "\n";
    }
```

And replace the `case CommandKind::TurnoutName:` block in `apply()` with:

```cpp
    case CommandKind::TurnoutAddress:
        if (command.intArg < 1 || command.intArg > NodeConfig::kChannelCount)
        {
            return "error: turnout channel must be between 1 and " +
                   std::to_string(NodeConfig::kChannelCount) + "\n";
        }
        draft_ = draft_.withChannelAddress(command.intArg, command.intArg2);
        return "OK\n";
```

- [ ] **Step 4: Run the test to see it pass**

Run: `pio test -e native -f test_commissioning_session`
Expected: PASS (all 12 test cases).

- [ ] **Step 5: Commit**

Commit via the `arlo-commits` skill (message should reflect: `CommissioningSession` handles `turnout <n> address <addr>` instead of `turnout <n> name <jmriName>`).

---

### Task 9: `WebFormSubmission` + `WebFormCommissioningAdapter` — address field, normal-parser routing

**Files:**
- Modify: `lib/McsEsp32/src/domain/WebFormSubmission.h`
- Modify: `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp`
- Test: `test/test_web_form_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes: `NodeConfig::channelTurnoutAddresses` (Task 2), `CommandKind::TurnoutAddress` handling (Task 8).
- Produces: `WebFormSubmission::channelTurnoutAddresses` (`std::array<std::string, NodeConfig::kChannelCount>`, raw form text, blank = unconfigured). Task 10 (`SetupFormRenderer`) and Task 12 (`CaptivePortalServer`) consume this field name.

- [ ] **Step 1: Replace the test file**

Replace the full contents of `test/test_web_form_commissioning_adapter/test_main.cpp` with:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/WebFormCommissioningAdapter.h"
#include "application/CommissioningSession.h"
#include "domain/NodeConfig.h"
#include "domain/WebFormSubmission.h"
#include "support/FakeConfigStore.h"

namespace
{
    WebFormSubmission validSubmission()
    {
        WebFormSubmission form;
        form.nodeId = "5";
        form.wifiSsid = "MyLayoutWifi";
        form.wifiPassword = "hunter2";
        form.brokerHost = "192.168.1.50";
        form.brokerPort = "1883";
        form.channelTurnoutAddresses[0] = "17";
        return form;
    }
}

TEST_CASE("a fully valid submission saves and requests reboot")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const std::string response = adapter.submit(validSubmission());

    REQUIRE(response == "rebooting\n");
    REQUIRE(adapter.rebootRequested());
    REQUIRE(store.saveCount == 1);
    REQUIRE(store.load().nodeId == 5);
    REQUIRE(store.load().wifiSsid == "MyLayoutWifi");
    REQUIRE(store.load().channelTurnoutAddresses[0] == 17);
}

TEST_CASE("a blank channel field clears a previously stored channel address")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("w", "p")
                   .withBroker("h", 1883)
                   .withChannelAddress(2, 6));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    // form.channelTurnoutAddresses[1] (channel 2) is blank by default construction

    adapter.submit(form);

    REQUIRE(store.load().channelTurnoutAddresses[1] == 0);
}

TEST_CASE("a non-numeric node id stops immediately and never reaches save")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    form.nodeId = "not-a-number";

    const std::string response = adapter.submit(form);

    REQUIRE(response != "rebooting\n");
    REQUIRE_FALSE(adapter.rebootRequested());
    REQUIRE(store.saveCount == 0);
}

TEST_CASE("a non-numeric turnout address stops immediately and never reaches save")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    form.channelTurnoutAddresses[0] = "not-a-number";

    const std::string response = adapter.submit(form);

    REQUIRE(response != "rebooting\n");
    REQUIRE_FALSE(adapter.rebootRequested());
    REQUIRE(store.saveCount == 0);
}

TEST_CASE("a save failure is reported and reboot is not requested")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);
    store.failNextSave = true;

    const std::string response = adapter.submit(validSubmission());

    REQUIRE(response == "save failed: could not write to storage\n");
    REQUIRE_FALSE(adapter.rebootRequested());
    REQUIRE(store.saveCount == 0);
}

TEST_CASE("currentValues reflects a previously applied draft change")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    ParsedCommand idCommand;
    idCommand.kind = CommandKind::Id;
    idCommand.intArg = 42;
    session.apply(idCommand);

    const WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.nodeId == "42");
}

TEST_CASE("currentValues reports an unset node id as an empty string")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.nodeId.empty());
}

TEST_CASE("currentValues reports an unset turnout address as an empty string")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.channelTurnoutAddresses[0].empty());
}

TEST_CASE("currentValues reports a configured turnout address as its decimal text")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("w", "p")
                   .withBroker("h", 1883)
                   .withChannelAddress(3, 99));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.channelTurnoutAddresses[2] == "99");
}

TEST_CASE("wifi credentials containing spaces round-trip intact")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    form.wifiSsid = "My Layout Wifi";
    form.wifiPassword = "a pass with spaces";

    const std::string response = adapter.submit(form);

    REQUIRE(response == "rebooting\n");
    REQUIRE(store.load().wifiSsid == "My Layout Wifi");
    REQUIRE(store.load().wifiPassword == "a pass with spaces");
}

TEST_CASE("a blank wifi password keeps the previously stored password")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("MyLayoutWifi", "existing-secret")
                   .withBroker("h", 1883));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    form.wifiPassword = "";

    const std::string response = adapter.submit(form);

    REQUIRE(response == "rebooting\n");
    REQUIRE(store.load().wifiPassword == "existing-secret");
}

TEST_CASE("currentValues never reflects a stored wifi password")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("MyLayoutWifi", "existing-secret")
                   .withBroker("h", 1883));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.wifiPassword.empty());
    REQUIRE(values.wifiSsid == "MyLayoutWifi");
}
```

- [ ] **Step 2: Run the test to see it fail to compile**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: build FAILS — `WebFormSubmission::channelJmriNames` still exists, `channelTurnoutAddresses` doesn't yet.

- [ ] **Step 3: Update `WebFormSubmission.h`**

Replace the full contents of `lib/McsEsp32/src/domain/WebFormSubmission.h` with:

```cpp
#pragma once

#include <array>
#include <string>

#include "NodeConfig.h"

struct WebFormSubmission
{
    std::string nodeId;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string brokerHost;
    std::string brokerPort;
    std::array<std::string, NodeConfig::kChannelCount> channelTurnoutAddresses;
};
```

- [ ] **Step 4: Update `WebFormCommissioningAdapter.cpp`**

Replace the full contents of `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp` with:

```cpp
#include "WebFormCommissioningAdapter.h"

#include "../domain/CommandLineParser.h"
#include "../domain/NodeConfig.h"
#include "../domain/ParsedCommand.h"

WebFormCommissioningAdapter::WebFormCommissioningAdapter(CommissioningSession& session) : session_(session)
{
}

std::string WebFormCommissioningAdapter::submit(const WebFormSubmission& form)
{
    std::string response = session_.apply(CommandLineParser::parse("id " + form.nodeId));
    if (response != "OK\n")
    {
        return response;
    }

    ParsedCommand wifiCommand;
    wifiCommand.kind = CommandKind::Wifi;
    wifiCommand.stringArg1 = form.wifiSsid;
    wifiCommand.stringArg2 = form.wifiPassword.empty() ? session_.draft().wifiPassword : form.wifiPassword;
    response = session_.apply(wifiCommand);
    if (response != "OK\n")
    {
        return response;
    }

    response = session_.apply(CommandLineParser::parse("broker " + form.brokerHost + " " + form.brokerPort));
    if (response != "OK\n")
    {
        return response;
    }

    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string addressText =
            form.channelTurnoutAddresses[i].empty() ? "0" : form.channelTurnoutAddresses[i];
        response = session_.apply(
            CommandLineParser::parse("turnout " + std::to_string(i + 1) + " address " + addressText));
        if (response != "OK\n")
        {
            return response;
        }
    }

    response = session_.apply(CommandLineParser::parse("save"));
    if (response != "saved\n")
    {
        return response;
    }

    return session_.apply(CommandLineParser::parse("reboot"));
}

bool WebFormCommissioningAdapter::rebootRequested() const
{
    return session_.rebootRequested();
}

WebFormSubmission WebFormCommissioningAdapter::currentValues() const
{
    const NodeConfig& config = session_.draft();

    WebFormSubmission form;
    form.nodeId = config.nodeId == 0 ? "" : std::to_string(config.nodeId);
    form.wifiSsid = config.wifiSsid;
    form.wifiPassword = "";
    form.brokerHost = config.brokerHost;
    form.brokerPort = std::to_string(config.brokerPort);
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        form.channelTurnoutAddresses[i] =
            config.channelTurnoutAddresses[i] == 0 ? "" : std::to_string(config.channelTurnoutAddresses[i]);
    }

    return form;
}
```

(Turnout addresses no longer need the quote-handling bypass `WebFormCommissioningAdapter` used for JMRI names — they're bare integers, so they now flow through `CommandLineParser::parse()` like every other field. The `wifi` command keeps its direct-`ParsedCommand` construction, since SSIDs/passwords can still contain spaces `CommandLineParser` can't tokenize.)

- [ ] **Step 5: Run the test to see it pass**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: PASS (all 12 test cases).

- [ ] **Step 6: Commit**

Commit via the `arlo-commits` skill (message should reflect: web-form turnout field becomes a numeric address, routed through the normal command parser instead of a quote-handling bypass).

---

### Task 10: `SetupFormRenderer` + `FirmwareVersion` — address field UI, version display

**Files:**
- Create: `lib/McsEsp32/src/domain/FirmwareVersion.h`
- Modify: `lib/McsEsp32/src/domain/SetupFormRenderer.h`
- Test: `test/test_setup_form_renderer/test_main.cpp`

**Interfaces:**
- Consumes: `WebFormSubmission::channelTurnoutAddresses` (Task 9).
- Produces: `kFirmwareVersion` (`const char*`, currently `"1.0.0"`) — bump by hand for future releases. `SetupFormRenderer::render()`'s HTML output, field names `t{n}_address`. Task 12 (`CaptivePortalServer`) consumes the `t{n}_address` field-name contract.

- [ ] **Step 1: Update the test file**

In `test/test_setup_form_renderer/test_main.cpp`, change the include block at the top from:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/SetupFormRenderer.h"
#include "domain/WifiScanFormatter.h"
```

to:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/FirmwareVersion.h"
#include "domain/SetupFormRenderer.h"
#include "domain/WifiScanFormatter.h"
```

Then replace the test case `"render includes a labeled input for each of the 12 turnout channels"` with:

```cpp
TEST_CASE("render includes a labeled input for each of the 12 turnout channels")
{
    WebFormSubmission form = emptyValues();
    form.channelTurnoutAddresses[0] = "5";
    form.channelTurnoutAddresses[11] = "600";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("Turnout 1 Address") != std::string::npos);
    REQUIRE(html.find("name='t1_address'") != std::string::npos);
    REQUIRE(html.find("value='5'") != std::string::npos);
    REQUIRE(html.find("Turnout 12 Address") != std::string::npos);
    REQUIRE(html.find("name='t12_address'") != std::string::npos);
    REQUIRE(html.find("value='600'") != std::string::npos);
}
```

Then add a new test case anywhere after it:

```cpp
TEST_CASE("render displays the current firmware version")
{
    const std::string html = SetupFormRenderer::render(emptyValues());

    REQUIRE(html.find(kFirmwareVersion) != std::string::npos);
}
```

- [ ] **Step 2: Run the test to see it fail to compile**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: build FAILS — `WebFormSubmission::channelTurnoutAddresses` field name mismatch and `kFirmwareVersion` doesn't exist yet.

- [ ] **Step 3: Create `FirmwareVersion.h`**

Create `lib/McsEsp32/src/domain/FirmwareVersion.h`:

```cpp
#pragma once

// Bump by hand whenever a release worth tracking across the fleet goes out.
inline constexpr const char* kFirmwareVersion = "1.0.0";
```

- [ ] **Step 4: Update `SetupFormRenderer.h`**

Replace the full contents of `lib/McsEsp32/src/domain/SetupFormRenderer.h` with:

```cpp
#pragma once

#include <string>
#include <vector>

#include "FirmwareVersion.h"
#include "NodeConfig.h"
#include "WebFormSubmission.h"
#include "WifiScanFormatter.h"

class SetupFormRenderer
{
public:
    static std::string escapeHtml(const std::string& text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (char c : text)
        {
            switch (c)
            {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&#39;";
                break;
            default:
                escaped += c;
            }
        }
        return escaped;
    }

    static std::string render(const WebFormSubmission& values, const std::vector<ScannedNetwork>& networks = {})
    {
        std::string html;
        html += "<!DOCTYPE html><html><head>";
        html += "<meta charset='utf-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>" + kStyle + "</style></head><body>";
        html += "<div class='card'><h1>MaltBee Panel Setup</h1>";
        html += "<p class='subtitle'>Configure this panel's network settings</p>";
        html += "<p class='version'>Firmware v" + std::string(kFirmwareVersion) + "</p>";
        html += "<form method='POST' action='/submit'>";
        html += "<label>Node ID</label><select name='id'>" + renderIdOptions(values.nodeId) + "</select>";
        html += "<label>WiFi SSID</label>";
        html += "<select onchange=\"document.getElementsByName('wifi_ssid')[0].value=this.value\">"
            + renderNetworkOptions(networks) + "</select>";
        html += "<input name='wifi_ssid' value='" + escapeHtml(values.wifiSsid) + "'>";
        html += "<p class='hint'>Pick a network above, or type one in directly if it's not listed. "
            "<a href='/rescan'>Rescan</a></p>";
        html += "<label>WiFi Password</label><input name='wifi_password' type='password' value=''>";
        html += "<p class='hint'>Leave blank to keep the current password.</p>";
        html += "<label>Broker Host</label><input name='broker_host' value='" + escapeHtml(values.brokerHost) + "'>";
        html += "<label>Broker Port</label><input name='broker_port' type='number' value='"
            + escapeHtml(values.brokerPort) + "'>";
        html += "<details><summary>Turnout Addresses</summary>";
        html += "<p class='warning'>Leave a channel blank to leave it unconfigured.</p>";
        for (int i = 0; i < NodeConfig::kChannelCount; ++i)
        {
            html += renderChannelField(i + 1, values.channelTurnoutAddresses[i]);
        }
        html += "</details>";
        html += "<button type='submit'>Save</button>";
        html += "</form></div></body></html>";
        return html;
    }

private:
    static std::string renderNetworkOptions(const std::vector<ScannedNetwork>& networks)
    {
        std::string options = "<option value='' disabled selected hidden>-- select a nearby network --</option>";
        for (const auto& network : networks)
        {
            const std::string label = WifiScanFormatter::withSignalBars(network.ssid, network.rssi);
            options += "<option value='" + escapeHtml(network.ssid) + "'>" + escapeHtml(label) + "</option>";
        }
        return options;
    }

    static std::string renderIdOptions(const std::string& selectedId)
    {
        std::string options;
        if (selectedId.empty())
        {
            options += "<option value='' disabled selected hidden>-- select --</option>";
        }
        for (int i = NodeConfig::kMinNodeId; i <= NodeConfig::kMaxNodeId; ++i)
        {
            const std::string value = std::to_string(i);
            options += "<option value='" + value + "'" + (value == selectedId ? " selected" : "") + ">" + value
                + "</option>";
        }
        return options;
    }

    static std::string renderChannelField(int channelNumber, const std::string& address)
    {
        const std::string fieldName = "t" + std::to_string(channelNumber) + "_address";
        std::string html;
        html += "<label>Turnout " + std::to_string(channelNumber) + " Address</label>";
        html += "<input name='" + fieldName + "' type='number' value='" + escapeHtml(address) + "'>";
        return html;
    }

    static inline const std::string kStyle =
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
        "background:#f2f4f7;margin:0;padding:24px 16px;color:#1f2933;}"
        ".card{max-width:420px;margin:0 auto;background:#fff;border-radius:12px;"
        "box-shadow:0 1px 3px rgba(0,0,0,0.1);padding:24px;}"
        "h1{font-size:1.25rem;margin:0 0 4px;}"
        ".subtitle{color:#6b7280;font-size:0.875rem;margin:0 0 20px;}"
        ".version{color:#9ca3af;font-size:0.7rem;margin:0 0 12px;}"
        "label{display:block;font-size:0.8rem;font-weight:600;color:#374151;margin:16px 0 4px;}"
        "input,select{width:100%;box-sizing:border-box;padding:8px 10px;border:1px solid #d1d5db;"
        "border-radius:6px;font-size:0.95rem;}"
        "button{margin-top:24px;width:100%;padding:10px;background:#2563eb;color:#fff;"
        "border:none;border-radius:6px;font-size:1rem;font-weight:600;cursor:pointer;}"
        "button:hover{background:#1d4ed8;}"
        "details{margin-top:20px;}"
        "summary{cursor:pointer;font-size:0.85rem;font-weight:600;color:#374151;}"
        ".warning{color:#b45309;font-size:0.8rem;margin:8px 0 0;}"
        ".hint{color:#6b7280;font-size:0.8rem;margin:4px 0 0;}";
};
```

- [ ] **Step 5: Run the test to see it pass**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: PASS (all test cases, including the two new/changed ones).

- [ ] **Step 6: Commit**

Commit via the `arlo-commits` skill (message should reflect: setup web page shows numeric turnout-address fields and the firmware version).

---

### Task 11: `EspMdnsResolver` — hardware adapter (build-check only)

**Files:**
- Create: `lib/McsEsp32/src/adapters/EspMdnsResolver.h`
- Create: `lib/McsEsp32/src/adapters/EspMdnsResolver.cpp`

**Interfaces:**
- Consumes: `MdnsResolver` port (Task 4).
- Produces: `EspMdnsResolver` implementing `MdnsResolver::resolveHost()` against real hardware (WiFi status + `ESPmDNS`). Task 12 (`main.cpp` wiring) constructs this.

No native test is possible here — this file is `#ifdef ARDUINO`-guarded and depends on ESP32 WiFi/mDNS hardware state, same as `MqttLink`/`WiFiLink`. Verification is a build check, done together with Task 12's full wiring (both touch `main.cpp`'s includes/globals, so there's no value in a separate esp32dev build check here that Task 12 will immediately repeat).

- [ ] **Step 1: Create the header**

Create `lib/McsEsp32/src/adapters/EspMdnsResolver.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <optional>
#include <string>

#include "ports/MdnsResolver.h"

class EspMdnsResolver final : public MdnsResolver
{
public:
    std::optional<std::string> resolveHost(const std::string& hostname) override;

private:
    static constexpr unsigned long kWifiWaitTimeoutMs = 8000;
    bool mdnsStarted_ = false;
};

#endif
```

- [ ] **Step 2: Create the implementation**

Create `lib/McsEsp32/src/adapters/EspMdnsResolver.cpp`:

```cpp
#ifdef ARDUINO

#include "EspMdnsResolver.h"

#include <ESPmDNS.h>
#include <WiFi.h>

std::optional<std::string> EspMdnsResolver::resolveHost(const std::string& hostname)
{
    const unsigned long waitStart = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - waitStart >= kWifiWaitTimeoutMs)
        {
            return std::nullopt;
        }
        delay(100);
    }

    if (!mdnsStarted_)
    {
        if (!MDNS.begin("maltbee-resolver"))
        {
            return std::nullopt;
        }
        mdnsStarted_ = true;
    }

    const IPAddress resolved = MDNS.queryHost(hostname.c_str());
    if (resolved == IPAddress(0, 0, 0, 0))
    {
        return std::nullopt;
    }

    return std::string(resolved.toString().c_str());
}

#endif
```

(This adapter blocks — with a bounded `delay()`-based wait for WiFi association, then whatever `MDNS.queryHost()` itself blocks for — which is acceptable here specifically because it is a hardware shim invoked exactly once, during `setup()`, before any time-sensitive `loop()` servicing begins. This does not violate the domain/application no-blocking rule, which does not apply to `#ifdef ARDUINO`-guarded adapters.)

- [ ] **Step 3: Commit**

Commit via the `arlo-commits` skill (message should reflect: add `EspMdnsResolver`, a bounded-wait-then-query mDNS hardware adapter). Note in the commit body (or let the reviewer note) that this compiles only under `esp32dev` — full verification happens in Task 12.

---

### Task 12: Wire `main.cpp` + `CaptivePortalServer` — final integration

**Files:**
- Modify: `src/esp32/main.cpp`
- Modify: `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp`

**Interfaces:**
- Consumes everything produced by Tasks 1–11: `Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource` (Tasks 5–6), `EspMdnsResolver`/`BrokerAddressResolver` (Tasks 4/11), `NodeConfig::channelTurnoutAddresses` (Task 2), `WebFormSubmission::channelTurnoutAddresses` (Task 9).
- Produces: a fully wired, building `esp32dev` firmware. Nothing downstream depends on this task — it's the top of the stack.

- [ ] **Step 1: Update `CaptivePortalServer.cpp`'s form-reading field name**

In `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp`, in `readForm()`, replace:

```cpp
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string fieldName = "t" + std::to_string(i + 1) + "_name";
        form.channelJmriNames[i] = webServer_.arg(fieldName.c_str()).c_str();
    }
```

with:

```cpp
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string fieldName = "t" + std::to_string(i + 1) + "_address";
        form.channelTurnoutAddresses[i] = webServer_.arg(fieldName.c_str()).c_str();
    }
```

- [ ] **Step 2: Update `main.cpp`'s includes**

In `src/esp32/main.cpp`, in the `#include` block, remove:

```cpp
#include "adapters/JmriFeedbackSource.h"
#include "adapters/JmriTurnoutCommandAdapter.h"
```

and add, keeping the whole block alphabetically sorted (matching the existing convention):

```cpp
#include "adapters/EspMdnsResolver.h"
```

right after `#include "adapters/EspDeviceIdentity.h"` and before `#include "adapters/EspUartPort.h"`, and:

```cpp
#include "adapters/Loco2MqttFeedbackSource.h"
#include "adapters/Loco2MqttTurnoutCommandAdapter.h"
```

right after `#include "adapters/LedPairStation.h"` and before `#include "adapters/MatrixDigitalInput.h"`, and:

```cpp
#include "application/BrokerAddressResolver.h"
```

right before `#include "application/CommissioningSession.h"`.

The resulting include block (lines 7–37 in the current file) should read:

```cpp
#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ArduinoDigitalOutput.h"
#include "adapters/ButtonSetupModeTrigger.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspMdnsResolver.h"
#include "adapters/EspUartPort.h"
#include "adapters/GatedDigitalInput.h"
#include "adapters/LedPairStation.h"
#include "adapters/Loco2MqttFeedbackSource.h"
#include "adapters/Loco2MqttTurnoutCommandAdapter.h"
#include "adapters/MatrixDigitalInput.h"
#include "adapters/MqttLink.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "adapters/WiFiLink.h"
#include "application/BrokerAddressResolver.h"
#include "application/CommissioningSession.h"
#include "application/MqttPresenceAnnouncer.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/IdentifyModeTimer.h"
#include "domain/LedPairDriver.h"
#include "domain/MatrixScanner.h"
#include "domain/NodeConfig.h"
#include "domain/NodeIdentityGuard.h"
#include "domain/PresenceTopics.h"
#include "domain/SetupApName.h"
#include "ports/TurnoutCommandPort.h"
```

- [ ] **Step 3: Add the `EspMdnsResolver`/`BrokerAddressResolver` globals**

In `src/esp32/main.cpp`, find:

```cpp
MqttLink mqttLink(systemClock, RETRY_INTERVAL_MS, mqttClientId, mqttWillTopic, mqttWillMessage);

NodeIdentityGuard identityGuard(ownMac.lastFourHexDigits());
```

and change it to:

```cpp
MqttLink mqttLink(systemClock, RETRY_INTERVAL_MS, mqttClientId, mqttWillTopic, mqttWillMessage);

EspMdnsResolver mdnsResolver;
BrokerAddressResolver brokerAddressResolver(mdnsResolver);

NodeIdentityGuard identityGuard(ownMac.lastFourHexDigits());
```

- [ ] **Step 4: Rename the turnout command/feedback adapter globals**

In `src/esp32/main.cpp`, find:

```cpp
JmriTurnoutCommandAdapter turnoutCommandPort(mqttLink, runningConfig.channelJmriNames);
JmriFeedbackSource feedbackSource(mqttLink, runningConfig.channelJmriNames);
```

and change it to:

```cpp
Loco2MqttTurnoutCommandAdapter turnoutCommandPort(mqttLink, runningConfig.channelTurnoutAddresses);
Loco2MqttFeedbackSource feedbackSource(mqttLink, runningConfig.channelTurnoutAddresses);
```

- [ ] **Step 5: Resolve the broker host before connecting**

In `src/esp32/main.cpp`, find, inside `setup()`:

```cpp
    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
        mqttLink.subscribe(PresenceTopics::macTopic(runningConfig.nodeId),
                            [](const std::string& payload) { identityGuard.onMacObserved(payload); });
        mqttLink.subscribe(PresenceTopics::identifyTopic(runningConfig.nodeId),
                            [](const std::string&) { identifyTimer.trigger(); });
    }
```

and change it to:

```cpp
    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        const std::string resolvedBrokerHost = brokerAddressResolver.resolve("loco2mqtt", runningConfig.brokerHost);
        mqttLink.begin(resolvedBrokerHost, runningConfig.brokerPort);
        mqttLink.subscribe(PresenceTopics::macTopic(runningConfig.nodeId),
                            [](const std::string& payload) { identityGuard.onMacObserved(payload); });
        mqttLink.subscribe(PresenceTopics::identifyTopic(runningConfig.nodeId),
                            [](const std::string&) { identifyTimer.trigger(); });
    }
```

- [ ] **Step 6: Run the full native test suite**

Run (in Bash/Git Bash): `pio test -e native`
Expected: PASS — every suite, including all suites touched by Tasks 1–10 and every pre-existing suite this pivot didn't touch.

- [ ] **Step 7: Build-check the ESP32 firmware**

Run: `pio run -e esp32dev`
Expected: BUILD SUCCESS — this is the first point at which `EspMdnsResolver.cpp`/`.h` (Task 11), the `CaptivePortalServer.cpp` change (Step 1 above), and every renamed/retyped class get compiled together as real firmware.

- [ ] **Step 8: Commit**

Commit via the `arlo-commits` skill (message should reflect: wire Loco2MQTT adapters, mDNS broker discovery, and the renamed turnout-address config into the ESP32 composition root).

---

## Explicitly out of scope for this plan

Matches the design doc's "Out of scope" section:
- Presence/collision-detection heartbeat redesign (sub-project #9b) — `MqttPresenceAnnouncer`/`NodeIdentityGuard` are untouched by every task above.
- Decommissioning `jmri/panel_mqtt_turnout_bridge.py` and its docs (sub-project #9c).
- Command-burst throttling for Loco2MQTT's 32-entry drop-oldest inbound queue.
- Periodic mDNS re-resolution after boot.
