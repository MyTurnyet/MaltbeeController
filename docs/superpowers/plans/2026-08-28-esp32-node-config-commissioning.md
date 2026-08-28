# ESP32 NodeConfig & Bench-Serial Commissioning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the (not-yet-built) ESP32 turnout panel a field-configurable
identity (node id, WiFi, broker, per-channel JMRI turnout name) and a
bench-serial commissioning workflow to set it, without hardcoding any of it
at compile time.

**Architecture:** Pure domain classes (`NodeConfig`, `ParsedCommand`,
`CommandLineParser`, `CommissioningSession`) compiled and tested under the
existing `native` PlatformIO environment with no `Arduino.h` dependency, same
as every other domain class in this project. Two new port interfaces
(`ConfigStore`, `UartPort`) plus one native-testable adapter
(`SerialCommissioningAdapter`) and two real hardware shims (`NvsConfigStore`,
`EspUartPort`, both `#ifdef ARDUINO`-guarded) complete the slice. No
WiFi/MQTT, no composition root wiring — this only makes the config
field-editable over USB serial.

**Tech Stack:** C++17, PlatformIO, Catch2 (native tests), ESP32 Arduino core
(`Preferences.h`, `Serial`) for the two hardware shims.

## Global Constraints

- Domain and port headers must compile under `native` with no `Arduino.h`
  (mechanically enforced: if a file includes `<Arduino.h>`, it isn't
  domain/port code).
- `std::string`/`std::vector`/`std::array` are fine here — this code only
  ever targets `native` and `esp32dev`, both with full libstdc++.
  **Do not** use `FixedString32` or fixed-capacity arrays for this slice;
  that type exists specifically to work around the Mega's AVR toolchain
  having no real STL, which doesn't apply to ESP32-only code.
- `NodeConfig` is an immutable value type: every mutation goes through a
  `with...()` method that returns a modified copy. Never add a setter that
  mutates in place.
- No mocking framework. Test doubles are hand-written classes implementing
  the same port interface, living in `test/support/`.
- `NodeConfig::validate()` must NOT require every one of the 12 channels to
  have a JMRI name — partial commissioning (some turnouts not yet wired) is
  legitimate. Only reject a *duplicate* non-empty name across channels.
- `NodeConfig::withChannelName()` with a channel number outside 1–12 is a
  silent no-op, not an error — same "add past capacity is a no-op"
  convention this project already uses for `TurnoutCollection`/`RouteService`
  (see `CLAUDE.md`).
- Hardware shims (`NvsConfigStore`, `EspUartPort`) are wrapped in
  `#ifdef ARDUINO` / `#endif` in their `.cpp` files (never their `.h`
  files), `final` classes, matching `ArduinoClock`/`ArduinoDigitalInput`'s
  existing style exactly.
- Every new header uses `#pragma once`. Adapter headers include ports via
  relative paths (`"../ports/X.h"`); `test/support/` fakes include via
  rooted paths (`"ports/X.h"`) — match the existing convention file-for-file
  (see `lib/McsCore/src/adapters/ArduinoDigitalInput.h` vs.
  `test/support/FakeTurnoutCommandPort.h`).
- Commit messages use this project's Arlo's Commit Notation (ACN) —
  `<risk symbol> <intention letter> <description>` — per `CLAUDE.md`. Every
  task below adds a new class with full test coverage but more than 8 lines
  of change, which under ACN's rules caps it at `!` (risky), not `^`,
  regardless of test quality — use `! F` for every task in this plan.

---

### Task 1: `NodeConfig` domain value type

**Files:**
- Create: `lib/McsCore/src/domain/NodeConfig.h`
- Create: `lib/McsCore/src/domain/NodeConfig.cpp`
- Test: `test/test_node_config/test_main.cpp`

**Interfaces:**
- Consumes: nothing (standard library only).
- Produces: `struct NodeConfig` with fields `int nodeId`, `std::string
  wifiSsid`, `std::string wifiPassword`, `std::string brokerHost`, `int
  brokerPort`, `std::array<std::string, NodeConfig::kChannelCount>
  channelJmriNames` (`kChannelCount == 12`); static
  `NodeConfig::factoryDefault() -> NodeConfig`; `NodeConfig::withNodeId(int)
  const -> NodeConfig`; `NodeConfig::withWifi(std::string, std::string) const
  -> NodeConfig`; `NodeConfig::withBroker(std::string, int) const ->
  NodeConfig`; `NodeConfig::withChannelName(int channel1To12, std::string)
  const -> NodeConfig`; `NodeConfig::validate() const ->
  std::vector<std::string>` (empty = valid).

