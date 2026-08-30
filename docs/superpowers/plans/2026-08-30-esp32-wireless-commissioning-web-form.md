# ESP32 Wireless Commissioning Web Form (Sub-project #2c-b1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the web form + captive-portal server classes for wireless ESP32 panel commissioning — all native-testable except the one hardware shim that actually opens WiFi/DNS/HTTP.

**Architecture:** Pure domain/adapter classes reusing `CommissioningSession`/`CommandLineParser`/`NodeConfig` unchanged (plus one additive accessor on `CommissioningSession`). No `main.cpp` wiring — that's sub-project #2c-b2.

**Tech Stack:** C++17, PlatformIO (`native` for tests, `esp32dev` build-check for the one hardware shim), Catch2, Arduino-ESP32's `WiFi`/`DNSServer`/`WebServer` libraries and ESP-IDF's `esp_mac.h`.

## Global Constraints

- **`CommandLineParser` has no quote handling** — it's plain whitespace tokenization (`istringstream >> token`), and `wifi`/`turnout ... name` require exact token counts. Wrapping a value in `\"..\"` would embed the literal quote characters into the stored value, and any embedded space breaks tokenization outright. **Resolution (confirmed by reading `CommandLineParser.cpp` directly, not assumed):** for the `wifi` and `turnout N name X` commands specifically — the two whose string arguments could contain spaces — construct `ParsedCommand` directly (bypassing `CommandLineParser::parse()` entirely), exactly the way this project's own `test_commissioning_session` suite already does throughout. For `id`, `broker`, `save`, and `reboot` (numeric/fixed-token, safe to tokenize), route through `CommandLineParser::parse()` as normal.
- Exact `CommissioningSession::apply()` response strings (verified from `CommissioningSession.cpp`, do not guess a different value): `"OK\n"` (Id/Wifi/Broker/TurnoutName success), `"saved\n"` (Save success), `"rebooting\n"` (Reboot, never fails), `"invalid config:\n  - ...\n"` (Save validation failure), `"save failed: could not write to storage\n"` (Save storage failure), `"error: ...\n"` (Invalid command / out-of-range channel).
- AP name prefix: `"MaltBee-Setup-"` + `MacAddress::lastFourHexDigits()`.
- Broker host is a single plain-text field — no IPv4-octet splitting.
- `EspDeviceIdentity::mac()` uses `esp_efuse_mac_get_default(uint8_t[6])` from `esp_mac.h`, not `WiFi.macAddress()`.
- `CaptivePortalServer` takes the AP name as a `std::string` via `begin(apName)`, never computes it internally from a `MacAddress`.
- No `Arduino.h` dependency anywhere except the two `#ifdef ARDUINO`-guarded hardware shims (`EspDeviceIdentity`, `CaptivePortalServer`) — build-check only via `pio run -e esp32dev`, no native test, same convention as `NvsConfigStore`/`WiFiLink`/`MqttLink`.
- This plan does not touch `src/esp32/main.cpp`, `BootMode`, `ComboSetupModeTrigger`, `GatedDigitalInput`, or any part of the suppression-timing question — all sub-project #2c-b2.
- `pio test -e native` must stay green throughout (32 suites before this plan starts), and `pio run -e megaatmega2560` must build unchanged.

---

### Task 1: `MacAddress` and `SetupApName`

**Files:**
- Create: `lib/McsEsp32/src/domain/MacAddress.h`
- Create: `lib/McsEsp32/src/domain/SetupApName.h`
- Test: `test/test_mac_address/test_main.cpp`
- Test: `test/test_setup_ap_name/test_main.cpp`

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces (consumed by Task 2):
  ```cpp
  class MacAddress
  {
  public:
      explicit MacAddress(std::array<uint8_t, 6> bytes);
      [[nodiscard]] const std::array<uint8_t, 6>& bytes() const;
      [[nodiscard]] std::string lastFourHexDigits() const;
      bool operator==(const MacAddress& other) const;
      bool operator!=(const MacAddress& other) const;
  };

  class SetupApName
  {
  public:
      static std::string from(const MacAddress& mac);
  };
  ```

- [ ] **Step 1: Write the failing tests**

Create `test/test_mac_address/test_main.cpp`:

