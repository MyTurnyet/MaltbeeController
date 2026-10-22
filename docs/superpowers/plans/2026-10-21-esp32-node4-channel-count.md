# 4-Turnout Node: Build Target and Channel Count — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a second PlatformIO environment + composition root for the 4-turnout node board, sharing the 12-channel panel's commissioning logic via a channel count template parameter, with zero behavior change to the 12-channel panel.

**Architecture:** Eight existing classes in the ESP32 commissioning chain (`NodeConfig`, `ConfigStore`, `CommissioningSession`, `WebFormSubmission`, `WebFormCommissioningAdapter`, `SerialCommissioningAdapter`, `NvsConfigStore`, `CaptivePortalServer`) become templates parameterized on channel count `N`, renamed internally to `Basic<X><N>`, with the existing name preserved as a `using X = Basic<X><12>` alias and a new `using X4 = Basic<X><4>` alias for the node. Two more classes (`SetupFormRenderer::render()`, `BootModeSelector::select()`) gain a templated method instead of becoming class templates, since they're stateless. A new `[env:esp32node4]` PlatformIO environment and `src/esp32node4/main.cpp` composition root wire the `*4` aliases into a minimal placeholder that boots and commissions but doesn't yet drive turnout hardware.

**Tech Stack:** C++17, PlatformIO, Catch2 (native tests), ESP-IDF/Arduino framework (esp32dev board).

## Global Constraints

- `pio test -e native` must pass all 44 existing suites **unchanged**, plus new suites for the N=4 instantiations.
- `pio run -e esp32dev` must build with no behavior change to the 12-channel panel.
- `pio run -e esp32node4` (new) must build.
- `pio run -e megaatmega2560` must be unaffected.
- Do not change MQTT topics, NVS keys/namespace, or `kFirmwareVersion` semantics for the 12-channel build.
- On Windows, run `pio test -e native` via Bash/Git Bash, not PowerShell (per `CLAUDE.md`).
- Run tests via the `pioTest` skill. Commit via the `/arlo-commits` skill — never hand-write commit messages.
- Full spec: `docs/superpowers/specs/2026-10-21-esp32-node4-channel-count-design.md`.

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `lib/McsEsp32/src/domain/NodeConfig.h` | Modify (becomes template, absorbs `.cpp` body) | `BasicNodeConfig<N>` + `NodeConfig`/`NodeConfig4` aliases |
| `lib/McsEsp32/src/domain/NodeConfig.cpp` | Delete | Body folded into the header (templates must be visible at instantiation) |
| `test/test_node_config4/test_main.cpp` | Create | N=4-sensitive `NodeConfig4` coverage |
| `lib/McsEsp32/src/ports/ConfigStore.h` | Modify (becomes template) | `BasicConfigStore<N>` + `ConfigStore`/`ConfigStore4` aliases |
| `lib/McsEsp32/src/application/CommissioningSession.h` | Modify (becomes template, absorbs `.cpp` body) | `BasicCommissioningSession<N>` + aliases |
| `lib/McsEsp32/src/application/CommissioningSession.cpp` | Delete | Body folded into the header |
| `test/support/FakeConfigStore4.h` | Create | N=4 test double for `ConfigStore4` |
| `test/test_commissioning_session4/test_main.cpp` | Create | N=4-sensitive `CommissioningSession4` coverage |
| `lib/McsEsp32/src/domain/WebFormSubmission.h` | Modify (becomes template) | `BasicWebFormSubmission<N>` + aliases |
| `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.h` | Modify (becomes template, absorbs `.cpp` body) | `BasicWebFormCommissioningAdapter<N>` + aliases |
| `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp` | Delete | Body folded into the header |
| `test/test_web_form_commissioning_adapter4/test_main.cpp` | Create | N=4-sensitive web-form-adapter coverage |
| `lib/McsEsp32/src/domain/SetupFormRenderer.h` | Modify (`render()` becomes a function template) | Renders 4 or 12 channel fields depending on `N` |
| `test/test_setup_form_renderer/test_main.cpp` | Modify (append only) | New N=4 rendering test case |
| `lib/McsEsp32/src/domain/BootModeSelector.h` | Modify (`select()` becomes a function template) | Works for any `BasicNodeConfig<N>` |
| `test/test_boot_mode_selector/test_main.cpp` | Modify (append only) | New N=4 boot-mode-selection test cases |
| `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h` | Modify (becomes template, absorbs `.cpp` body) | `BasicSerialCommissioningAdapter<N>` + aliases |
| `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.cpp` | Delete | Body folded into the header |
| `test/test_serial_commissioning_adapter4/test_main.cpp` | Create | N=4-sensitive serial-adapter coverage |
| `lib/McsEsp32/src/adapters/NvsConfigStore.h` | Modify (becomes template) | `BasicNvsConfigStore<N>` + aliases |
| `lib/McsEsp32/src/adapters/NvsConfigStore.cpp` | Modify (template method bodies + explicit instantiation) | Hardware adapter, `#ifdef ARDUINO`-guarded, no native test (unchanged from today) |
| `lib/McsEsp32/src/adapters/CaptivePortalServer.h` | Modify (becomes template) | `BasicCaptivePortalServer<N>` + aliases |
| `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp` | Modify (template method bodies + explicit instantiation) | Hardware adapter, `#ifdef ARDUINO`-guarded, no native test (unchanged from today) |
| `platformio.ini` | Modify | New `[env:esp32node4]` environment |
| `src/esp32node4/main.cpp` | Create | Minimal placeholder composition root for the node |

---

## Task 1: Template `NodeConfig`

**Files:**
- Modify: `lib/McsEsp32/src/domain/NodeConfig.h`
- Delete: `lib/McsEsp32/src/domain/NodeConfig.cpp`
- Test: `test/test_node_config4/test_main.cpp` (new)
- Existing test (must keep passing unchanged): `test/test_node_config/test_main.cpp`

**Interfaces:**
- Produces: `template <int N> struct BasicNodeConfig { ... }`; `using NodeConfig = BasicNodeConfig<12>;`; `using NodeConfig4 = BasicNodeConfig<4>;`. Same member names/signatures as today's `NodeConfig` (`nodeId`, `wifiSsid`, `wifiPassword`, `brokerHost`, `brokerPort`, `channelTurnoutAddresses`, `factoryDefault()`, `withNodeId()`, `withWifi()`, `withBroker()`, `withChannelAddress()`, `validate()`, `kChannelCount`, `kMinNodeId`, `kMaxNodeId`, `kMinBrokerPort`, `kMaxBrokerPort`, `kMinTurnoutAddress`, `kMaxTurnoutAddress`).

- [ ] **Step 1: Write the failing test**