- [ ] **Step 1: Write the failing test**

Create `test/test_node_config/test_main.cpp`:

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

TEST_CASE("withChannelName sets the named channel (1-based) without mutating the original")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withChannelName(1, "LT1");

    REQUIRE(updated.channelJmriNames[0] == "LT1");
    REQUIRE(original.channelJmriNames[0].empty());
}

TEST_CASE("withChannelName ignores an out-of-range channel number")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withChannelName(13, "LT13");

    for (const auto& name : updated.channelJmriNames)
    {
        REQUIRE(name.empty());
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

TEST_CASE("A valid config with only some channels named still passes validation")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelName(1, "LT1")
                                   .withChannelName(2, "LT2");

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

TEST_CASE("validate rejects two channels claiming the same jmri name")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelName(1, "LT1")
                                   .withChannelName(2, "LT1");

    REQUIRE_FALSE(config.validate().empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_node_config`
Expected: FAIL to compile — `domain/NodeConfig.h` does not exist yet.

- [ ] **Step 3: Write `lib/McsCore/src/domain/NodeConfig.h`**

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

    int nodeId = 0;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string brokerHost;
    int brokerPort = 1883;
    std::array<std::string, kChannelCount> channelJmriNames;

    static NodeConfig factoryDefault();

    [[nodiscard]] NodeConfig withNodeId(int id) const;
    [[nodiscard]] NodeConfig withWifi(std::string ssid, std::string password) const;
    [[nodiscard]] NodeConfig withBroker(std::string host, int port) const;
    [[nodiscard]] NodeConfig withChannelName(int channel, std::string jmriName) const;

    [[nodiscard]] std::vector<std::string> validate() const;
};
```

- [ ] **Step 4: Write `lib/McsCore/src/domain/NodeConfig.cpp`**

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

NodeConfig NodeConfig::withChannelName(const int channel, std::string jmriName) const
{
    NodeConfig copy = *this;
    if (channel >= 1 && channel <= kChannelCount)
    {
        copy.channelJmriNames[channel - 1] = std::move(jmriName);
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
        if (channelJmriNames[i].empty())
        {
            continue;
        }
        for (int j = i + 1; j < kChannelCount; ++j)
        {
            if (channelJmriNames[i] == channelJmriNames[j])
            {
                errors.push_back("channels " + std::to_string(i + 1) + " and " +
                                  std::to_string(j + 1) + " both claim jmri name \"" +
                                  channelJmriNames[i] + "\"");
            }
        }
    }

    return errors;
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `pio test -e native -f test_node_config`
Expected: PASS, all 12 test cases green.

- [ ] **Step 6: Commit**

```bash
git add lib/McsCore/src/domain/NodeConfig.h lib/McsCore/src/domain/NodeConfig.cpp test/test_node_config/test_main.cpp
git commit -m "! F Add NodeConfig value type"
```

---

### Task 2: `ParsedCommand` and `CommandLineParser`

**Files:**
- Create: `lib/McsCore/src/domain/ParsedCommand.h`
- Create: `lib/McsCore/src/domain/CommandLineParser.h`
- Create: `lib/McsCore/src/domain/CommandLineParser.cpp`
- Test: `test/test_command_line_parser/test_main.cpp`

**Interfaces:**
- Consumes: nothing (standard library only).
- Produces: `enum class CommandKind { Id, Wifi, Broker, TurnoutName, Show,
  Save, Reboot, Invalid }`; `struct ParsedCommand { CommandKind kind;
  int intArg; std::string stringArg1; std::string stringArg2; int intArg2;
  std::string errorMessage; }`; `CommandLineParser::parse(const std::string&
  line) -> ParsedCommand` (pure, static).

- [ ] **Step 1: Write the failing test**

Create `test/test_command_line_parser/test_main.cpp`:

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

TEST_CASE("parses turnout name command")
{
    const ParsedCommand command = CommandLineParser::parse("turnout 3 name LT3");

    REQUIRE(command.kind == CommandKind::TurnoutName);
    REQUIRE(command.intArg == 3);
    REQUIRE(command.stringArg1 == "LT3");
}

TEST_CASE("turnout command missing the name keyword is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("turnout 3 LT3");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("turnout command with a non-numeric channel is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("turnout x name LT3");

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

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_command_line_parser`
Expected: FAIL to compile — `domain/CommandLineParser.h` does not exist yet.

- [ ] **Step 3: Write `lib/McsCore/src/domain/ParsedCommand.h`**

```cpp
#pragma once

#include <string>

enum class CommandKind
{
    Id,
    Wifi,
    Broker,
    TurnoutName,
    Show,
    Save,
    Reboot,
    Invalid
};

struct ParsedCommand
{
    CommandKind kind = CommandKind::Invalid;
    int intArg = 0;
    std::string stringArg1;
    std::string stringArg2;
    int intArg2 = 0;
    std::string errorMessage;
};
```

- [ ] **Step 4: Write `lib/McsCore/src/domain/CommandLineParser.h`**

```cpp
#pragma once

#include "ParsedCommand.h"

class CommandLineParser
{
public:
    static ParsedCommand parse(const std::string& line);
};
```

- [ ] **Step 5: Write `lib/McsCore/src/domain/CommandLineParser.cpp`**

```cpp
#include "CommandLineParser.h"

#include <sstream>
#include <vector>

namespace
{
    ParsedCommand invalid(const std::string& message)
    {
        ParsedCommand command;
        command.kind = CommandKind::Invalid;
        command.errorMessage = message;
        return command;
    }

    std::vector<std::string> tokenize(const std::string& line)
    {
        std::vector<std::string> tokens;
        std::istringstream stream(line);
        std::string token;
        while (stream >> token)
        {
            tokens.push_back(token);
        }
        return tokens;
    }

    bool parseInt(const std::string& text, int& out)
    {
        if (text.empty())
        {
            return false;
        }
        try
        {
            size_t consumed = 0;
            const int value = std::stoi(text, &consumed);
            if (consumed != text.size())
            {
                return false;
            }
            out = value;
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
}

ParsedCommand CommandLineParser::parse(const std::string& line)
{
    const std::vector<std::string> tokens = tokenize(line);

    if (tokens.empty())
    {
        return invalid("empty command");
    }

    const std::string& verb = tokens[0];

    if (verb == "id")
    {
        if (tokens.size() != 2)
        {
            return invalid("usage: id <n>");
        }
        int id = 0;
        if (!parseInt(tokens[1], id))
        {
            return invalid("id must be a number");
        }
        ParsedCommand command;
        command.kind = CommandKind::Id;
        command.intArg = id;
        return command;
    }

    if (verb == "wifi")
    {
        if (tokens.size() != 3)
        {
            return invalid("usage: wifi <ssid> <password> (no spaces in either)");
        }
        ParsedCommand command;
        command.kind = CommandKind::Wifi;
        command.stringArg1 = tokens[1];
        command.stringArg2 = tokens[2];
        return command;
    }

    if (verb == "broker")
    {
        if (tokens.size() != 3)
        {
            return invalid("usage: broker <host> <port>");
        }
        int port = 0;
        if (!parseInt(tokens[2], port))
        {
            return invalid("broker port must be a number");
        }
        ParsedCommand command;
        command.kind = CommandKind::Broker;
        command.stringArg1 = tokens[1];
        command.intArg2 = port;
        return command;
    }

    if (verb == "turnout")
    {
        if (tokens.size() != 4 || tokens[2] != "name")
        {
            return invalid("usage: turnout <n> name <jmriSystemName>");
        }
        int channel = 0;
        if (!parseInt(tokens[1], channel))
        {
            return invalid("turnout channel must be a number");
        }
        ParsedCommand command;
        command.kind = CommandKind::TurnoutName;
        command.intArg = channel;
        command.stringArg1 = tokens[3];
        return command;
    }

    if (verb == "show")
    {
        if (tokens.size() != 1)
        {
            return invalid("usage: show");
        }
        ParsedCommand command;
        command.kind = CommandKind::Show;
        return command;
    }

    if (verb == "save")
    {
        if (tokens.size() != 1)
        {
            return invalid("usage: save");
        }
        ParsedCommand command;
        command.kind = CommandKind::Save;
        return command;
    }

    if (verb == "reboot")
    {
        if (tokens.size() != 1)
        {
            return invalid("usage: reboot");
        }
        ParsedCommand command;
        command.kind = CommandKind::Reboot;
        return command;
    }

    return invalid("unknown command: " + verb);
}
```

- [ ] **Step 6: Run test to verify it passes**

Run: `pio test -e native -f test_command_line_parser`
Expected: PASS, all 16 test cases green.

- [ ] **Step 7: Commit**

```bash
git add lib/McsCore/src/domain/ParsedCommand.h lib/McsCore/src/domain/CommandLineParser.h lib/McsCore/src/domain/CommandLineParser.cpp test/test_command_line_parser/test_main.cpp
git commit -m "! F Add ParsedCommand and CommandLineParser"
```

---

### Task 3: `ConfigStore` port, `FakeConfigStore`, and `CommissioningSession`

**Files:**
- Create: `lib/McsCore/src/ports/ConfigStore.h`
- Create: `test/support/FakeConfigStore.h`
- Create: `lib/McsCore/src/domain/CommissioningSession.h`
- Create: `lib/McsCore/src/domain/CommissioningSession.cpp`
- Test: `test/test_commissioning_session/test_main.cpp`

**Interfaces:**
- Consumes: `NodeConfig` (Task 1); `CommandKind`/`ParsedCommand` (Task 2).
- Produces: `class ConfigStore { virtual NodeConfig load() = 0; virtual void
  save(const NodeConfig&) = 0; }`; `class FakeConfigStore final : public
  ConfigStore` with a public `int saveCount` field; `class
  CommissioningSession { CommissioningSession(ConfigStore&);
  std::string apply(const ParsedCommand&); bool rebootRequested() const; }`.

- [ ] **Step 1: Write the failing test**

Create `test/test_commissioning_session/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/CommissioningSession.h"
#include "support/FakeConfigStore.h"

TEST_CASE("id command updates the draft and confirms")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand command;
    command.kind = CommandKind::Id;
    command.intArg = 5;

    const std::string response = session.apply(command);

    REQUIRE(response == "OK\n");
}

TEST_CASE("wifi command updates the draft and confirms")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand command;
    command.kind = CommandKind::Wifi;
    command.stringArg1 = "MyLayoutWifi";
    command.stringArg2 = "hunter2";

    const std::string response = session.apply(command);

    REQUIRE(response == "OK\n");
}

TEST_CASE("show reports an unconfigured factory-default draft")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand command;
    command.kind = CommandKind::Show;

    const std::string response = session.apply(command);

    REQUIRE(response.find("id: 0") != std::string::npos);
    REQUIRE(response.find("(unconfigured)") != std::string::npos);
}

TEST_CASE("show reports a configured turnout name")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand nameCommand;
    nameCommand.kind = CommandKind::TurnoutName;
    nameCommand.intArg = 1;
    nameCommand.stringArg1 = "LT1";
    session.apply(nameCommand);

    ParsedCommand showCommand;
    showCommand.kind = CommandKind::Show;
    const std::string response = session.apply(showCommand);

    REQUIRE(response.find("turnout 1: LT1") != std::string::npos);
}

TEST_CASE("save persists a valid draft to the config store")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand idCommand;
    idCommand.kind = CommandKind::Id;
    idCommand.intArg = 1;
    session.apply(idCommand);

    ParsedCommand wifiCommand;
    wifiCommand.kind = CommandKind::Wifi;
    wifiCommand.stringArg1 = "MyLayoutWifi";
    wifiCommand.stringArg2 = "hunter2";
    session.apply(wifiCommand);

    ParsedCommand brokerCommand;
    brokerCommand.kind = CommandKind::Broker;
    brokerCommand.stringArg1 = "192.168.1.50";
    brokerCommand.intArg2 = 1883;
    session.apply(brokerCommand);

    ParsedCommand saveCommand;
    saveCommand.kind = CommandKind::Save;
    const std::string response = session.apply(saveCommand);

    REQUIRE(response == "saved\n");
    REQUIRE(store.saveCount == 1);
    REQUIRE(store.load().nodeId == 1);
}

TEST_CASE("save refuses an invalid draft and does not persist")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand saveCommand;
    saveCommand.kind = CommandKind::Save;
    const std::string response = session.apply(saveCommand);

    REQUIRE(response.find("invalid config") != std::string::npos);
    REQUIRE(store.saveCount == 0);
}