```cpp
#include <array>

#include <catch2/catch_test_macros.hpp>

#include "domain/MacAddress.h"

TEST_CASE("lastFourHexDigits formats the last two bytes as uppercase hex")
{
    const MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF});

    REQUIRE(mac.lastFourHexDigits() == "CDEF");
}

TEST_CASE("lastFourHexDigits zero-pads a byte with a high nibble of zero")
{
    const MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0x01, 0x02});

    REQUIRE(mac.lastFourHexDigits() == "0102");
}

TEST_CASE("bytes returns the exact constructed byte array")
{
    const std::array<uint8_t, 6> bytes{0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    const MacAddress mac(bytes);

    REQUIRE(mac.bytes() == bytes);
}

TEST_CASE("equal byte arrays compare equal")
{
    const MacAddress a({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    const MacAddress b({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});

    REQUIRE(a == b);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("different byte arrays compare not equal")
{
    const MacAddress a({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
    const MacAddress b({0x01, 0x02, 0x03, 0x04, 0x05, 0x07});

    REQUIRE(a != b);
    REQUIRE_FALSE(a == b);
}
```

Create `test/test_setup_ap_name/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/SetupApName.h"

TEST_CASE("from builds the expected AP name from a MAC address")
{
    const MacAddress mac({0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF});

    REQUIRE(SetupApName::from(mac) == "MaltBee-Setup-CDEF");
}
```

- [ ] **Step 2: Run the tests to verify they fail to compile**

Run: `pio test -e native -f test_mac_address`
Run: `pio test -e native -f test_setup_ap_name`
Expected: both FAIL — the headers don't exist yet.

- [ ] **Step 3: Write `MacAddress.h`**

Create `lib/McsEsp32/src/domain/MacAddress.h`:

```cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>

class MacAddress
{
public:
    explicit MacAddress(std::array<uint8_t, 6> bytes) : bytes_(bytes)
    {
    }

    [[nodiscard]] const std::array<uint8_t, 6>& bytes() const
    {
        return bytes_;
    }

    [[nodiscard]] std::string lastFourHexDigits() const
    {
        static const char* kHexDigits = "0123456789ABCDEF";
        std::string result;
        result += kHexDigits[(bytes_[4] >> 4) & 0x0F];
        result += kHexDigits[bytes_[4] & 0x0F];
        result += kHexDigits[(bytes_[5] >> 4) & 0x0F];
        result += kHexDigits[bytes_[5] & 0x0F];
        return result;
    }

    bool operator==(const MacAddress& other) const
    {
        return bytes_ == other.bytes_;
    }

    bool operator!=(const MacAddress& other) const
    {
        return !(*this == other);
    }

private:
    std::array<uint8_t, 6> bytes_;
};
```

- [ ] **Step 4: Write `SetupApName.h`**

Create `lib/McsEsp32/src/domain/SetupApName.h`:

```cpp
#pragma once

#include <string>

#include "MacAddress.h"

class SetupApName
{
public:
    static std::string from(const MacAddress& mac)
    {
        return "MaltBee-Setup-" + mac.lastFourHexDigits();
    }
};
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `pio test -e native -f test_mac_address`
Run: `pio test -e native -f test_setup_ap_name`
Expected: PASS — 5 test cases in `test_mac_address`, 1 in `test_setup_ap_name`.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites pass (34/34 including the two new ones).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/domain/MacAddress.h lib/McsEsp32/src/domain/SetupApName.h test/test_mac_address/test_main.cpp test/test_setup_ap_name/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add MacAddress and SetupApName
EOF
)"
```

---

### Task 2: `EspDeviceIdentity`

**Files:**
- Create: `lib/McsEsp32/src/adapters/EspDeviceIdentity.h`
- Create: `lib/McsEsp32/src/adapters/EspDeviceIdentity.cpp`