Create `test/test_node_config4/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/NodeConfig.h"

TEST_CASE("NodeConfig4 has a channel count of 4")
{
    REQUIRE(NodeConfig4::kChannelCount == 4);
}

TEST_CASE("NodeConfig4 factory default has 4 unconfigured channels")
{
    const NodeConfig4 config = NodeConfig4::factoryDefault();

    REQUIRE(config.channelTurnoutAddresses.size() == 4);
    for (const int address : config.channelTurnoutAddresses)
    {
        REQUIRE(address == 0);
    }
}

TEST_CASE("NodeConfig4 withChannelAddress accepts channel 4")
{
    const NodeConfig4 original = NodeConfig4::factoryDefault();

    const NodeConfig4 updated = original.withChannelAddress(4, 42);

    REQUIRE(updated.channelTurnoutAddresses[3] == 42);
}

TEST_CASE("NodeConfig4 withChannelAddress ignores channel 5, which was valid for the 12-channel panel")
{
    const NodeConfig4 original = NodeConfig4::factoryDefault();

    const NodeConfig4 updated = original.withChannelAddress(5, 42);

    for (const int address : updated.channelTurnoutAddresses)
    {
        REQUIRE(address == 0);
    }
}

TEST_CASE("NodeConfig4 validate rejects two channels claiming the same turnout address")
{
    const NodeConfig4 config = NodeConfig4::factoryDefault()
                                    .withNodeId(1)
                                    .withWifi("MyLayoutWifi", "hunter2")
                                    .withBroker("192.168.1.50", 1883)
                                    .withChannelAddress(1, 5)
                                    .withChannelAddress(2, 5);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("A fully valid NodeConfig4 with all 4 channels addressed passes validation")
{
    const NodeConfig4 config = NodeConfig4::factoryDefault()
                                    .withNodeId(1)
                                    .withWifi("MyLayoutWifi", "hunter2")
                                    .withBroker("192.168.1.50", 1883)
                                    .withChannelAddress(1, 1)
                                    .withChannelAddress(2, 2)
                                    .withChannelAddress(3, 3)
                                    .withChannelAddress(4, 4);

    REQUIRE(config.validate().empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_node_config4`
Expected: FAIL to compile — `NodeConfig4` is not declared.

- [ ] **Step 3: Replace `NodeConfig.h` with the templated form**

Replace the full contents of `lib/McsEsp32/src/domain/NodeConfig.h` with:

```cpp
#pragma once

#include <array>
#include <string>
#include <vector>

template <int N>
struct BasicNodeConfig
{
    static constexpr int kChannelCount = N;
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

    static BasicNodeConfig factoryDefault()
    {
        return BasicNodeConfig{};
    }

    [[nodiscard]] BasicNodeConfig withNodeId(const int id) const
    {
        BasicNodeConfig copy = *this;
        copy.nodeId = id;
        return copy;
    }

    [[nodiscard]] BasicNodeConfig withWifi(std::string ssid, std::string password) const
    {
        BasicNodeConfig copy = *this;
        copy.wifiSsid = std::move(ssid);
        copy.wifiPassword = std::move(password);
        return copy;
    }

    [[nodiscard]] BasicNodeConfig withBroker(std::string host, const int port) const
    {
        BasicNodeConfig copy = *this;
        copy.brokerHost = std::move(host);
        copy.brokerPort = port;
        return copy;
    }

    [[nodiscard]] BasicNodeConfig withChannelAddress(const int channel, const int address) const
    {
        BasicNodeConfig copy = *this;
        if (channel >= 1 && channel <= kChannelCount)
        {
            copy.channelTurnoutAddresses[channel - 1] = address;
        }
        return copy;
    }

    [[nodiscard]] std::vector<std::string> validate() const
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
                                  std::to_string(kMinTurnoutAddress) + " and " +
                                  std::to_string(kMaxTurnoutAddress));
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
};

using NodeConfig = BasicNodeConfig<12>;
using NodeConfig4 = BasicNodeConfig<4>;
```

- [ ] **Step 4: Delete `NodeConfig.cpp`**

```bash
git rm lib/McsEsp32/src/domain/NodeConfig.cpp
```

(Its body is now the header's inline template definitions — a template's method bodies must be visible at every instantiation site, so a separate `.cpp` can no longer hold them.)

- [ ] **Step 5: Run the existing test to confirm zero behavior change**

Run: `pio test -e native -f test_node_config`
Expected: PASS — all existing assertions against `NodeConfig` (the N=12 alias) succeed unchanged.

- [ ] **Step 6: Run the new test to confirm it passes**

Run: `pio test -e native -f test_node_config4`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/domain/NodeConfig.h test/test_node_config4/test_main.cpp
git add lib/McsEsp32/src/domain/NodeConfig.cpp
```

(The second `git add` stages the deletion.) Use `/arlo-commits` to compose and create the commit — do not hand-write the message.

---

## Task 2: Template `ConfigStore` and `CommissioningSession`

**Files:**
- Modify: `lib/McsEsp32/src/ports/ConfigStore.h`
- Modify: `lib/McsEsp32/src/application/CommissioningSession.h`
- Delete: `lib/McsEsp32/src/application/CommissioningSession.cpp`
- Create: `test/support/FakeConfigStore4.h`
- Test: `test/test_commissioning_session4/test_main.cpp` (new)
- Existing test (must keep passing unchanged): `test/test_commissioning_session/test_main.cpp`

**Interfaces:**
- Consumes: `BasicNodeConfig<N>` from Task 1.
- Produces: `template <int N> class BasicConfigStore { virtual BasicNodeConfig<N> load() = 0; virtual bool save(const BasicNodeConfig<N>&) = 0; };` with `using ConfigStore = BasicConfigStore<12>;` / `using ConfigStore4 = BasicConfigStore<4>;`. `template <int N> class BasicCommissioningSession` with constructor `explicit BasicCommissioningSession(BasicConfigStore<N>& store)`, methods `apply(const ParsedCommand&)`, `rebootRequested()`, `draft()` — same signatures as today's `CommissioningSession`. Aliases `using CommissioningSession = BasicCommissioningSession<12>;` / `using CommissioningSession4 = BasicCommissioningSession<4>;`.

- [ ] **Step 1: Write the failing test**

Create `test/support/FakeConfigStore4.h`:

```cpp
#pragma once

#include "domain/NodeConfig.h"
#include "ports/ConfigStore.h"

class FakeConfigStore4 final : public ConfigStore4
{
public:
    int saveCount = 0;
    bool failNextSave = false;

    NodeConfig4 load() override
    {
        return stored_;
    }