TEST_CASE("reboot sets rebootRequested and confirms")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    REQUIRE_FALSE(session.rebootRequested());

    ParsedCommand rebootCommand;
    rebootCommand.kind = CommandKind::Reboot;
    const std::string response = session.apply(rebootCommand);

    REQUIRE(response == "rebooting\n");
    REQUIRE(session.rebootRequested());
}

TEST_CASE("an invalid parsed command echoes its error message")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand invalidCommand;
    invalidCommand.kind = CommandKind::Invalid;
    invalidCommand.errorMessage = "usage: id <n>";

    const std::string response = session.apply(invalidCommand);

    REQUIRE(response == "error: usage: id <n>\n");
}

TEST_CASE("a fresh session loads the store's existing config as its draft")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault().withNodeId(7));

    CommissioningSession session(store);

    ParsedCommand showCommand;
    showCommand.kind = CommandKind::Show;
    const std::string response = session.apply(showCommand);

    REQUIRE(response.find("id: 7") != std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_commissioning_session`
Expected: FAIL to compile — none of this task's files exist yet.

- [ ] **Step 3: Write `lib/McsCore/src/ports/ConfigStore.h`**

```cpp
#pragma once

#include "../domain/NodeConfig.h"

class ConfigStore
{
public:
    virtual ~ConfigStore() = default;

    virtual NodeConfig load() = 0;
    virtual void save(const NodeConfig& config) = 0;
};
```

- [ ] **Step 4: Write `test/support/FakeConfigStore.h`**

```cpp
#pragma once

#include "domain/NodeConfig.h"
#include "ports/ConfigStore.h"

class FakeConfigStore final : public ConfigStore
{
public:
    int saveCount = 0;

    NodeConfig load() override
    {
        return stored_;
    }

    void save(const NodeConfig& config) override
    {
        stored_ = config;
        saveCount++;
    }

private:
    NodeConfig stored_ = NodeConfig::factoryDefault();
};
```

- [ ] **Step 5: Write `lib/McsCore/src/domain/CommissioningSession.h`**

```cpp
#pragma once

#include <string>
#include <vector>

#include "NodeConfig.h"
#include "ParsedCommand.h"
#include "../ports/ConfigStore.h"

class CommissioningSession
{
public:
    explicit CommissioningSession(ConfigStore& store);

    std::string apply(const ParsedCommand& command);

    [[nodiscard]] bool rebootRequested() const;

private:
    [[nodiscard]] std::string formatShow() const;
    [[nodiscard]] std::string formatErrors(const std::vector<std::string>& errors) const;

    ConfigStore& store_;
    NodeConfig draft_;
    bool rebootRequested_ = false;
};
```

- [ ] **Step 6: Write `lib/McsCore/src/domain/CommissioningSession.cpp`**

```cpp
#include "CommissioningSession.h"

CommissioningSession::CommissioningSession(ConfigStore& store)
    : store_(store), draft_(store.load())
{
}

std::string CommissioningSession::formatErrors(const std::vector<std::string>& errors) const
{
    std::string result = "invalid config:\n";
    for (const std::string& error : errors)
    {
        result += "  - " + error + "\n";
    }
    return result;
}

std::string CommissioningSession::formatShow() const
{
    std::string result;
    result += "id: " + std::to_string(draft_.nodeId) + "\n";
    result += "wifi ssid: " + (draft_.wifiSsid.empty() ? std::string("(unconfigured)") : draft_.wifiSsid) + "\n";
    result += "broker: " + (draft_.brokerHost.empty() ? std::string("(unconfigured)") : draft_.brokerHost) +
              ":" + std::to_string(draft_.brokerPort) + "\n";
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string& name = draft_.channelJmriNames[i];
        result += "turnout " + std::to_string(i + 1) + ": " +
                  (name.empty() ? std::string("(unconfigured)") : name) + "\n";
    }
    return result;
}