**Interfaces:**
- Consumes: `MacAddress` from Task 1.
- Produces (consumed by sub-project #2c-b2, not by any task in this plan): `class EspDeviceIdentity { public: [[nodiscard]] MacAddress mac() const; };`

No native test — real hardware shim, build-check only, same convention as `NvsConfigStore`.

- [ ] **Step 1: Write the header**

Create `lib/McsEsp32/src/adapters/EspDeviceIdentity.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include "../domain/MacAddress.h"

class EspDeviceIdentity
{
public:
    [[nodiscard]] MacAddress mac() const;
};

#endif
```

- [ ] **Step 2: Write the implementation**

Create `lib/McsEsp32/src/adapters/EspDeviceIdentity.cpp`:

```cpp
#ifdef ARDUINO

#include "EspDeviceIdentity.h"

#include <esp_mac.h>

#include <array>

MacAddress EspDeviceIdentity::mac() const
{
    std::array<uint8_t, 6> bytes{};
    esp_efuse_mac_get_default(bytes.data());
    return MacAddress(bytes);
}

#endif
```

- [ ] **Step 3: Build-check the ESP32 target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — `esp32dev`'s `lib_deps` already includes `McsEsp32` as a full library dependency, so this new `.cpp` is compiled automatically without needing a `main.cpp` reference.

- [ ] **Step 4: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites still pass (34/34, unchanged from Task 1 — this task adds no native test).

- [ ] **Step 5: Commit**

```bash
git add lib/McsEsp32/src/adapters/EspDeviceIdentity.h lib/McsEsp32/src/adapters/EspDeviceIdentity.cpp
git commit -m "$(cat <<'EOF'
^ F Add EspDeviceIdentity
EOF
)"
```

---

### Task 3: `WebFormSubmission`

**Files:**
- Create: `lib/McsEsp32/src/domain/WebFormSubmission.h`

**Interfaces:**
- Consumes: `NodeConfig::kChannelCount` (`lib/McsEsp32/src/domain/NodeConfig.h`).
- Produces (consumed by Tasks 4, 5, 6):
  ```cpp
  struct WebFormSubmission
  {
      std::string nodeId;
      std::string wifiSsid;
      std::string wifiPassword;
      std::string brokerHost;
      std::string brokerPort;
      std::array<std::string, NodeConfig::kChannelCount> channelJmriNames;
  };
  ```

This is a plain data struct with no behavior — no dedicated test file, matching this project's existing precedent (`ParsedCommand`, also a plain data struct, has no `test/test_parsed_command/` directory; confirmed by checking the `test/` directory listing before writing this plan). It's exercised indirectly through Tasks 4 and 5's tests.

- [ ] **Step 1: Write the file**

Create `lib/McsEsp32/src/domain/WebFormSubmission.h`:

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
    std::array<std::string, NodeConfig::kChannelCount> channelJmriNames;
};
```

- [ ] **Step 2: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites still pass (34/34, unchanged — this task adds no test and nothing includes this header yet).

- [ ] **Step 3: Commit**

```bash
git add lib/McsEsp32/src/domain/WebFormSubmission.h
git commit -m "$(cat <<'EOF'
^ F Add WebFormSubmission
EOF
)"
```

---

### Task 4: `CommissioningSession::draft()` and `WebFormCommissioningAdapter`

**Files:**
- Modify: `lib/McsEsp32/src/application/CommissioningSession.h`
- Modify: `lib/McsEsp32/src/application/CommissioningSession.cpp`
- Modify: `test/test_commissioning_session/test_main.cpp`
- Create: `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.h`
- Create: `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp`
- Test: `test/test_web_form_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes:
  - `WebFormSubmission` from Task 3.
  - `CommissioningSession` (`lib/McsEsp32/src/application/CommissioningSession.h`) — `explicit CommissioningSession(ConfigStore&)`, `std::string apply(const ParsedCommand&)`, `bool rebootRequested() const`, plus the new `draft()` this task adds.
  - `ParsedCommand`/`CommandKind` (`lib/McsEsp32/src/domain/ParsedCommand.h`) — `struct ParsedCommand { CommandKind kind = CommandKind::Invalid; int intArg = 0; std::string stringArg1; std::string stringArg2; int intArg2 = 0; std::string errorMessage; };`
  - `CommandLineParser::parse(const std::string&)` (`lib/McsEsp32/src/domain/CommandLineParser.h`).
  - `NodeConfig` (`lib/McsEsp32/src/domain/NodeConfig.h`) — `nodeId`, `wifiSsid`, `wifiPassword`, `brokerHost`, `brokerPort`, `channelJmriNames`, `kChannelCount`.
  - `FakeConfigStore` (`test/support/FakeConfigStore.h`) — `int saveCount`, `bool failNextSave`, `NodeConfig load()`, `bool save(const NodeConfig&)`.
- Produces (consumed by Task 6):
  ```cpp
  // CommissioningSession gains:
  [[nodiscard]] const NodeConfig& draft() const;

  class WebFormCommissioningAdapter
  {
  public:
      explicit WebFormCommissioningAdapter(CommissioningSession& session);
      std::string submit(const WebFormSubmission& form);
      [[nodiscard]] bool rebootRequested() const;
      [[nodiscard]] WebFormSubmission currentValues() const;
  };
  ```