    bool save(const NodeConfig4& config) override
    {
        if (failNextSave)
        {
            failNextSave = false;
            return false;
        }
        stored_ = config;
        saveCount++;
        return true;
    }

private:
    NodeConfig4 stored_ = NodeConfig4::factoryDefault();
};
```

Create `test/test_commissioning_session4/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "application/CommissioningSession.h"
#include "support/FakeConfigStore4.h"

TEST_CASE("a turnout address command accepts channel 4")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);

    ParsedCommand command;
    command.kind = CommandKind::TurnoutAddress;
    command.intArg = 4;
    command.intArg2 = 7;

    const std::string response = session.apply(command);

    REQUIRE(response == "OK\n");
}

TEST_CASE("a turnout address command rejects channel 5, which was valid for the 12-channel panel")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);

    ParsedCommand command;
    command.kind = CommandKind::TurnoutAddress;
    command.intArg = 5;
    command.intArg2 = 7;

    const std::string response = session.apply(command);

    REQUIRE(response == "error: turnout channel must be between 1 and 4\n");
}

TEST_CASE("show reports exactly 4 turnout lines")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);

    ParsedCommand showCommand;
    showCommand.kind = CommandKind::Show;
    const std::string response = session.apply(showCommand);

    REQUIRE(response.find("turnout 4:") != std::string::npos);
    REQUIRE(response.find("turnout 5:") == std::string::npos);
}

TEST_CASE("save persists a valid draft with all 4 channels addressed")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);

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

    for (int channel = 1; channel <= 4; ++channel)
    {
        ParsedCommand addressCommand;
        addressCommand.kind = CommandKind::TurnoutAddress;
        addressCommand.intArg = channel;
        addressCommand.intArg2 = channel;
        session.apply(addressCommand);
    }

    ParsedCommand saveCommand;
    saveCommand.kind = CommandKind::Save;
    const std::string response = session.apply(saveCommand);

    REQUIRE(response == "saved\n");
    REQUIRE(store.saveCount == 1);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_commissioning_session4`
Expected: FAIL to compile — `ConfigStore4`/`CommissioningSession4` are not declared.

- [ ] **Step 3: Replace `ConfigStore.h` with the templated form**

Replace the full contents of `lib/McsEsp32/src/ports/ConfigStore.h` with:

```cpp
#pragma once

#include "../domain/NodeConfig.h"

template <int N>
class BasicConfigStore
{
public:
    virtual ~BasicConfigStore() = default;

    virtual BasicNodeConfig<N> load() = 0;
    virtual bool save(const BasicNodeConfig<N>& config) = 0;
};

using ConfigStore = BasicConfigStore<12>;
using ConfigStore4 = BasicConfigStore<4>;
```

- [ ] **Step 4: Replace `CommissioningSession.h` with the templated form**

Replace the full contents of `lib/McsEsp32/src/application/CommissioningSession.h` with:

```cpp
#pragma once

#include <string>
#include <vector>

#include "../domain/NodeConfig.h"
#include "../domain/ParsedCommand.h"
#include "../ports/ConfigStore.h"

template <int N>
class BasicCommissioningSession
{
public:
    explicit BasicCommissioningSession(BasicConfigStore<N>& store) : store_(store), draft_(store.load())
    {
    }

    std::string apply(const ParsedCommand& command)
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

        case CommandKind::TurnoutAddress:
            if (command.intArg < 1 || command.intArg > N)
            {
                return "error: turnout channel must be between 1 and " + std::to_string(N) + "\n";
            }
            draft_ = draft_.withChannelAddress(command.intArg, command.intArg2);
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
            if (!store_.save(draft_))
            {
                return "save failed: could not write to storage\n";
            }
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

    [[nodiscard]] bool rebootRequested() const
    {
        return rebootRequested_;
    }

    [[nodiscard]] const BasicNodeConfig<N>& draft() const
    {
        return draft_;
    }

private:
    [[nodiscard]] std::string formatErrors(const std::vector<std::string>& errors) const
    {
        std::string result = "invalid config:\n";
        for (const std::string& error : errors)
        {
            result += "  - " + error + "\n";
        }
        return result;
    }

    [[nodiscard]] std::string formatShow() const
    {
        std::string result;
        result += "id: " + std::to_string(draft_.nodeId) + "\n";
        result += "wifi ssid: " + (draft_.wifiSsid.empty() ? std::string("(unconfigured)") : draft_.wifiSsid) + "\n";
        result += "broker: " + (draft_.brokerHost.empty() ? std::string("(unconfigured)") : draft_.brokerHost) +
                  ":" + std::to_string(draft_.brokerPort) + "\n";
        for (int i = 0; i < N; ++i)
        {
            const int address = draft_.channelTurnoutAddresses[i];
            result += "turnout " + std::to_string(i + 1) + ": " +
                      (address == 0 ? std::string("(unconfigured)") : std::to_string(address)) + "\n";
        }
        return result;
    }

    BasicConfigStore<N>& store_;
    BasicNodeConfig<N> draft_;
    bool rebootRequested_ = false;
};

using CommissioningSession = BasicCommissioningSession<12>;
using CommissioningSession4 = BasicCommissioningSession<4>;
```

- [ ] **Step 5: Delete `CommissioningSession.cpp`**

```bash
git rm lib/McsEsp32/src/application/CommissioningSession.cpp
```

- [ ] **Step 6: Run the existing test to confirm zero behavior change**

Run: `pio test -e native -f test_commissioning_session`
Expected: PASS — unchanged.

- [ ] **Step 7: Run the new test to confirm it passes**

Run: `pio test -e native -f test_commissioning_session4`
Expected: PASS.

- [ ] **Step 8: Run the full native suite as a collateral-damage check**

Run: `pio test -e native`
Expected: All suites PASS (confirms `FakeConfigStore` in `test/support/` — used by many other suites — still compiles against the now-templated `ConfigStore` alias).

- [ ] **Step 9: Commit**

```bash
git add lib/McsEsp32/src/ports/ConfigStore.h lib/McsEsp32/src/application/CommissioningSession.h
git add lib/McsEsp32/src/application/CommissioningSession.cpp
git add test/support/FakeConfigStore4.h test/test_commissioning_session4/test_main.cpp
```

Use `/arlo-commits` to compose and create the commit.

---

## Task 3: Template `WebFormSubmission` and `WebFormCommissioningAdapter`

**Files:**
- Modify: `lib/McsEsp32/src/domain/WebFormSubmission.h`
- Modify: `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.h`
- Delete: `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp`
- Test: `test/test_web_form_commissioning_adapter4/test_main.cpp` (new)
- Existing test (must keep passing unchanged): `test/test_web_form_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes: `BasicNodeConfig<N>` (Task 1), `BasicConfigStore<N>`/`BasicCommissioningSession<N>` (Task 2).
- Produces: `template <int N> struct BasicWebFormSubmission { std::string nodeId, wifiSsid, wifiPassword, brokerHost, brokerPort; std::array<std::string, N> channelTurnoutAddresses; };` aliased as `WebFormSubmission`/`WebFormSubmission4`. `template <int N> class BasicWebFormCommissioningAdapter` with `explicit BasicWebFormCommissioningAdapter(BasicCommissioningSession<N>& session)`, `submit(const BasicWebFormSubmission<N>&)`, `rebootRequested()`, `currentValues()` — aliased as `WebFormCommissioningAdapter`/`WebFormCommissioningAdapter4`.