std::string CommissioningSession::apply(const ParsedCommand& command)
{
    switch (command.kind)
    {
    case CommandKind::Id:
        draft_ = draft_.withNodeId(command.intArg);
        return "OK\n";

    case CommandKind::Wifi:
        draft_ = draft_.withWifi(command.stringArg1, command.stringArg2);
        return "OK\n";

    case CommandKind::Broker:
        draft_ = draft_.withBroker(command.stringArg1, command.intArg2);
        return "OK\n";

    case CommandKind::TurnoutName:
        draft_ = draft_.withChannelName(command.intArg, command.stringArg1);
        return "OK\n";

    case CommandKind::Show:
        return formatShow();

    case CommandKind::Save:
    {
        const std::vector<std::string> errors = draft_.validate();
        if (!errors.empty())
        {
            return formatErrors(errors);
        }
        store_.save(draft_);
        return "saved\n";
    }

    case CommandKind::Reboot:
        rebootRequested_ = true;
        return "rebooting\n";

    case CommandKind::Invalid:
    default:
        return "error: " + command.errorMessage + "\n";
    }
}

bool CommissioningSession::rebootRequested() const
{
    return rebootRequested_;
}
```

- [ ] **Step 7: Run test to verify it passes**

Run: `pio test -e native -f test_commissioning_session`
Expected: PASS, all 9 test cases green.

- [ ] **Step 8: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every existing suite plus the 3 new ones from Tasks 1-3 green.

- [ ] **Step 9: Commit**

```bash
git add lib/McsCore/src/ports/ConfigStore.h test/support/FakeConfigStore.h lib/McsCore/src/domain/CommissioningSession.h lib/McsCore/src/domain/CommissioningSession.cpp test/test_commissioning_session/test_main.cpp
git commit -m "! F Add ConfigStore port and CommissioningSession"
```

---

### Task 4: `UartPort` port, `FakeUartPort`, and `SerialCommissioningAdapter`

**Files:**
- Create: `lib/McsCore/src/ports/UartPort.h`
- Create: `test/support/FakeUartPort.h`
- Create: `lib/McsCore/src/adapters/SerialCommissioningAdapter.h`
- Create: `lib/McsCore/src/adapters/SerialCommissioningAdapter.cpp`
- Test: `test/test_serial_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes: `CommissioningSession` (Task 3); `CommandLineParser`/
  `ParsedCommand` (Task 2).