- [ ] **Step 1: Write the failing test for `CommissioningSession::draft()`**

Add to the end of `test/test_commissioning_session/test_main.cpp`:

```cpp

TEST_CASE("draft reflects the current in-progress config, including after a mutation")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    REQUIRE(session.draft().nodeId == 0);

    ParsedCommand idCommand;
    idCommand.kind = CommandKind::Id;
    idCommand.intArg = 5;
    session.apply(idCommand);

    REQUIRE(session.draft().nodeId == 5);
}
```

- [ ] **Step 2: Run it to verify it fails to compile**

Run: `pio test -e native -f test_commissioning_session`
Expected: FAIL — `CommissioningSession` has no `draft()` method yet.

- [ ] **Step 3: Add `draft()` to `CommissioningSession`**

In `lib/McsEsp32/src/application/CommissioningSession.h`, add this line inside the `public:` section, right after the `rebootRequested()` declaration:

```cpp
    [[nodiscard]] const NodeConfig& draft() const;
```

In `lib/McsEsp32/src/application/CommissioningSession.cpp`, add this definition at the end of the file:

```cpp

const NodeConfig& CommissioningSession::draft() const
{
    return draft_;
}
```

- [ ] **Step 4: Run it to verify it passes**

Run: `pio test -e native -f test_commissioning_session`
Expected: PASS — all cases in this suite, including the new one.

- [ ] **Step 5: Write the failing tests for `WebFormCommissioningAdapter`**

Create `test/test_web_form_commissioning_adapter/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/WebFormCommissioningAdapter.h"
#include "application/CommissioningSession.h"
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
        form.channelJmriNames[0] = "LT1";
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
    REQUIRE(store.load().channelJmriNames[0] == "LT1");
}

TEST_CASE("an empty channel name is skipped, not sent as an empty turnout command")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const WebFormSubmission form = validSubmission();
    // channelJmriNames[1..11] are empty by default construction

    adapter.submit(form);

    REQUIRE(store.load().channelJmriNames[1].empty());
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
```

- [ ] **Step 6: Run it to verify it fails to compile**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: FAIL — `WebFormCommissioningAdapter.h` does not exist yet.

- [ ] **Step 7: Write the header**

Create `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.h`:

```cpp
#pragma once

#include <string>

#include "../application/CommissioningSession.h"
#include "../domain/WebFormSubmission.h"

class WebFormCommissioningAdapter
{
public:
    explicit WebFormCommissioningAdapter(CommissioningSession& session);

    std::string submit(const WebFormSubmission& form);
    [[nodiscard]] bool rebootRequested() const;
    [[nodiscard]] WebFormSubmission currentValues() const;

private:
    CommissioningSession& session_;
};
```

- [ ] **Step 8: Write the implementation**

Create `lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp`:

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
    wifiCommand.stringArg2 = form.wifiPassword;
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
        if (form.channelJmriNames[i].empty())
        {
            continue;
        }

        ParsedCommand turnoutCommand;
        turnoutCommand.kind = CommandKind::TurnoutName;
        turnoutCommand.intArg = i + 1;
        turnoutCommand.stringArg1 = form.channelJmriNames[i];
        response = session_.apply(turnoutCommand);
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
    form.wifiPassword = config.wifiPassword;
    form.brokerHost = config.brokerHost;
    form.brokerPort = std::to_string(config.brokerPort);
    form.channelJmriNames = config.channelJmriNames;

    return form;
}
```

**Why `wifi` and `turnout ... name` bypass `CommandLineParser`:** both take string arguments that could contain spaces (a WiFi SSID/password, or a JMRI system name like `"Yard Ladder 3"`), and `CommandLineParser`'s tokenizer has no quoting support — see Global Constraints. Constructing the `ParsedCommand` directly is both correct and simpler than trying to serialize-then-reparse a value that might not survive the round trip.

- [ ] **Step 9: Run the tests to verify they pass**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: PASS — 6 test cases, 0 failures.

- [ ] **Step 10: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites pass (35/35 including the new one — `test_commissioning_session`'s case count grew but it's still one suite).

- [ ] **Step 11: Commit**

```bash
git add lib/McsEsp32/src/application/CommissioningSession.h lib/McsEsp32/src/application/CommissioningSession.cpp test/test_commissioning_session/test_main.cpp lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.h lib/McsEsp32/src/adapters/WebFormCommissioningAdapter.cpp test/test_web_form_commissioning_adapter/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add CommissioningSession::draft() and WebFormCommissioningAdapter
EOF
)"
```

---

### Task 5: `SetupFormRenderer`

**Files:**
- Create: `lib/McsEsp32/src/domain/SetupFormRenderer.h`
- Test: `test/test_setup_form_renderer/test_main.cpp`

**Interfaces:**
- Consumes: `WebFormSubmission` from Task 3, `NodeConfig::kMinNodeId`/`kMaxNodeId`/`kChannelCount` from `NodeConfig.h`.
- Produces (consumed by Task 6):
  ```cpp
  class SetupFormRenderer
  {
  public:
      static std::string escapeHtml(const std::string& text);
      static std::string render(const WebFormSubmission& values);
  };
  ```

- [ ] **Step 1: Write the failing tests**

Create `test/test_setup_form_renderer/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/SetupFormRenderer.h"