- [ ] **Step 1: Write the failing test**

Create `test/test_web_form_commissioning_adapter4/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/WebFormCommissioningAdapter.h"
#include "application/CommissioningSession.h"
#include "domain/NodeConfig.h"
#include "domain/WebFormSubmission.h"
#include "support/FakeConfigStore4.h"

namespace
{
    WebFormSubmission4 validSubmission()
    {
        WebFormSubmission4 form;
        form.nodeId = "5";
        form.wifiSsid = "MyLayoutWifi";
        form.wifiPassword = "hunter2";
        form.brokerHost = "192.168.1.50";
        form.brokerPort = "1883";
        form.channelTurnoutAddresses[0] = "17";
        form.channelTurnoutAddresses[3] = "20";
        return form;
    }
}

TEST_CASE("a fully valid 4-channel submission saves and requests reboot")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);
    WebFormCommissioningAdapter4 adapter(session);

    const std::string response = adapter.submit(validSubmission());

    REQUIRE(response == "rebooting\n");
    REQUIRE(adapter.rebootRequested());
    REQUIRE(store.saveCount == 1);
    REQUIRE(store.load().channelTurnoutAddresses[0] == 17);
    REQUIRE(store.load().channelTurnoutAddresses[3] == 20);
}

TEST_CASE("currentValues reports exactly 4 channel slots")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);
    WebFormCommissioningAdapter4 adapter(session);

    const WebFormSubmission4 values = adapter.currentValues();

    REQUIRE(values.channelTurnoutAddresses.size() == 4);
}

TEST_CASE("a blank channel field clears a previously stored channel address on the 4-channel node")
{
    FakeConfigStore4 store;
    store.save(NodeConfig4::factoryDefault()
                   .withNodeId(5)
                   .withWifi("w", "p")
                   .withBroker("h", 1883)
                   .withChannelAddress(2, 6));
    CommissioningSession4 session(store);
    WebFormCommissioningAdapter4 adapter(session);

    WebFormSubmission4 form = validSubmission();
    // form.channelTurnoutAddresses[1] (channel 2) is blank by default construction

    adapter.submit(form);

    REQUIRE(store.load().channelTurnoutAddresses[1] == 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_web_form_commissioning_adapter4`
Expected: FAIL to compile — `WebFormSubmission4`/`WebFormCommissioningAdapter4` are not declared.

- [ ] **Step 3: Replace `WebFormSubmission.h` with the templated form**

Replace the full contents of `lib/McsEsp32/src/domain/WebFormSubmission.h` with:

```cpp
#pragma once

#include <array>
#include <string>

#include "NodeConfig.h"

template <int N>
struct BasicWebFormSubmission
{
    std::string nodeId;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string brokerHost;
    std::string brokerPort;
    std::array<std::string, N> channelTurnoutAddresses;
};

using WebFormSubmission = BasicWebFormSubmission<12>;
using WebFormSubmission4 = BasicWebFormSubmission<4>;
```