- Produces: `class UartPort { virtual bool available() const = 0; virtual
  char read() = 0; virtual void write(const std::string&) = 0; }`; `class
  FakeUartPort final : public UartPort` with `void queueInput(const
  std::string&)` and a public `std::string written` field; `class
  SerialCommissioningAdapter { SerialCommissioningAdapter(UartPort&,
  CommissioningSession&); void poll(); bool rebootRequested() const; }`.

- [ ] **Step 1: Write the failing test**

Create `test/test_serial_commissioning_adapter/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/SerialCommissioningAdapter.h"
#include "support/FakeConfigStore.h"
#include "support/FakeUartPort.h"

TEST_CASE("a complete line is parsed and dispatched on newline")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.queueInput("id 5\n");
    adapter.poll();

    REQUIRE(uart.written == "OK\n");
}

TEST_CASE("a partial line with no newline yet does not dispatch")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.queueInput("id 5");
    adapter.poll();

    REQUIRE(uart.written.empty());
}

TEST_CASE("multiple commands in sequence each dispatch separately")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.queueInput("id 5\nshow\n");
    adapter.poll();

    REQUIRE(uart.written.find("OK\n") != std::string::npos);
    REQUIRE(uart.written.find("id: 5") != std::string::npos);
}

TEST_CASE("a trailing carriage return before the newline is stripped")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.queueInput("id 5\r\n");
    adapter.poll();

    REQUIRE(uart.written == "OK\n");
}

TEST_CASE("bytes arriving across multiple poll() calls still assemble into one line")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.queueInput("id ");
    adapter.poll();
    REQUIRE(uart.written.empty());

    uart.queueInput("5\n");
    adapter.poll();
    REQUIRE(uart.written == "OK\n");
}

TEST_CASE("rebootRequested delegates to the underlying session")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    REQUIRE_FALSE(adapter.rebootRequested());

    uart.queueInput("reboot\n");
    adapter.poll();

    REQUIRE(adapter.rebootRequested());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_serial_commissioning_adapter`