namespace
{
    WebFormSubmission emptyValues()
    {
        WebFormSubmission form;
        return form;
    }
}

TEST_CASE("escapeHtml escapes each HTML-significant character")
{
    REQUIRE(SetupFormRenderer::escapeHtml("&") == "&amp;");
    REQUIRE(SetupFormRenderer::escapeHtml("<") == "&lt;");
    REQUIRE(SetupFormRenderer::escapeHtml(">") == "&gt;");
    REQUIRE(SetupFormRenderer::escapeHtml("\"") == "&quot;");
    REQUIRE(SetupFormRenderer::escapeHtml("'") == "&#39;");
}

TEST_CASE("escapeHtml leaves ordinary characters untouched")
{
    REQUIRE(SetupFormRenderer::escapeHtml("MyLayoutWifi123") == "MyLayoutWifi123");
}

TEST_CASE("render embeds the escaped wifi ssid and password values")
{
    WebFormSubmission form = emptyValues();
    form.wifiSsid = "My\"Wifi";
    form.wifiPassword = "pass&word";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("My&quot;Wifi") != std::string::npos);
    REQUIRE(html.find("pass&amp;word") != std::string::npos);
}

TEST_CASE("render embeds the broker host and port values")
{
    WebFormSubmission form = emptyValues();
    form.brokerHost = "192.168.1.50";
    form.brokerPort = "1883";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("192.168.1.50") != std::string::npos);
    REQUIRE(html.find("value='1883'") != std::string::npos);
}

TEST_CASE("render's id dropdown spans the full valid node id range")
{
    const std::string html = SetupFormRenderer::render(emptyValues());

    REQUIRE(html.find(">1</option>") != std::string::npos);
    REQUIRE(html.find(">99</option>") != std::string::npos);
    REQUIRE(html.find(">100</option>") == std::string::npos);
    REQUIRE(html.find(">0</option>") == std::string::npos);
}

TEST_CASE("render's selected node id is marked selected")
{
    WebFormSubmission form = emptyValues();
    form.nodeId = "7";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("value='7' selected") != std::string::npos);
}

TEST_CASE("render includes a labeled input for each of the 12 turnout channels")
{
    WebFormSubmission form = emptyValues();
    form.channelJmriNames[0] = "LT1";
    form.channelJmriNames[11] = "LT12";

    const std::string html = SetupFormRenderer::render(form);

    REQUIRE(html.find("Turnout 1 JMRI Name") != std::string::npos);
    REQUIRE(html.find("name='t1_name'") != std::string::npos);
    REQUIRE(html.find("value='LT1'") != std::string::npos);
    REQUIRE(html.find("Turnout 12 JMRI Name") != std::string::npos);
    REQUIRE(html.find("name='t12_name'") != std::string::npos);
    REQUIRE(html.find("value='LT12'") != std::string::npos);
}
```

- [ ] **Step 2: Run the tests to verify they fail to compile**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: FAIL — `SetupFormRenderer.h` does not exist yet.

- [ ] **Step 3: Write the implementation**

Create `lib/McsEsp32/src/domain/SetupFormRenderer.h`:

```cpp
#pragma once

#include <string>