(The `NodeConfig.h` include is kept even though this file no longer reads `kChannelCount` directly — `test/test_setup_form_renderer/test_main.cpp` relies on it transitively via `SetupFormRenderer.h` → `WebFormSubmission.h`, and removing it would silently break that file's compilation.)

- [ ] **Step 4: Replace `WebFormCommissioningAdapter.h` with the templated form**

Replace the full contents of `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.h` with:

```cpp
#pragma once

#include <string>

#include "../application/CommissioningSession.h"
#include "../domain/CommandLineParser.h"
#include "../domain/NodeConfig.h"
#include "../domain/ParsedCommand.h"
#include "../domain/WebFormSubmission.h"

template <int N>
class BasicWebFormCommissioningAdapter
{
public:
    explicit BasicWebFormCommissioningAdapter(BasicCommissioningSession<N>& session) : session_(session)
    {
    }

    std::string submit(const BasicWebFormSubmission<N>& form)
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

        for (int i = 0; i < N; ++i)
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

    [[nodiscard]] bool rebootRequested() const
    {
        return session_.rebootRequested();
    }

    [[nodiscard]] BasicWebFormSubmission<N> currentValues() const
    {
        const BasicNodeConfig<N>& config = session_.draft();

        BasicWebFormSubmission<N> form;
        form.nodeId = config.nodeId == 0 ? "" : std::to_string(config.nodeId);
        form.wifiSsid = config.wifiSsid;
        form.wifiPassword = "";
        form.brokerHost = config.brokerHost;
        form.brokerPort = std::to_string(config.brokerPort);
        for (int i = 0; i < N; ++i)
        {
            form.channelTurnoutAddresses[i] =
                config.channelTurnoutAddresses[i] == 0 ? "" : std::to_string(config.channelTurnoutAddresses[i]);
        }

        return form;
    }

private:
    BasicCommissioningSession<N>& session_;
};

using WebFormCommissioningAdapter = BasicWebFormCommissioningAdapter<12>;
using WebFormCommissioningAdapter4 = BasicWebFormCommissioningAdapter<4>;
```

- [ ] **Step 5: Delete `WebFormCommissioningAdapter.cpp`**

```bash
git rm lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp
```

- [ ] **Step 6: Run the existing test to confirm zero behavior change**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: PASS — unchanged.

- [ ] **Step 7: Run the new test to confirm it passes**

Run: `pio test -e native -f test_web_form_commissioning_adapter4`
Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add lib/McsEsp32/src/domain/WebFormSubmission.h lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.h
git add lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp
git add test/test_web_form_commissioning_adapter4/test_main.cpp
```

Use `/arlo-commits` to compose and create the commit.

---

## Task 4: `SetupFormRenderer::render()` becomes a function template

**Files:**
- Modify: `lib/McsEsp32/src/domain/SetupFormRenderer.h`
- Modify (append only): `test/test_setup_form_renderer/test_main.cpp`

**Interfaces:**
- Consumes: `BasicWebFormSubmission<N>` (Task 3).
- Produces: `template <int N> static std::string render(const BasicWebFormSubmission<N>& values, const std::vector<ScannedNetwork>& networks = {})` — same behavior as today's `render(const WebFormSubmission&, ...)`, generalized over `N`. `escapeHtml()` is unchanged (it never referenced channel count).

- [ ] **Step 1: Write the failing test**

Append this `TEST_CASE` to the end of `test/test_setup_form_renderer/test_main.cpp` (do not modify any existing case):

```cpp
TEST_CASE("render includes a labeled input for each of the 4 turnout channels on the node variant")
{
    WebFormSubmission4 form;
    form.channelTurnoutAddresses[0] = "5";
    form.channelTurnoutAddresses[3] = "600";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("Turnout 1 Address") != std::string::npos);
    REQUIRE(html.find("name='t1_address'") != std::string::npos);
    REQUIRE(html.find("value='5'") != std::string::npos);
    REQUIRE(html.find("Turnout 4 Address") != std::string::npos);
    REQUIRE(html.find("name='t4_address'") != std::string::npos);
    REQUIRE(html.find("value='600'") != std::string::npos);
    REQUIRE(html.find("Turnout 5 Address") == std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: FAIL to compile — `render()` doesn't yet accept a `BasicWebFormSubmission<4>` (`WebFormSubmission4`).

- [ ] **Step 3: Edit `SetupFormRenderer.h`**

In `lib/McsEsp32/src/domain/SetupFormRenderer.h`, change the `render` method's signature from:

```cpp
    static std::string render(const WebFormSubmission& values, const std::vector<ScannedNetwork>& networks = {})
```

to:

```cpp
    template <int N>
    static std::string render(const BasicWebFormSubmission<N>& values, const std::vector<ScannedNetwork>& networks = {})
```

And inside that method, change:

```cpp
        for (int i = 0; i < NodeConfig::kChannelCount; ++i)
        {
            html += renderChannelField(i + 1, values.channelTurnoutAddresses[i]);
        }
```

to:

```cpp
        for (int i = 0; i < N; ++i)
        {
            html += renderChannelField(i + 1, values.channelTurnoutAddresses[i]);
        }
```

No other lines in this file change. `renderIdOptions()`, `renderNetworkOptions()`, `renderChannelField()`, `escapeHtml()`, and `kStyle` are all channel-count-agnostic and stay exactly as they are.

- [ ] **Step 4: Run the existing suite to confirm zero behavior change**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: PASS — all existing cases (which call `render()` with a plain `WebFormSubmission`, i.e. `BasicWebFormSubmission<12>`, via template argument deduction) plus the new N=4 case.

- [ ] **Step 5: Commit**

```bash
git add lib/McsEsp32/src/domain/SetupFormRenderer.h test/test_setup_form_renderer/test_main.cpp
```

Use `/arlo-commits` to compose and create the commit.

---

## Task 5: `BootModeSelector::select()` becomes a function template

**Files:**
- Modify: `lib/McsEsp32/src/domain/BootModeSelector.h`
- Modify (append only): `test/test_boot_mode_selector/test_main.cpp`

**Interfaces:**
- Consumes: `BasicNodeConfig<N>` (Task 1).
- Produces: `template <int N> static BootMode select(const BasicNodeConfig<N>& config, bool wirelessSetupRequested)` — same behavior as today's `select(const NodeConfig&, bool)`, generalized over `N`.

This class was not in the original 8-class list in the design spec — it surfaced during planning because `src/esp32node4/main.cpp` (Task 9) needs boot-mode selection without putting that logic directly in the composition root (which the project's architecture rule forbids: "main.cpp is a composition root only ... must not contain business logic"). It gets the identical function-template treatment as `SetupFormRenderer::render()` in Task 4, since it's likewise a stateless static helper.

- [ ] **Step 1: Write the failing test**

Append these two `TEST_CASE`s to the end of `test/test_boot_mode_selector/test_main.cpp` (do not modify any existing case):

```cpp
TEST_CASE("a pending wireless setup request selects WirelessSetup for a 4-channel config")
{
    const NodeConfig4 config = NodeConfig4::factoryDefault();

    REQUIRE(BootModeSelector::select(config, true) == BootMode::WirelessSetup);
}

TEST_CASE("no request and an invalid 4-channel config selects NeedsCommissioning")
{
    const NodeConfig4 config = NodeConfig4::factoryDefault();

    REQUIRE(BootModeSelector::select(config, false) == BootMode::NeedsCommissioning);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_boot_mode_selector`
Expected: FAIL to compile — `select()` doesn't yet accept a `NodeConfig4`.

- [ ] **Step 3: Edit `BootModeSelector.h`**

Replace the full contents of `lib/McsEsp32/src/domain/BootModeSelector.h` with:

```cpp
#pragma once

#include "BootMode.h"
#include "NodeConfig.h"

class BootModeSelector
{
public:
    template <int N>
    static BootMode select(const BasicNodeConfig<N>& config, const bool wirelessSetupRequested)
    {
        if (wirelessSetupRequested)
        {
            return BootMode::WirelessSetup;
        }

        return config.validate().empty() ? BootMode::Normal : BootMode::NeedsCommissioning;
    }
};
```

- [ ] **Step 4: Run the existing suite to confirm zero behavior change**

Run: `pio test -e native -f test_boot_mode_selector`
Expected: PASS — all existing cases (calling `select()` with a plain `NodeConfig`) plus the two new N=4 cases.

- [ ] **Step 5: Commit**

```bash
git add lib/McsEsp32/src/domain/BootModeSelector.h test/test_boot_mode_selector/test_main.cpp
```

Use `/arlo-commits` to compose and create the commit.

---

## Task 6: Template `SerialCommissioningAdapter`

**Files:**
- Modify: `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h`
- Delete: `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.cpp`
- Test: `test/test_serial_commissioning_adapter4/test_main.cpp` (new)
- Existing test (must keep passing unchanged): `test/test_serial_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes: `BasicCommissioningSession<N>` (Task 2). Reuses the existing `UartPort` port and `test/support/FakeUartPort.h` as-is (neither references channel count).
- Produces: `template <int N> class BasicSerialCommissioningAdapter` with `BasicSerialCommissioningAdapter(UartPort& uart, BasicCommissioningSession<N>& session)`, `poll()`, `rebootRequested()`, `static constexpr size_t kMaxLineLength = 128` — aliased as `SerialCommissioningAdapter`/`SerialCommissioningAdapter4`.

- [ ] **Step 1: Write the failing test**

Create `test/test_serial_commissioning_adapter4/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/SerialCommissioningAdapter.h"
#include "support/FakeConfigStore4.h"
#include "support/FakeUartPort.h"

TEST_CASE("a turnout command within the 4-channel range is dispatched")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter4 adapter(uart, session);

    uart.queueInput("turnout 4 address 7\n");
    adapter.poll();

    REQUIRE(uart.written == "OK\n");
}

TEST_CASE("a turnout command outside the 4-channel range is rejected")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter4 adapter(uart, session);

    uart.queueInput("turnout 5 address 7\n");
    adapter.poll();

    REQUIRE(uart.written.rfind("error:", 0) == 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_serial_commissioning_adapter4`
Expected: FAIL to compile — `SerialCommissioningAdapter4` is not declared.

- [ ] **Step 3: Replace `SerialCommissioningAdapter.h` with the templated form**

Replace the full contents of `lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h` with:

```cpp
#pragma once

#include <cstddef>
#include <string>

#include "../application/CommissioningSession.h"
#include "../domain/CommandLineParser.h"
#include "../ports/UartPort.h"

template <int N>
class BasicSerialCommissioningAdapter
{
public:
    static constexpr size_t kMaxLineLength = 128;

    BasicSerialCommissioningAdapter(UartPort& uart, BasicCommissioningSession<N>& session)
        : uart_(uart), session_(session)
    {
    }

    void poll()
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

    [[nodiscard]] bool rebootRequested() const
    {
        return session_.rebootRequested();
    }

private:
    UartPort& uart_;
    BasicCommissioningSession<N>& session_;
    std::string lineBuffer_;
};

using SerialCommissioningAdapter = BasicSerialCommissioningAdapter<12>;
using SerialCommissioningAdapter4 = BasicSerialCommissioningAdapter<4>;
```

- [ ] **Step 4: Delete `SerialCommissioningAdapter.cpp`**

```bash
git rm lib/McsEsp32/src/adapters/SerialCommissioningAdapter.cpp
```

- [ ] **Step 5: Run the existing test to confirm zero behavior change**

Run: `pio test -e native -f test_serial_commissioning_adapter`
Expected: PASS — unchanged.

- [ ] **Step 6: Run the new test to confirm it passes**

Run: `pio test -e native -f test_serial_commissioning_adapter4`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/adapters/SerialCommissioningAdapter.h
git add lib/McsEsp32/src/adapters/SerialCommissioningAdapter.cpp
git add test/test_serial_commissioning_adapter4/test_main.cpp
```

Use `/arlo-commits` to compose and create the commit.

---

## Task 7: Template `NvsConfigStore` (hardware adapter, no native test)

**Files:**
- Modify: `lib/McsEsp32/src/adapters/NvsConfigStore.h`
- Modify: `lib/McsEsp32/src/adapters/NvsConfigStore.cpp`

**Interfaces:**
- Consumes: `BasicConfigStore<N>` (Task 2), `BasicNodeConfig<N>` (Task 1).
- Produces: `template <int N> class BasicNvsConfigStore final : public BasicConfigStore<N>` — aliased as `NvsConfigStore`/`NvsConfigStore4`.

This class is `#ifdef ARDUINO`-guarded and has no native test today (confirmed: no `test_nvs_config_store` directory exists) — its NVS-dependent body only compiles on hardware. This task's verification is therefore an `esp32dev`/`esp32node4` build, not a native test run, matching the existing testability note in `CLAUDE.md` for this same class.

**Do not change the NVS namespace (`"mcsnode"`) or any key name** — both variants are always different physical devices running different firmware, so there is no collision risk, and this preserves the hard constraint on not changing the 12-channel build's NVS keys.

- [ ] **Step 1: Replace `NvsConfigStore.h` with the templated form**

Replace the full contents of `lib/McsEsp32/src/adapters/NvsConfigStore.h` with:

```cpp
#pragma once

#include "../ports/ConfigStore.h"

template <int N>
class BasicNvsConfigStore final : public BasicConfigStore<N>
{
public:
    BasicNodeConfig<N> load() override;
    bool save(const BasicNodeConfig<N>& config) override;
};

using NvsConfigStore = BasicNvsConfigStore<12>;
using NvsConfigStore4 = BasicNvsConfigStore<4>;
```

- [ ] **Step 2: Replace `NvsConfigStore.cpp` with the templated form**

Replace the full contents of `lib/McsEsp32/src/adapters/NvsConfigStore.cpp` with:

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

template <int N>
BasicNodeConfig<N> BasicNvsConfigStore<N>::load()
{
    Preferences prefs;
    prefs.begin(kNamespace, true);

    BasicNodeConfig<N> config = BasicNodeConfig<N>::factoryDefault();
    config.nodeId = prefs.getInt(kKeyNodeId, 0);
    config.wifiSsid = prefs.getString(kKeyWifiSsid, "").c_str();
    config.wifiPassword = prefs.getString(kKeyWifiPassword, "").c_str();
    config.brokerHost = prefs.getString(kKeyBrokerHost, "").c_str();
    config.brokerPort = prefs.getInt(kKeyBrokerPort, 1883);

    for (int i = 0; i < N; ++i)
    {
        const std::string key = std::string(kKeyChannelPrefix) + std::to_string(i);
        config.channelTurnoutAddresses[i] = prefs.getInt(key.c_str(), 0);
    }

    prefs.end();
    return config;
}

template <int N>
bool BasicNvsConfigStore<N>::save(const BasicNodeConfig<N>& config)
{
    Preferences prefs;
    bool ok = prefs.begin(kNamespace, false);

    ok = prefs.putInt(kKeyNodeId, config.nodeId) > 0 && ok;
    // putString()'s return (bytes written) is not checked here: ESP-IDF's
    // Preferences::putString() returns 0 both on failure AND when writing a
    // legitimate empty string (e.g. an unconfigured wifiPassword, which
    // defaults to "" and is valid per NodeConfig::validate()'s
    // partial-commissioning rules). ANDing that into `ok` would report
    // failure on every normal partial-commissioning save, so only
    // prefs.begin() and the putInt calls (which always write a fixed
    // non-zero byte count on success, regardless of the value stored —
    // unlike putString, a stored 0 is not ambiguous with failure) are
    // trusted.
    prefs.putString(kKeyWifiSsid, config.wifiSsid.c_str());
    prefs.putString(kKeyWifiPassword, config.wifiPassword.c_str());
    prefs.putString(kKeyBrokerHost, config.brokerHost.c_str());
    ok = prefs.putInt(kKeyBrokerPort, config.brokerPort) > 0 && ok;

    for (int i = 0; i < N; ++i)
    {
        const std::string key = std::string(kKeyChannelPrefix) + std::to_string(i);
        ok = prefs.putInt(key.c_str(), config.channelTurnoutAddresses[i]) > 0 && ok;
    }

    prefs.end();
    return ok;
}

template class BasicNvsConfigStore<12>;
template class BasicNvsConfigStore<4>;

#endif
```

- [ ] **Step 3: Run the native suite (confirms nothing outside the `#ifdef ARDUINO` block broke)**

Run: `pio test -e native`
Expected: All suites PASS (this file compiles to nothing under `native`, since `ARDUINO` is undefined there — this step is a sanity check that the header's non-guarded declarations are still well-formed).

- [ ] **Step 4: Build the panel firmware to confirm zero behavior change**

Run: `pio run -e esp32dev`
Expected: Build succeeds, same as before this change.

- [ ] **Step 5: Commit**

```bash
git add lib/McsEsp32/src/adapters/NvsConfigStore.h lib/McsEsp32/src/adapters/NvsConfigStore.cpp
```

Use `/arlo-commits` to compose and create the commit.

---

## Task 8: Template `CaptivePortalServer` (hardware adapter, no native test)

**Files:**
- Modify: `lib/McsEsp32/src/adapters/CaptivePortalServer.h`
- Modify: `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp`

**Interfaces:**
- Consumes: `BasicWebFormCommissioningAdapter<N>`, `BasicWebFormSubmission<N>` (Task 3).
- Produces: `template <int N> class BasicCaptivePortalServer` with `explicit BasicCaptivePortalServer(BasicWebFormCommissioningAdapter<N>& adapter)`, `begin(const std::string&)`, `poll()` — aliased as `CaptivePortalServer`/`CaptivePortalServer4`.

Same situation as Task 7: `#ifdef ARDUINO`-guarded, no native test today, verified via hardware build.

- [ ] **Step 1: Replace `CaptivePortalServer.h` with the templated form**

Replace the full contents of `lib/McsEsp32/src/adapters/CaptivePortalServer.h` with:

```cpp
#pragma once

#ifdef ARDUINO

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include <string>
#include <vector>

#include "WebFormCommissioningAdapter.h"
#include "../domain/SetupFormRenderer.h"
#include "../domain/WifiScanFormatter.h"

template <int N>
class BasicCaptivePortalServer
{
public:
    explicit BasicCaptivePortalServer(BasicWebFormCommissioningAdapter<N>& adapter);

    void begin(const std::string& apName);
    void poll();

private:
    void handleRoot();
    void handleSubmit();
    void handleRescan();
    void scanNetworks();
    BasicWebFormSubmission<N> readForm();

    BasicWebFormCommissioningAdapter<N>& adapter_;
    DNSServer dnsServer_;
    WebServer webServer_{80};
    std::vector<ScannedNetwork> scannedNetworks_;
};

using CaptivePortalServer = BasicCaptivePortalServer<12>;
using CaptivePortalServer4 = BasicCaptivePortalServer<4>;

#endif
```

- [ ] **Step 2: Replace `CaptivePortalServer.cpp` with the templated form**

Replace the full contents of `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp` with:

```cpp
#ifdef ARDUINO

#include "CaptivePortalServer.h"

#include "../domain/NodeConfig.h"

template <int N>
BasicCaptivePortalServer<N>::BasicCaptivePortalServer(BasicWebFormCommissioningAdapter<N>& adapter)
    : adapter_(adapter)
{
}

template <int N>
void BasicCaptivePortalServer<N>::begin(const std::string& apName)
{
    WiFi.softAP(apName.c_str());

    IPAddress apIp = WiFi.softAPIP();
    dnsServer_.start(53, "*", apIp);

    webServer_.on("/", [this]() { handleRoot(); });
    webServer_.on("/submit", HTTP_POST, [this]() { handleSubmit(); });
    webServer_.on("/rescan", [this]() { handleRescan(); });
    webServer_.onNotFound([this]() { handleRoot(); });
    webServer_.begin();

    scanNetworks();
}

template <int N>
void BasicCaptivePortalServer<N>::poll()
{
    dnsServer_.processNextRequest();
    webServer_.handleClient();
}

template <int N>
void BasicCaptivePortalServer<N>::handleRoot()
{
    webServer_.send(200, "text/html; charset=utf-8",
                     SetupFormRenderer::render(adapter_.currentValues(), scannedNetworks_).c_str());
}

template <int N>
void BasicCaptivePortalServer<N>::handleSubmit()
{
    const BasicWebFormSubmission<N> form = readForm();
    const std::string response = adapter_.submit(form);
    webServer_.send(200, "text/plain", response.c_str());
}

template <int N>
void BasicCaptivePortalServer<N>::handleRescan()
{
    scanNetworks();
    webServer_.sendHeader("Location", "/");
    webServer_.send(302);
}

template <int N>
void BasicCaptivePortalServer<N>::scanNetworks()
{
    std::vector<ScannedNetwork> raw;
    const int16_t count = WiFi.scanNetworks();
    for (int16_t i = 0; i < count; ++i)
    {
        raw.push_back({std::string(WiFi.SSID(i).c_str()), WiFi.RSSI(i)});
    }
    scannedNetworks_ = WifiScanFormatter::dedupeAndSort(raw);
}

template <int N>
BasicWebFormSubmission<N> BasicCaptivePortalServer<N>::readForm()
{
    BasicWebFormSubmission<N> form;
    form.nodeId = webServer_.arg("id").c_str();
    form.wifiSsid = webServer_.arg("wifi_ssid").c_str();
    form.wifiPassword = webServer_.arg("wifi_password").c_str();
    form.brokerHost = webServer_.arg("broker_host").c_str();
    form.brokerPort = webServer_.arg("broker_port").c_str();

    for (int i = 0; i < N; ++i)
    {
        const std::string fieldName = "t" + std::to_string(i + 1) + "_address";
        form.channelTurnoutAddresses[i] = webServer_.arg(fieldName.c_str()).c_str();
    }

    return form;
}

template class BasicCaptivePortalServer<12>;
template class BasicCaptivePortalServer<4>;

#endif
```

- [ ] **Step 3: Run the native suite (confirms nothing outside the `#ifdef ARDUINO` block broke)**

Run: `pio test -e native`
Expected: All suites PASS.

- [ ] **Step 4: Build the panel firmware to confirm zero behavior change**

Run: `pio run -e esp32dev`
Expected: Build succeeds, same as before this change — this is the last task touching shared `McsEsp32` code, so this build is the final proof the 12-channel panel is unaffected by the whole templating effort.

- [ ] **Step 5: Commit**

```bash
git add lib/McsEsp32/src/adapters/CaptivePortalServer.h lib/McsEsp32/src/adapters/CaptivePortalServer.cpp
```

Use `/arlo-commits` to compose and create the commit.

---

## Task 9: New `esp32node4` environment and placeholder composition root

**Files:**
- Modify: `platformio.ini`
- Create: `src/esp32node4/main.cpp`

**Interfaces:**
- Consumes: `NodeConfig4`, `ConfigStore4`/`NvsConfigStore4`, `CommissioningSession4`, `SerialCommissioningAdapter4`, `WebFormCommissioningAdapter4`, `CaptivePortalServer4` (all prior tasks). Also reuses, unchanged: `EspUartPort`, `NvsSetupModeRequestStore`, `BootMode`, `BootModeSelector::select<N>()`, `EspDeviceIdentity`, `MacAddress`, `SetupApName`.
- Produces: a working `esp32node4` PlatformIO environment.

- [ ] **Step 1: Add the new environment to `platformio.ini`**

In `platformio.ini`, insert a new `[env:esp32node4]` block immediately after the existing `[env:esp32dev]` block (before `[env:native]`):

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

Leave every other section of `platformio.ini` untouched — `default_envs` stays `megaatmega2560`, and `[env:esp32dev]`/`[env:megaatmega2560]`/`[env:native]` are not modified.

- [ ] **Step 2: Write the placeholder composition root**

Create `src/esp32node4/main.cpp`:

```cpp
#include <Arduino.h>

#include <string>

#include <nvs_flash.h>

#include "adapters/CaptivePortalServer.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspUartPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "application/CommissioningSession.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/MacAddress.h"
#include "domain/NodeConfig.h"
#include "domain/SetupApName.h"

namespace
{
    constexpr unsigned long UART_BAUD_RATE = 115200;
}

namespace
{
    struct NvsBootstrap
    {
        NvsBootstrap()
        {
            // ESP-IDF runs global constructors before app_main(), so Arduino's
            // initArduino() has not yet called nvs_flash_init() when the globals
            // below read NVS. Initialize it here (idempotent - initArduino()'s
            // later call returns ESP_OK) so this boot's config is the one
            // commissioning actually saved.
            esp_err_t err = nvs_flash_init();
            if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                nvs_flash_erase();
                nvs_flash_init();
            }
        }
    };
}

NvsBootstrap nvsBootstrap;

const MacAddress ownMac = EspDeviceIdentity().mac();

EspUartPort uartPort(UART_BAUD_RATE);
NvsConfigStore4 configStore;
CommissioningSession4 commissioningSession(configStore);
SerialCommissioningAdapter4 serialCommissioningAdapter(uartPort, commissioningSession);
WebFormCommissioningAdapter4 webFormAdapter(commissioningSession);
CaptivePortalServer4 captivePortalServer(webFormAdapter);

NvsSetupModeRequestStore setupModeRequestStore;
NodeConfig4 runningConfig = configStore.load();
const bool wirelessSetupRequested = setupModeRequestStore.consumeRequest();
const BootMode bootMode = BootModeSelector::select(runningConfig, wirelessSetupRequested);
const bool configValid = runningConfig.validate().empty();

void setup()
{
    uartPort.begin();

    if (bootMode == BootMode::WirelessSetup)
    {
        const std::string apName = SetupApName::from(ownMac);
        captivePortalServer.begin(apName);
        uartPort.write("MaltBee node in wireless setup mode. Connect to " + apName + "\n");
        return;
    }

    uartPort.write(configValid ? "MaltBee node ready (configured).\n"
                                : "MaltBee node needs commissioning. Type 'show'.\n");
}

void loop()
{
    serialCommissioningAdapter.poll();
    if (serialCommissioningAdapter.rebootRequested())
    {
        Serial.flush();
        ESP.restart();
    }

    if (bootMode == BootMode::WirelessSetup)
    {
        captivePortalServer.poll();
    }
}
```

This composition root deliberately does not yet wire WiFi/MQTT/turnout stations/LEDs/buttons — those are later sub-projects (#2-#4 for stations/LEDs/buttons, and WiFi/MQTT wiring follows once there's a turnout command/feedback path worth connecting). It boots, selects boot mode, and supports both serial commissioning (`SerialCommissioningAdapter4`) and wireless commissioning (`CaptivePortalServer4`) — the two things sub-project #1 commits to delivering.

- [ ] **Step 3: Build the new environment**

Run: `pio run -e esp32node4`
Expected: Build succeeds.

- [ ] **Step 4: Build the panel and Mega environments to confirm they're unaffected**

Run: `pio run -e esp32dev`
Run: `pio run -e megaatmega2560`
Expected: Both succeed, unchanged from before this task.

- [ ] **Step 5: Commit**

```bash
git add platformio.ini src/esp32node4/main.cpp
```

Use `/arlo-commits` to compose and create the commit.

---

## Task 10: Final full-suite verification

**Files:** none (verification only).

- [ ] **Step 1: Run the full native test suite**

Run: `pio test -e native`
Expected: All 44 pre-existing suites pass unchanged, plus the 5 new N=4 suites added in Tasks 1, 2, 3, 6 (`test_node_config4`, `test_commissioning_session4`, `test_web_form_commissioning_adapter4`, `test_serial_commissioning_adapter4`) and the appended cases in Tasks 4 and 5 (`test_setup_form_renderer`, `test_boot_mode_selector`) — 49 suites total, all green.

- [ ] **Step 2: Build every firmware environment**

Run: `pio run -e esp32dev`
Run: `pio run -e esp32node4`
Run: `pio run -e megaatmega2560`
Expected: All three succeed.

- [ ] **Step 3: Report to the user**

Summarize: all 44 pre-existing native suites pass unchanged (proof of zero 12-channel behavior change), 5 new suites + 2 appended test files cover the N=4 instantiation, all four PlatformIO environments build, and `src/esp32node4/main.cpp` is a placeholder that boots/commissions but does not yet drive turnout hardware (sub-projects #2-#4 next). No commit in this task — it's a verification checkpoint, not a code change.

---

## Self-Review Notes

- **Spec coverage:** every file the spec's "Template boundary and naming" table lists is covered (Tasks 1-3, 6-8); `SetupFormRenderer` (Task 4) and the new PlatformIO environment + composition root (Task 9) are covered; `CommandLineParser`/`ParsedCommand` are confirmed unchanged and touched by no task. `BootModeSelector` (Task 5) was added beyond the spec's original 8-class list — it surfaced during planning as a composition-root dependency that would otherwise force business logic into `main.cpp`; it uses the same function-template pattern already approved for `SetupFormRenderer`, so no new design decision was introduced, just an additional instance of an already-approved pattern.
- **Placeholder scan:** no TBD/TODO; every step has complete, exact code.
- **Type consistency:** `BasicNodeConfig<N>`, `BasicConfigStore<N>`, `BasicCommissioningSession<N>`, `BasicWebFormSubmission<N>`, `BasicWebFormCommissioningAdapter<N>`, `BasicSerialCommissioningAdapter<N>`, `BasicNvsConfigStore<N>`, `BasicCaptivePortalServer<N>` are spelled identically everywhere they're consumed across tasks; aliases (`NodeConfig4`, `ConfigStore4`, etc.) match between the task that defines them and every later task/test that uses them.