Expected: FAIL to compile — none of this task's files exist yet.

- [ ] **Step 3: Write `lib/McsCore/src/ports/UartPort.h`**

```cpp
#pragma once

#include <string>

class UartPort
{
public:
    virtual ~UartPort() = default;

    [[nodiscard]] virtual bool available() const = 0;
    virtual char read() = 0;
    virtual void write(const std::string& text) = 0;
};
```

- [ ] **Step 4: Write `test/support/FakeUartPort.h`**

```cpp
#pragma once

#include <string>

#include "ports/UartPort.h"

class FakeUartPort final : public UartPort
{
public:
    std::string written;

    void queueInput(const std::string& text)
    {
        pending_ += text;
    }

    [[nodiscard]] bool available() const override
    {
        return position_ < pending_.size();
    }

    char read() override
    {
        return pending_[position_++];
    }

    void write(const std::string& text) override
    {
        written += text;
    }

private:
    std::string pending_;
    size_t position_ = 0;
};
```

- [ ] **Step 5: Write `lib/McsCore/src/adapters/SerialCommissioningAdapter.h`**

```cpp
#pragma once

#include <string>

#include "../domain/CommissioningSession.h"
#include "../ports/UartPort.h"

class SerialCommissioningAdapter
{
public:
    SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session);

    void poll();

    [[nodiscard]] bool rebootRequested() const;

private:
    UartPort& uart_;
    CommissioningSession& session_;
    std::string lineBuffer_;
};
```