#include "NodeConfig.h"
#include "WebFormSubmission.h"

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

    static std::string render(const WebFormSubmission& values)
    {
        std::string html;
        html += "<!DOCTYPE html><html><head>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>" + kStyle + "</style></head><body>";
        html += "<div class='card'><h1>MaltBee Panel Setup</h1>";
        html += "<p class='subtitle'>Configure this panel's network settings</p>";
        html += "<form method='POST' action='/submit'>";
        html += "<label>Node ID</label><select name='id'>" + renderIdOptions(values.nodeId) + "</select>";
        html += "<label>WiFi SSID</label><input name='wifi_ssid' value='" + escapeHtml(values.wifiSsid) + "'>";
        html += "<label>WiFi Password</label><input name='wifi_password' type='password' value='"
            + escapeHtml(values.wifiPassword) + "'>";
        html += "<label>Broker Host</label><input name='broker_host' value='" + escapeHtml(values.brokerHost) + "'>";
        html += "<label>Broker Port</label><input name='broker_port' type='number' value='"
            + escapeHtml(values.brokerPort) + "'>";
        html += "<details><summary>Turnout JMRI Names</summary>";
        html += "<p class='warning'>Leave a channel blank to leave it unconfigured.</p>";
        for (int i = 0; i < NodeConfig::kChannelCount; ++i)
        {
            html += renderChannelField(i + 1, values.channelJmriNames[i]);
        }
        html += "</details>";
        html += "<button type='submit'>Save</button>";
        html += "</form></div></body></html>";
        return html;
    }

private:
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

    static std::string renderChannelField(int channelNumber, const std::string& jmriName)
    {
        const std::string fieldName = "t" + std::to_string(channelNumber) + "_name";
        std::string html;
        html += "<label>Turnout " + std::to_string(channelNumber) + " JMRI Name</label>";
        html += "<input name='" + fieldName + "' value='" + escapeHtml(jmriName) + "'>";
        return html;
    }

    static inline const std::string kStyle =
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
        "background:#f2f4f7;margin:0;padding:24px 16px;color:#1f2933;}"
        ".card{max-width:420px;margin:0 auto;background:#fff;border-radius:12px;"
        "box-shadow:0 1px 3px rgba(0,0,0,0.1);padding:24px;}"
        "h1{font-size:1.25rem;margin:0 0 4px;}"
        ".subtitle{color:#6b7280;font-size:0.875rem;margin:0 0 20px;}"
        "label{display:block;font-size:0.8rem;font-weight:600;color:#374151;margin:16px 0 4px;}"
        "input,select{width:100%;box-sizing:border-box;padding:8px 10px;border:1px solid #d1d5db;"
        "border-radius:6px;font-size:0.95rem;}"
        "button{margin-top:24px;width:100%;padding:10px;background:#2563eb;color:#fff;"
        "border:none;border-radius:6px;font-size:1rem;font-weight:600;cursor:pointer;}"
        "button:hover{background:#1d4ed8;}"
        "details{margin-top:20px;}"
        "summary{cursor:pointer;font-size:0.85rem;font-weight:600;color:#374151;}"
        ".warning{color:#b45309;font-size:0.8rem;margin:8px 0 0;}";
};
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: PASS — 7 test cases, 0 failures.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites pass (36/36 including the new one).

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/domain/SetupFormRenderer.h test/test_setup_form_renderer/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add SetupFormRenderer
EOF
)"
```

---

### Task 6: `CaptivePortalServer`

**Files:**
- Create: `lib/McsEsp32/src/adapters/CaptivePortalServer.h`
- Create: `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp`

**Interfaces:**
- Consumes: `WebFormCommissioningAdapter` from Task 4, `SetupFormRenderer` from Task 5, `WebFormSubmission` from Task 3, `NodeConfig::kChannelCount`.
- Produces (consumed by sub-project #2c-b2, not by any task in this plan):
  ```cpp
  class CaptivePortalServer
  {
  public:
      explicit CaptivePortalServer(WebFormCommissioningAdapter& adapter);
      void begin(const std::string& apName);
      void poll();
  };
  ```

No native test — real hardware shim (WiFi AP, DNS, HTTP server), build-check only via `pio run -e esp32dev`, same convention as `NvsConfigStore`/`WiFiLink`/`MqttLink`.

- [ ] **Step 1: Write the header**

Create `lib/McsEsp32/src/adapters/CaptivePortalServer.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include <string>

#include "WebFormCommissioningAdapter.h"
#include "../domain/SetupFormRenderer.h"