- [ ] **Step 6: Write `lib/McsCore/src/adapters/SerialCommissioningAdapter.cpp`**

```cpp
#include "SerialCommissioningAdapter.h"

#include "../domain/CommandLineParser.h"

SerialCommissioningAdapter::SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session)
    : uart_(uart), session_(session)
{
}

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
        else
        {
            lineBuffer_ += c;
        }
    }
}

bool SerialCommissioningAdapter::rebootRequested() const
{
    return session_.rebootRequested();
}
```

- [ ] **Step 7: Run test to verify it passes**

Run: `pio test -e native -f test_serial_commissioning_adapter`
Expected: PASS, all 6 test cases green.

- [ ] **Step 8: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every existing suite plus the 4 new ones green.

- [ ] **Step 9: Commit**

```bash
git add lib/McsCore/src/ports/UartPort.h test/support/FakeUartPort.h lib/McsCore/src/adapters/SerialCommissioningAdapter.h lib/McsCore/src/adapters/SerialCommissioningAdapter.cpp test/test_serial_commissioning_adapter/test_main.cpp
git commit -m "! F Add UartPort port and SerialCommissioningAdapter"
```

---

### Task 5: `NvsConfigStore` and `EspUartPort` hardware adapters

**Files:**
- Create: `lib/McsCore/src/adapters/NvsConfigStore.h`
- Create: `lib/McsCore/src/adapters/NvsConfigStore.cpp`
- Create: `lib/McsCore/src/adapters/EspUartPort.h`
- Create: `lib/McsCore/src/adapters/EspUartPort.cpp`
- Temporarily modify (then revert): `src/esp32/main.cpp`

**Interfaces:**
- Consumes: `ConfigStore`/`NodeConfig` (Tasks 1, 3); `UartPort` (Task 4).
- Produces: `class NvsConfigStore final : public ConfigStore` (ESP32
  `Preferences`-backed); `class EspUartPort final : public UartPort` with an
  added `void begin()` (wraps `Serial`).

No native test exists for these — both are `#ifdef ARDUINO`-guarded hardware
shims with no logic beyond wrapping ESP32 APIs, verified instead by a
build-check against the real `esp32dev` target, matching this project's
existing convention for `ArduinoClock`/`ArduinoDigitalInput`.

- [ ] **Step 1: Write `lib/McsCore/src/adapters/NvsConfigStore.h`**

```cpp
#pragma once

#include "../ports/ConfigStore.h"

class NvsConfigStore final : public ConfigStore
{
public:
    NodeConfig load() override;
    void save(const NodeConfig& config) override;
};
```

- [ ] **Step 2: Write `lib/McsCore/src/adapters/NvsConfigStore.cpp`**

```cpp
#ifdef ARDUINO

#include "NvsConfigStore.h"

#include <Preferences.h>

namespace
{
    constexpr const char* kNamespace = "mcsnode";
    constexpr const char* kKeyNodeId = "nodeId";
    constexpr const char* kKeyWifiSsid = "wifiSsid";
    constexpr const char* kKeyWifiPassword = "wifiPw";
    constexpr const char* kKeyBrokerHost = "brokerHost";
    constexpr const char* kKeyBrokerPort = "brokerPort";
    constexpr const char* kKeyChannelPrefix = "ch";
}

NodeConfig NvsConfigStore::load()
{
    Preferences prefs;
    prefs.begin(kNamespace, true);

    NodeConfig config = NodeConfig::factoryDefault();
    config.nodeId = prefs.getInt(kKeyNodeId, 0);
    config.wifiSsid = prefs.getString(kKeyWifiSsid, "").c_str();
    config.wifiPassword = prefs.getString(kKeyWifiPassword, "").c_str();
    config.brokerHost = prefs.getString(kKeyBrokerHost, "").c_str();
    config.brokerPort = prefs.getInt(kKeyBrokerPort, 1883);

    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string key = std::string(kKeyChannelPrefix) + std::to_string(i);
        config.channelJmriNames[i] = prefs.getString(key.c_str(), "").c_str();
    }

    prefs.end();
    return config;
}

void NvsConfigStore::save(const NodeConfig& config)
{
    Preferences prefs;
    prefs.begin(kNamespace, false);

    prefs.putInt(kKeyNodeId, config.nodeId);
    prefs.putString(kKeyWifiSsid, config.wifiSsid.c_str());
    prefs.putString(kKeyWifiPassword, config.wifiPassword.c_str());
    prefs.putString(kKeyBrokerHost, config.brokerHost.c_str());
    prefs.putInt(kKeyBrokerPort, config.brokerPort);

    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string key = std::string(kKeyChannelPrefix) + std::to_string(i);
        prefs.putString(key.c_str(), config.channelJmriNames[i].c_str());
    }

    prefs.end();
}

#endif
```

Every NVS key above is under the ESP-IDF's 15-character key-name limit
(`brokerHost`/`brokerPort` are the longest at 10 chars) — verify this holds
if any key name is changed.

- [ ] **Step 3: Write `lib/McsCore/src/adapters/EspUartPort.h`**

```cpp
#pragma once

#include "../ports/UartPort.h"

class EspUartPort final : public UartPort
{
public:
    explicit EspUartPort(unsigned long baudRate);

    void begin();

    [[nodiscard]] bool available() const override;
    char read() override;
    void write(const std::string& text) override;

private:
    unsigned long baudRate_;
};
```

- [ ] **Step 4: Write `lib/McsCore/src/adapters/EspUartPort.cpp`**

```cpp
#ifdef ARDUINO

#include "EspUartPort.h"

#include <Arduino.h>

EspUartPort::EspUartPort(const unsigned long baudRate) : baudRate_(baudRate)
{
}

void EspUartPort::begin()
{
    Serial.begin(baudRate_);
}

bool EspUartPort::available() const
{
    return Serial.available() > 0;
}

char EspUartPort::read()
{
    return static_cast<char>(Serial.read());
}

void EspUartPort::write(const std::string& text)
{
    Serial.print(text.c_str());
}

#endif
```

- [ ] **Step 5: Run the full native suite to confirm nothing broke**

Run: `pio test -e native`
Expected: PASS — these two files are Arduino-only and contribute nothing to
the native build, so this just confirms Tasks 1-4 are still green.

- [ ] **Step 6: Temporarily wire both into `src/esp32/main.cpp` to build-check them**

Read the current `src/esp32/main.cpp` first to confirm it's still the
empty stub (`#include <Arduino.h>` plus empty `setup()`/`loop()`) before
editing — if it's been changed by other work in the meantime, adapt this
step accordingly rather than clobbering it.

Replace its contents with:

```cpp
#include <Arduino.h>

#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"

static NvsConfigStore configStore;
static EspUartPort uartPort(115200);

void setup()
{
    uartPort.begin();
    NodeConfig config = configStore.load();
    configStore.save(config);
}

void loop()
{
}
```

- [ ] **Step 7: Build for the ESP32 target**

Run: `pio run -e esp32dev`
Expected: SUCCESS — confirms `NvsConfigStore` and `EspUartPort` compile
cleanly against the real `Preferences`/`Serial` ESP32 APIs.

- [ ] **Step 8: Revert `src/esp32/main.cpp` back to the empty stub**

```cpp
#include <Arduino.h>

void setup()
{
}

void loop()
{
}
```

This temporary wiring exists only to force the compiler through the two new
adapters — the real composition root arrives in a later slice
(2b, per the design spec). Confirm with `git diff src/esp32/main.cpp` that
it's back to its original state before committing.

- [ ] **Step 9: Commit**

```bash
git add lib/McsCore/src/adapters/NvsConfigStore.h lib/McsCore/src/adapters/NvsConfigStore.cpp lib/McsCore/src/adapters/EspUartPort.h lib/McsCore/src/adapters/EspUartPort.cpp
git commit -m "! F Add NvsConfigStore and EspUartPort ESP32 adapters"
```

---

## Definition of Done for this Plan

- [ ] `pio test -e native` passes, including the 4 new suites
      (`test_node_config`, `test_command_line_parser`,
      `test_commissioning_session`, `test_serial_commissioning_adapter`).
- [ ] `pio run -e megaatmega2560` still builds cleanly (none of this plan's
      files are reachable from `src/mega/main.cpp`, so this should be
      unaffected — verify rather than assume).
- [ ] `pio run -e esp32dev` builds cleanly with `src/esp32/main.cpp` back at
      its original empty-stub content (i.e., the build-check wiring from
      Task 5 has been reverted, not left in place).
- [ ] Five commits on `main` (or a feature branch, per whatever the executor
      chooses), each `! F`, one per task above.
- [ ] Nothing in this plan touches `src/esp32/main.cpp` permanently, WiFi,
      MQTT, the wireless captive portal, or node presence/collision/identify
      — all explicitly deferred to slices 2b/2c/2d per
      `docs/superpowers/specs/2026-08-28-esp32-node-config-commissioning-design.md`.