class CaptivePortalServer
{
public:
    explicit CaptivePortalServer(WebFormCommissioningAdapter& adapter);

    void begin(const std::string& apName);
    void poll();

private:
    void handleRoot();
    void handleSubmit();
    WebFormSubmission readForm();

    WebFormCommissioningAdapter& adapter_;
    DNSServer dnsServer_;
    WebServer webServer_{80};
};

#endif
```

- [ ] **Step 2: Write the implementation**

Create `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp`:

```cpp
#ifdef ARDUINO

#include "CaptivePortalServer.h"

#include "../domain/NodeConfig.h"

CaptivePortalServer::CaptivePortalServer(WebFormCommissioningAdapter& adapter) : adapter_(adapter)
{
}

void CaptivePortalServer::begin(const std::string& apName)
{
    WiFi.softAP(apName.c_str());

    IPAddress apIp = WiFi.softAPIP();
    dnsServer_.start(53, "*", apIp);

    webServer_.on("/", [this]() { handleRoot(); });
    webServer_.on("/submit", HTTP_POST, [this]() { handleSubmit(); });
    webServer_.onNotFound([this]() { handleRoot(); });
    webServer_.begin();
}

void CaptivePortalServer::poll()
{
    dnsServer_.processNextRequest();
    webServer_.handleClient();
}

void CaptivePortalServer::handleRoot()
{
    webServer_.send(200, "text/html", SetupFormRenderer::render(adapter_.currentValues()).c_str());
}

void CaptivePortalServer::handleSubmit()
{
    const WebFormSubmission form = readForm();
    const std::string response = adapter_.submit(form);
    webServer_.send(200, "text/plain", response.c_str());
}

WebFormSubmission CaptivePortalServer::readForm()
{
    WebFormSubmission form;
    form.nodeId = webServer_.arg("id").c_str();
    form.wifiSsid = webServer_.arg("wifi_ssid").c_str();
    form.wifiPassword = webServer_.arg("wifi_password").c_str();
    form.brokerHost = webServer_.arg("broker_host").c_str();
    form.brokerPort = webServer_.arg("broker_port").c_str();

    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string fieldName = "t" + std::to_string(i + 1) + "_name";
        form.channelJmriNames[i] = webServer_.arg(fieldName.c_str()).c_str();
    }

    return form;
}

#endif
```

- [ ] **Step 3: Build-check the ESP32 target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — no compile errors. If `WebServer`/`DNSServer` aren't found, this environment is missing them from its framework (they ship with `arduino-esp32` core, not a separate `lib_deps` entry) — check `platformio.ini`'s `[env:esp32dev]` framework setting before assuming a missing dependency.

- [ ] **Step 4: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites still pass (36/36, unchanged — this task adds no native test).

- [ ] **Step 5: Verify the Mega target is unaffected**

Run: `pio run -e megaatmega2560`
Expected: `SUCCESS`, unchanged (this task touches nothing under `src/mega/` or `lib/McsLoconet`).

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/adapters/CaptivePortalServer.h lib/McsEsp32/src/adapters/CaptivePortalServer.cpp
git commit -m "$(cat <<'EOF'
^ F Add CaptivePortalServer
EOF
)"
```

---

## Self-review notes

- **Spec coverage:** All eight components from the spec (`MacAddress`, `SetupApName`, `EspDeviceIdentity`, `WebFormSubmission`, `CommissioningSession::draft()`, `WebFormCommissioningAdapter`, `SetupFormRenderer`, `CaptivePortalServer`) are covered across the six tasks. The spec's flagged open question (`CommandLineParser` quoting) was resolved by reading the actual parser: it has zero quote support, so `wifi`/`turnout ... name` construct `ParsedCommand` directly instead of round-tripping through `CommandLineParser::parse()` — documented in Global Constraints and at the point of use in Task 4.
- **Placeholder scan:** No TBD/TODO; every step has complete, concrete code.
- **Type consistency:** `WebFormSubmission`'s field names (`nodeId`, `wifiSsid`, `wifiPassword`, `brokerHost`, `brokerPort`, `channelJmriNames`) are used identically across Tasks 3, 4, 5, and 6. `CaptivePortalServer`'s form field names (`id`, `wifi_ssid`, `wifi_password`, `broker_host`, `broker_port`, `t{N}_name`) match `SetupFormRenderer`'s rendered `name='...'` attributes exactly.
