# Mega Panel Persistent Configuration (Milestone 13) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let an operator set and persist each of the Mega panel's 4 turnout addresses/names over USB serial, surviving power cycles via EEPROM — closing the final item on this project's original roadmap.

**Architecture:** A new `ConfigurationRepository` port (domain: `MegaConfig`; adapters: `InMemoryConfigurationRepository` for tests, `EepromConfigurationRepository` for real hardware) plus a small, hand-parsed `MegaCommissioningSession` application class (3 commands: `address`, `name`, `save` — no generic parser pipeline mirrored from the ESP32 side). Wired into `src/mega/main.cpp`: each `TurnoutStation`'s effective address/name comes from the loaded-or-defaulted config; pins stay compile-time.

**Tech Stack:** C++17 (AVR/`megaatmega2560` target has no real libstdc++ — no STL containers/strings/optionals in `lib/McsCore`, which this milestone's new code lives in and which both the Mega and ESP32 targets compile), PlatformIO, Catch2 (native suite), AVR's built-in `<EEPROM.h>` (no new `lib_deps`).

## Global Constraints

- `ConfigurationRepository::load()` returns a `MegaConfig` value; `save(const MegaConfig&)` returns `bool` — matching the ESP32 side's `ConfigStore` API shape, not the roadmap's original bool-returning sketch.
- Only turnout **addresses and names** become runtime-configurable. GPIO pins stay compile-time, taken from the existing `STATION_CONFIGS` table exactly as today.
- Routes (the 2 Milestone 12 placeholders) are out of scope — not touched by this milestone.
- `MegaConfig::kStationCount = 4`, `kMinAddress = 1`, `kMaxAddress = 2048` — matches the ESP32 side's own `NodeConfig::kMaxTurnoutAddress`. `validate()` returns `true` only if every station's address is in range AND no two stations share an address (no "0 means unconfigured" exception — the Mega always has exactly 4 physically-wired stations).
- EEPROM schema, starting at address 0: 2-byte magic (`0xBEEF`) + 1-byte schema version (`1`) + a single `MegaConfig` struct (4 × 34 bytes = 136 bytes) = 139 bytes total, well inside the ATmega2560's 4096-byte EEPROM. A magic/version mismatch returns an all-zero-address `MegaConfig` (immediately fails `validate()`, triggering the compile-time fallback) rather than interpreting wrong-schema bytes as real data. No CRC/checksum beyond the magic/version check.
- `InMemoryConfigurationRepository` is a real, reusable implementation (not a test double) — used directly by native tests, no separate "Fake" needed for most cases. A separate `test/support/FakeConfigurationRepository.h` (with an injectable `failNextSave` flag, matching this project's established `FakeConfigStore` precedent) is added only for testing `MegaCommissioningSession`'s handling of a failed save — `InMemoryConfigurationRepository` itself can never fail, so it can't exercise that path.
- `EepromConfigurationRepository` is `#ifdef ARDUINO`-guarded, no native test — same precedent as `TurnoutStation`, `MrrwaLocoNetSwitchDriver`, and every other hardware-shim adapter in this project.
- `MegaCommissioningSession::handleLine()` recognizes exactly 3 commands: `address <1-4> <value>`, `name <1-4> <text>`, `save`. **Correction made during planning, not part of the originally-approved design doc:** `show` is NOT one of `handleLine()`'s commands — a 4-station dump cannot fit in the single 32-byte `FixedString32` response every other command returns. `show` is handled entirely in `main.cpp`, which reads `commissioningSession.draft()` (already a public accessor) directly and prints each station itself. This is a module-boundary correction only — the external behavior ("typing `show` prints all 4 stations' current address/name") is unchanged from the design doc's intent.
- Names given to the `name` command are single tokens (no spaces) — a natural consequence of this milestone's "minimal, hand-parsed" scope decision; not a new design question to revisit.
- `src/mega/main.cpp` itself gets no native test — composition-root-only, consistent with every prior `main.cpp`-only change in this project.

---

### Task 1: `FixedString32::c_str()` + `MegaStationConfig`/`MegaConfig` domain

**Files:**
- Modify: `lib/McsCore/src/domain/FixedString32.h`
- Modify: `test/test_fixed_string32/test_main.cpp`
- Create: `lib/McsCore/src/domain/MegaStationConfig.h`
- Create: `lib/McsCore/src/domain/MegaConfig.h`
- Create: `lib/McsCore/src/domain/MegaConfig.cpp`
- Create: `test/test_mega_config/test_main.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `FixedString32::c_str() const` (returns `const char*`) — consumed by Task 5 (`main.cpp`'s `show` printing) and internally by `TurnoutConfig` construction. `MegaStationConfig{int address; FixedString32 name;}`, `MegaConfig{MegaStationConfig stations[4]; bool validate() const;}`, `MegaConfig::kStationCount/kMinAddress/kMaxAddress` — all consumed by Tasks 2-5.

- [ ] **Step 1: Write the failing test for `c_str()`**

Add this test case to the end of `test/test_fixed_string32/test_main.cpp` (after its existing 4 `TEST_CASE`s):

```cpp
TEST_CASE("FixedString32's c_str() exposes the stored value as a null-terminated C string")
{
    const FixedString32 value("Main Yard");

    REQUIRE(strcmp(value.c_str(), "Main Yard") == 0);
}
```

Add `#include <cstring>` to the top of the file, alongside the existing includes, for `strcmp`.

- [ ] **Step 2: Run the test to verify it fails**

Run (via Bash/Git Bash, not PowerShell): `pio test -e native -f test_fixed_string32`
Expected: FAIL / build error — `c_str()` does not exist yet.

- [ ] **Step 3: Add `c_str()` to `FixedString32`**

In `lib/McsCore/src/domain/FixedString32.h`, add this public method (anywhere in the public section — e.g. right after the constructor):

```cpp
    [[nodiscard]] const char* c_str() const
    {
        return buffer_;
    }
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_fixed_string32`
Expected: PASS, all 5 test cases (4 existing + this new one).

- [ ] **Step 5: Write the failing tests for `MegaConfig`**

Create `lib/McsCore/src/domain/MegaStationConfig.h`:

```cpp
#pragma once

#include "FixedString32.h"

struct MegaStationConfig
{
    int address = 0;
    FixedString32 name;
};
```

Create `test/test_mega_config/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/MegaConfig.h"

TEST_CASE("a config with all addresses in range and no duplicates is valid")
{
    MegaConfig config;
    config.stations[0] = {101, FixedString32("Turnout 1")};
    config.stations[1] = {102, FixedString32("Turnout 2")};
    config.stations[2] = {103, FixedString32("Turnout 3")};
    config.stations[3] = {104, FixedString32("Turnout 4")};

    REQUIRE(config.validate());
}

TEST_CASE("an address below the minimum is invalid")
{
    MegaConfig config;
    config.stations[0] = {101, FixedString32("Turnout 1")};
    config.stations[1] = {0, FixedString32("Turnout 2")};
    config.stations[2] = {103, FixedString32("Turnout 3")};
    config.stations[3] = {104, FixedString32("Turnout 4")};

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("an address above the maximum is invalid")
{
    MegaConfig config;
    config.stations[0] = {101, FixedString32("Turnout 1")};
    config.stations[1] = {102, FixedString32("Turnout 2")};
    config.stations[2] = {103, FixedString32("Turnout 3")};
    config.stations[3] = {2049, FixedString32("Turnout 4")};

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("the minimum and maximum address bounds are themselves valid")
{
    MegaConfig config;
    config.stations[0] = {1, FixedString32("Turnout 1")};
    config.stations[1] = {2048, FixedString32("Turnout 2")};
    config.stations[2] = {103, FixedString32("Turnout 3")};
    config.stations[3] = {104, FixedString32("Turnout 4")};

    REQUIRE(config.validate());
}

TEST_CASE("two stations sharing the same address is invalid")
{
    MegaConfig config;
    config.stations[0] = {101, FixedString32("Turnout 1")};
    config.stations[1] = {101, FixedString32("Turnout 2")};
    config.stations[2] = {103, FixedString32("Turnout 3")};
    config.stations[3] = {104, FixedString32("Turnout 4")};

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("a default-constructed config (all-zero addresses) is invalid")
{
    const MegaConfig config;

    REQUIRE_FALSE(config.validate());
}
```

- [ ] **Step 6: Run the tests to verify they fail**

Run: `pio test -e native -f test_mega_config`
Expected: FAIL / build error — `MegaConfig` does not exist yet.

- [ ] **Step 7: Create `MegaConfig`**

Create `lib/McsCore/src/domain/MegaConfig.h`:

```cpp
#pragma once

#include "MegaStationConfig.h"

struct MegaConfig
{
    static constexpr int kStationCount = 4;
    static constexpr int kMinAddress = 1;
    static constexpr int kMaxAddress = 2048;

    MegaStationConfig stations[kStationCount];

    [[nodiscard]] bool validate() const;
};
```

Create `lib/McsCore/src/domain/MegaConfig.cpp`:

```cpp
#include "MegaConfig.h"

bool MegaConfig::validate() const
{
    for (int i = 0; i < kStationCount; ++i)
    {
        const int address = stations[i].address;
        if (address < kMinAddress || address > kMaxAddress)
        {
            return false;
        }

        for (int j = i + 1; j < kStationCount; ++j)
        {
            if (stations[j].address == address)
            {
                return false;
            }
        }
    }

    return true;
}
```

- [ ] **Step 8: Run the tests to verify they pass**

Run: `pio test -e native -f test_mega_config`
Expected: PASS, all 6 test cases.

- [ ] **Step 9: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: 59 suites passing (57 existing + `test_mega_config`, which is new — `test_fixed_string32` already existed and just gained one more case, not a new suite), zero failures.

- [ ] **Step 10: Commit**

```bash
git add lib/McsCore/src/domain/FixedString32.h test/test_fixed_string32/test_main.cpp lib/McsCore/src/domain/MegaStationConfig.h lib/McsCore/src/domain/MegaConfig.h lib/McsCore/src/domain/MegaConfig.cpp test/test_mega_config/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add MegaConfig domain and FixedString32::c_str()

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: `ConfigurationRepository` port + `InMemoryConfigurationRepository` + `FakeConfigurationRepository`

**Files:**
- Create: `lib/McsCore/src/ports/ConfigurationRepository.h`
- Create: `lib/McsCore/src/adapters/InMemoryConfigurationRepository.h`
- Create: `test/support/FakeConfigurationRepository.h`
- Create: `test/test_in_memory_configuration_repository/test_main.cpp`

**Interfaces:**
- Consumes: `MegaConfig` (Task 1).
- Produces: `ConfigurationRepository` (pure virtual, `load()`/`save()`), `InMemoryConfigurationRepository` (real implementation), `FakeConfigurationRepository` (test double with `failNextSave`) — all consumed by Task 3. `EepromConfigurationRepository` (Task 4) also implements this same port.

- [ ] **Step 1: Write the failing tests**

Create `test/test_in_memory_configuration_repository/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/InMemoryConfigurationRepository.h"

TEST_CASE("load before any save returns a default-constructed, invalid config")
{
    InMemoryConfigurationRepository repository;

    const MegaConfig config = repository.load();

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("save then load round-trips the exact config")
{
    InMemoryConfigurationRepository repository;
    MegaConfig original;
    original.stations[0] = {101, FixedString32("Turnout 1")};
    original.stations[1] = {102, FixedString32("Turnout 2")};
    original.stations[2] = {103, FixedString32("Turnout 3")};
    original.stations[3] = {104, FixedString32("Turnout 4")};

    REQUIRE(repository.save(original));
    const MegaConfig loaded = repository.load();

    REQUIRE(loaded.stations[0].address == 101);
    REQUIRE(strcmp(loaded.stations[0].name.c_str(), "Turnout 1") == 0);
    REQUIRE(loaded.stations[3].address == 104);
    REQUIRE(strcmp(loaded.stations[3].name.c_str(), "Turnout 4") == 0);
}

TEST_CASE("a second save overwrites the first")
{
    InMemoryConfigurationRepository repository;
    MegaConfig first;
    first.stations[0] = {101, FixedString32("First")};
    MegaConfig second;
    second.stations[0] = {201, FixedString32("Second")};

    repository.save(first);
    repository.save(second);
    const MegaConfig loaded = repository.load();

    REQUIRE(loaded.stations[0].address == 201);
}
```

`FixedString32`/`MegaConfig` are brought in transitively via `InMemoryConfigurationRepository.h` → `ConfigurationRepository.h` → `MegaConfig.h`; add `#include <cstring>` for `strcmp`.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `pio test -e native -f test_in_memory_configuration_repository`
Expected: FAIL / build error — the classes don't exist yet.

- [ ] **Step 3: Create the port**

Create `lib/McsCore/src/ports/ConfigurationRepository.h`:

```cpp
#pragma once

#include "../domain/MegaConfig.h"

class ConfigurationRepository
{
public:
    virtual ~ConfigurationRepository() = default;

    virtual MegaConfig load() = 0;
    virtual bool save(const MegaConfig& config) = 0;
};
```

- [ ] **Step 4: Create `InMemoryConfigurationRepository`**

Create `lib/McsCore/src/adapters/InMemoryConfigurationRepository.h`:

```cpp
#pragma once

#include "../ports/ConfigurationRepository.h"

// A real, reusable ConfigurationRepository backed by a plain in-memory
// MegaConfig - not merely a test double. Useful for native tests (no
// EEPROM dependency) and anywhere a caller wants configuration without
// persistence.
class InMemoryConfigurationRepository final : public ConfigurationRepository
{
public:
    MegaConfig load() override
    {
        return stored_;
    }

    bool save(const MegaConfig& config) override
    {
        stored_ = config;
        return true;
    }

private:
    MegaConfig stored_;
};
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `pio test -e native -f test_in_memory_configuration_repository`
Expected: PASS, all 3 test cases.

- [ ] **Step 6: Create the `FakeConfigurationRepository` test double**

Create `test/support/FakeConfigurationRepository.h`:

```cpp
#pragma once

#include "ports/ConfigurationRepository.h"

class FakeConfigurationRepository final : public ConfigurationRepository
{
public:
    bool failNextSave = false;

    MegaConfig load() override
    {
        return stored_;
    }

    bool save(const MegaConfig& config) override
    {
        if (failNextSave)
        {
            failNextSave = false;
            return false;
        }
        stored_ = config;
        return true;
    }

private:
    MegaConfig stored_;
};
```

(No test file for this one — it's a test double, exercised indirectly by Task 3's tests, matching this project's established convention of not writing standalone tests for `test/support/Fake*` classes.)

- [ ] **Step 7: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: 60 suites passing (59 from Task 1 + `test_in_memory_configuration_repository`), zero failures.

- [ ] **Step 8: Commit**

```bash
git add lib/McsCore/src/ports/ConfigurationRepository.h lib/McsCore/src/adapters/InMemoryConfigurationRepository.h test/support/FakeConfigurationRepository.h test/test_in_memory_configuration_repository/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add ConfigurationRepository port and in-memory implementations

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: `MegaCommissioningSession`

**Files:**
- Create: `lib/McsCore/src/application/MegaCommissioningSession.h`
- Create: `lib/McsCore/src/application/MegaCommissioningSession.cpp`
- Create: `test/test_mega_commissioning_session/test_main.cpp`

**Interfaces:**
- Consumes: `ConfigurationRepository` (Task 2), `InMemoryConfigurationRepository`/`FakeConfigurationRepository` (Task 2, for its own tests), `MegaConfig`/`MegaStationConfig`/`FixedString32` (Task 1).
- Produces: `MegaCommissioningSession(ConfigurationRepository& repository, const MegaConfig& defaults)`, `FixedString32 handleLine(const char* line)`, `const MegaConfig& draft() const` — consumed by Task 5.

- [ ] **Step 1: Write the failing tests**

Create `test/test_mega_commissioning_session/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/MegaCommissioningSession.h"
#include "support/FakeConfigurationRepository.h"
#include "adapters/InMemoryConfigurationRepository.h"

namespace
{
    MegaConfig makeDefaults()
    {
        MegaConfig config;
        config.stations[0] = {101, FixedString32("Turnout 1")};
        config.stations[1] = {102, FixedString32("Turnout 2")};
        config.stations[2] = {103, FixedString32("Turnout 3")};
        config.stations[3] = {104, FixedString32("Turnout 4")};
        return config;
    }
}

TEST_CASE("a fresh session with nothing saved falls back to the compile-time defaults")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    REQUIRE(session.draft().stations[0].address == 101);
    REQUIRE(strcmp(session.draft().stations[0].name.c_str(), "Turnout 1") == 0);
}

TEST_CASE("a fresh session with a valid saved config uses it instead of the defaults")
{
    InMemoryConfigurationRepository repository;
    MegaConfig saved = makeDefaults();
    saved.stations[0].address = 999;
    repository.save(saved);

    MegaCommissioningSession session(repository, makeDefaults());

    REQUIRE(session.draft().stations[0].address == 999);
}

TEST_CASE("address command stages a valid address without touching the repository")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("address 1 500");

    REQUIRE(strcmp(response.c_str(), "OK") == 0);
    REQUIRE(session.draft().stations[0].address == 500);
    REQUIRE_FALSE(repository.load().validate());
}

TEST_CASE("address command rejects an out-of-range station number")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("address 5 500");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
    REQUIRE(session.draft().stations[0].address == 101);
}

TEST_CASE("address command rejects an out-of-range address value")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("address 1 9999");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
    REQUIRE(session.draft().stations[0].address == 101);
}

TEST_CASE("name command stages a new name")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("name 2 MainYard");

    REQUIRE(strcmp(response.c_str(), "OK") == 0);
    REQUIRE(strcmp(session.draft().stations[1].name.c_str(), "MainYard") == 0);
}

TEST_CASE("name command rejects an out-of-range station number")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("name 0 MainYard");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
    REQUIRE(strcmp(session.draft().stations[0].name.c_str(), "Turnout 1") == 0);
}

TEST_CASE("save command persists the draft to the repository")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());
    session.handleLine("address 1 500");

    const FixedString32 response = session.handleLine("save");

    REQUIRE(strcmp(response.c_str(), "OK") == 0);
    REQUIRE(repository.load().stations[0].address == 500);
}

TEST_CASE("save command reports an error when the repository fails to save")
{
    FakeConfigurationRepository repository;
    repository.failNextSave = true;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("save");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
}

TEST_CASE("an unrecognized command is rejected and leaves the draft unchanged")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("frobnicate 1 2");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
    REQUIRE(session.draft().stations[0].address == 101);
}

TEST_CASE("a malformed address command (non-numeric value) is rejected and leaves the draft unchanged")
{
    InMemoryConfigurationRepository repository;
    MegaCommissioningSession session(repository, makeDefaults());

    const FixedString32 response = session.handleLine("address 1 abc");

    REQUIRE(strcmp(response.c_str(), "OK") != 0);
    REQUIRE(session.draft().stations[0].address == 101);
}
```

Add `#include <cstring>` at the top for `strcmp`.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `pio test -e native -f test_mega_commissioning_session`
Expected: FAIL / build error — `MegaCommissioningSession` does not exist yet.

- [ ] **Step 3: Create the header**

Create `lib/McsCore/src/application/MegaCommissioningSession.h`:

```cpp
#pragma once

#include "../domain/MegaConfig.h"
#include "../ports/ConfigurationRepository.h"

// Minimal, hand-parsed serial commissioning for the Mega panel's 4 turnout
// addresses/names. Deliberately smaller than the ESP32 side's
// CommandLineParser/ParsedCommand/CommissioningSession layering - this
// command set (address/name/save) doesn't need a generic parser pipeline.
// Edits apply to an in-memory draft only; the repository is touched on
// "save" alone, to avoid EEPROM wear from every keystroke.
class MegaCommissioningSession
{
public:
    MegaCommissioningSession(ConfigurationRepository& repository, const MegaConfig& defaults);

    [[nodiscard]] FixedString32 handleLine(const char* line);
    [[nodiscard]] const MegaConfig& draft() const;

private:
    ConfigurationRepository& repository_;
    MegaConfig draft_;
};
```

- [ ] **Step 4: Create the implementation**

Create `lib/McsCore/src/application/MegaCommissioningSession.cpp`:

```cpp
#include "MegaCommissioningSession.h"

#include <cstdio>

MegaCommissioningSession::MegaCommissioningSession(ConfigurationRepository& repository, const MegaConfig& defaults)
    : repository_(repository), draft_(repository_.load())
{
    if (!draft_.validate())
    {
        draft_ = defaults;
    }
}

FixedString32 MegaCommissioningSession::handleLine(const char* line)
{
    int station = 0;
    int address = 0;

    if (sscanf(line, "address %d %d", &station, &address) == 2)
    {
        if (station < 1 || station > MegaConfig::kStationCount)
        {
            return FixedString32("ERR: station must be 1-4");
        }
        if (address < MegaConfig::kMinAddress || address > MegaConfig::kMaxAddress)
        {
            return FixedString32("ERR: address out of range");
        }
        draft_.stations[station - 1].address = address;
        return FixedString32("OK");
    }

    char nameBuffer[FixedString32::CAPACITY] = {};
    if (sscanf(line, "name %d %31s", &station, nameBuffer) == 2)
    {
        if (station < 1 || station > MegaConfig::kStationCount)
        {
            return FixedString32("ERR: station must be 1-4");
        }
        draft_.stations[station - 1].name = FixedString32(nameBuffer);
        return FixedString32("OK");
    }

    if (strcmp(line, "save") == 0)
    {
        return repository_.save(draft_) ? FixedString32("OK") : FixedString32("ERR: save failed");
    }

    return FixedString32("ERR: unknown command");
}

const MegaConfig& MegaCommissioningSession::draft() const
{
    return draft_;
}
```

Add `#include <cstring>` for `strcmp` (needed alongside `<cstdio>` for `sscanf`).

- [ ] **Step 5: Run the tests to verify they pass**

Run: `pio test -e native -f test_mega_commissioning_session`
Expected: PASS, all 11 test cases.

- [ ] **Step 6: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: 61 suites passing (60 from Task 2 + this new one), zero failures.

- [ ] **Step 7: Commit**

```bash
git add lib/McsCore/src/application/MegaCommissioningSession.h lib/McsCore/src/application/MegaCommissioningSession.cpp test/test_mega_commissioning_session/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add MegaCommissioningSession for serial config commands

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: `EepromConfigurationRepository`

**Files:**
- Create: `lib/McsCore/src/adapters/EepromConfigurationRepository.h`
- Create: `lib/McsCore/src/adapters/EepromConfigurationRepository.cpp`

**Interfaces:**
- Consumes: `ConfigurationRepository` (Task 2), `MegaConfig` (Task 1), AVR's `<EEPROM.h>` (`EEPROM.put<T>(address, const T&)`/`EEPROM.get<T>(address, T&)` — both handle multi-byte reads/writes of a trivially-copyable type directly; `MegaConfig`/`MegaStationConfig`/`FixedString32` are all trivially copyable — no virtual functions, no pointers, no user-defined destructor/copy/move — so writing/reading the whole `MegaConfig` struct in one call is safe).
- Produces: `EepromConfigurationRepository` — consumed by Task 5. No native test (`#ifdef ARDUINO`-guarded); verified by the `megaatmega2560` build in Task 5's own build step, since this class has no standalone build target of its own on `native`.

- [ ] **Step 1: Create the header**

Create `lib/McsCore/src/adapters/EepromConfigurationRepository.h`:

```cpp
#pragma once

#include "../ports/ConfigurationRepository.h"

// #ifdef ARDUINO-guarded hardware adapter, same precedent as TurnoutStation
// and MrrwaLocoNetSwitchDriver - uses the AVR EEPROM library directly, no
// native test possible or needed.
class EepromConfigurationRepository final : public ConfigurationRepository
{
public:
    MegaConfig load() override;
    bool save(const MegaConfig& config) override;
};
```

- [ ] **Step 2: Create the implementation**

Create `lib/McsCore/src/adapters/EepromConfigurationRepository.cpp`:

```cpp
#ifdef ARDUINO

#include "EepromConfigurationRepository.h"

#include <EEPROM.h>

namespace
{
    constexpr int EEPROM_MAGIC_ADDRESS = 0;
    constexpr int EEPROM_VERSION_ADDRESS = EEPROM_MAGIC_ADDRESS + sizeof(uint16_t);
    constexpr int EEPROM_CONFIG_ADDRESS = EEPROM_VERSION_ADDRESS + sizeof(uint8_t);

    constexpr uint16_t SCHEMA_MAGIC = 0xBEEF;
    constexpr uint8_t SCHEMA_VERSION = 1;
}

MegaConfig EepromConfigurationRepository::load()
{
    uint16_t magic = 0;
    uint8_t version = 0;
    EEPROM.get(EEPROM_MAGIC_ADDRESS, magic);
    EEPROM.get(EEPROM_VERSION_ADDRESS, version);

    MegaConfig config{};
    if (magic != SCHEMA_MAGIC || version != SCHEMA_VERSION)
    {
        return config;
    }

    EEPROM.get(EEPROM_CONFIG_ADDRESS, config);
    return config;
}

bool EepromConfigurationRepository::save(const MegaConfig& config)
{
    EEPROM.put(EEPROM_MAGIC_ADDRESS, SCHEMA_MAGIC);
    EEPROM.put(EEPROM_VERSION_ADDRESS, SCHEMA_VERSION);
    EEPROM.put(EEPROM_CONFIG_ADDRESS, config);
    return true;
}

#endif
```

(`EEPROM.put()` only rewrites bytes that actually changed, a built-in feature of the Arduino EEPROM library — reduces wear automatically, no extra logic needed here.)

- [ ] **Step 3: Build the Mega environment to confirm this compiles**

```bash
pio run -e megaatmega2560
```

Expected: builds clean (`SUCCESS`) — this class isn't wired into `main.cpp` yet (that's Task 5), but it must compile standalone as part of the `McsCore` library build. If it fails to compile at this stage, fix it here rather than letting Task 5 discover the problem.

- [ ] **Step 4: Run the native suite to confirm no regressions**

```bash
pio test -e native
```

Expected: unaffected (61 suites, same as after Task 3) — this file is `#ifdef ARDUINO`-guarded and contributes nothing to the `native` build.

**Do NOT launch this command with a backgrounding mechanism and then wait across turns for it.** Run it and wait for it to finish in the same turn.

- [ ] **Step 5: Commit**

```bash
git add lib/McsCore/src/adapters/EepromConfigurationRepository.h lib/McsCore/src/adapters/EepromConfigurationRepository.cpp
git commit -m "$(cat <<'EOF'
@ F Add EepromConfigurationRepository

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(`@` risk symbol: hardware-only class with no native test, same classification as `TurnoutStation`'s own commits.)

---

### Task 5: Wire persistent config into `src/mega/main.cpp`

**Files:**
- Modify: `src/mega/main.cpp`

**Interfaces:**
- Consumes: `EepromConfigurationRepository` (Task 4), `MegaCommissioningSession` (Task 3), `MegaConfig`/`MegaStationConfig`/`FixedString32::c_str()` (Task 1).
- Produces: nothing consumed by a later task — Task 6 only documents this task's result.

The file currently reads in full (verified immediately before writing this plan, last touched by commit `9f45f34`, no intervening commits):

```cpp
#include <Arduino.h>
#include <LocoNet.h>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/LocoNetFeedbackDecoder.h"
#include "adapters/MrrwaLocoNetFeedbackSource.h"
#include "adapters/MrrwaLocoNetSwitchDriver.h"
#include "adapters/MrrwaLocoNetTurnoutAdapter.h"
#include "adapters/PulsingLocoNetTransport.h"
#include "adapters/TurnoutStation.h"
#include "application/RouteExecutionService.h"
#include "domain/Button.h"
#include "domain/Route.h"

namespace
{
    constexpr int STATION_COUNT = 4;
    constexpr TurnoutConfig STATION_CONFIGS[STATION_COUNT] = {
        {101, "Turnout 1", 22, 23, 8, 9},
        {102, "Turnout 2", 24, 25, 10, 11},
        {103, "Turnout 3", 26, 27, 12, 13},
        {104, "Turnout 4", 28, 29, 14, 15},
    };
    constexpr unsigned long LOCONET_PULSE_DURATION_MS = 250;
    constexpr unsigned long ROUTE_COMMAND_SPACING_MS = 300;
    constexpr unsigned long ROUTE_BUTTON_DEBOUNCE_MS = 30;
}

static_assert(ROUTE_COMMAND_SPACING_MS > LOCONET_PULSE_DURATION_MS,
              "route spacing must exceed the LocoNet pulse duration, or route commands will truncate turnout pulses");

ArduinoClock clock;

MrrwaLocoNetSwitchDriver locoNetSwitchDriver;
PulsingLocoNetTransport locoNetTransport(locoNetSwitchDriver, clock, LOCONET_PULSE_DURATION_MS);
MrrwaLocoNetTurnoutAdapter turnoutCommandPort(locoNetTransport);

MrrwaLocoNetFeedbackSource locoNetFeedbackSource;
LocoNetFeedbackDecoder locoNetFeedbackDecoder;

TurnoutStation stations[STATION_COUNT] = {
    TurnoutStation(STATION_CONFIGS[0], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[1], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[2], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[3], clock, turnoutCommandPort),
};

ArduinoDigitalInput route1ButtonInput(30, /*activeLow=*/true, /*useInternalPullup=*/true);
ArduinoDigitalInput route2ButtonInput(31, /*activeLow=*/true, /*useInternalPullup=*/true);
Button route1Button(route1ButtonInput, clock, ROUTE_BUTTON_DEBOUNCE_MS);
Button route2Button(route2ButtonInput, clock, ROUTE_BUTTON_DEBOUNCE_MS);

// Placeholder routes - replace with real route definitions for your layout.
// Populated in setup() via addTurnout() calls below.
Route route1(1, "Example Route 1");
Route route2(2, "Example Route 2");

RouteExecutionService routeExecutionService(turnoutCommandPort, clock, ROUTE_COMMAND_SPACING_MS);

void setup()
{
    route1.addTurnout(101, TurnoutPosition::Thrown);
    route1.addTurnout(102, TurnoutPosition::Closed);
    route2.addTurnout(103, TurnoutPosition::Closed);
    route2.addTurnout(104, TurnoutPosition::Closed);

    route1ButtonInput.begin();
    route2ButtonInput.begin();

    for (auto& station : stations)
    {
        station.begin();
    }

    LocoNet.init();
}

void loop()
{
    for (auto& station : stations)
    {
        station.update();
    }

    locoNetTransport.update();

    SwitchOutputsReport report;
    if (locoNetFeedbackSource.poll(report))
    {
        const TurnoutFeedbackLookup lookup = locoNetFeedbackDecoder.decode(report);
        if (lookup.found)
        {
            for (auto& station : stations)
            {
                station.applyFeedback(lookup.feedback);
            }
        }
    }

    route1Button.update();
    route2Button.update();

    if (route1Button.wasPressed())
    {
        routeExecutionService.start(route1);
    }
    if (route2Button.wasPressed())
    {
        routeExecutionService.start(route2);
    }
    routeExecutionService.update();
}
```

- [ ] **Step 1: Add the new includes**

```cpp
#include <Arduino.h>
#include <LocoNet.h>

#include <cstring>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/EepromConfigurationRepository.h"
#include "adapters/LocoNetFeedbackDecoder.h"
#include "adapters/MrrwaLocoNetFeedbackSource.h"
#include "adapters/MrrwaLocoNetSwitchDriver.h"
#include "adapters/MrrwaLocoNetTurnoutAdapter.h"
#include "adapters/PulsingLocoNetTransport.h"
#include "adapters/TurnoutStation.h"
#include "application/MegaCommissioningSession.h"
#include "application/RouteExecutionService.h"
#include "domain/Button.h"
#include "domain/Route.h"
```

(`<cstring>` is for `strcmp`, used by the new `show`-detection in `loop()`. `domain/MegaConfig.h` doesn't need its own include — it's brought in transitively via `application/MegaCommissioningSession.h`, matching how `TurnoutPosition` is already available transitively today via `adapters/TurnoutStation.h` → `domain/Turnout.h`.)

- [ ] **Step 2: Add the new constant**

```cpp
namespace
{
    constexpr int STATION_COUNT = 4;
    constexpr TurnoutConfig STATION_CONFIGS[STATION_COUNT] = {
        {101, "Turnout 1", 22, 23, 8, 9},
        {102, "Turnout 2", 24, 25, 10, 11},
        {103, "Turnout 3", 26, 27, 12, 13},
        {104, "Turnout 4", 28, 29, 14, 15},
    };
    constexpr unsigned long LOCONET_PULSE_DURATION_MS = 250;
    constexpr unsigned long ROUTE_COMMAND_SPACING_MS = 300;
    constexpr unsigned long ROUTE_BUTTON_DEBOUNCE_MS = 30;
    constexpr unsigned long UART_BAUD_RATE = 115200;
    constexpr int SERIAL_LINE_BUFFER_SIZE = 64;
}
```

- [ ] **Step 3: Add the config-loading globals and a helper function, before `stations[]`**

Replace:

```cpp
MrrwaLocoNetFeedbackSource locoNetFeedbackSource;
LocoNetFeedbackDecoder locoNetFeedbackDecoder;

TurnoutStation stations[STATION_COUNT] = {
    TurnoutStation(STATION_CONFIGS[0], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[1], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[2], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[3], clock, turnoutCommandPort),
};
```

with:

```cpp
MrrwaLocoNetFeedbackSource locoNetFeedbackSource;
LocoNetFeedbackDecoder locoNetFeedbackDecoder;

namespace
{
    MegaConfig buildDefaultConfig()
    {
        MegaConfig config;
        for (int i = 0; i < MegaConfig::kStationCount; ++i)
        {
            config.stations[i].address = STATION_CONFIGS[i].address;
            config.stations[i].name = FixedString32(STATION_CONFIGS[i].name);
        }
        return config;
    }
}

EepromConfigurationRepository configRepository;
const MegaConfig DEFAULT_CONFIG = buildDefaultConfig();
MegaCommissioningSession commissioningSession(configRepository, DEFAULT_CONFIG);

namespace
{
    TurnoutConfig buildEffectiveStationConfig(const int index)
    {
        const MegaStationConfig& stationConfig = commissioningSession.draft().stations[index];
        return TurnoutConfig{
            stationConfig.address,
            stationConfig.name.c_str(),
            STATION_CONFIGS[index].throwButtonPin,
            STATION_CONFIGS[index].closeButtonPin,
            STATION_CONFIGS[index].thrownLedPin,
            STATION_CONFIGS[index].closedLedPin,
        };
    }
}

TurnoutStation stations[STATION_COUNT] = {
    TurnoutStation(buildEffectiveStationConfig(0), clock, turnoutCommandPort),
    TurnoutStation(buildEffectiveStationConfig(1), clock, turnoutCommandPort),
    TurnoutStation(buildEffectiveStationConfig(2), clock, turnoutCommandPort),
    TurnoutStation(buildEffectiveStationConfig(3), clock, turnoutCommandPort),
};
```

(`configRepository`/`DEFAULT_CONFIG`/`commissioningSession` must be declared before `stations[]`, since `buildEffectiveStationConfig()` reads `commissioningSession.draft()` — C++ guarantees globals in one translation unit initialize top-to-bottom, so this ordering is what makes it safe. `buildEffectiveStationConfig()`'s returned `TurnoutConfig::name` pointer comes from `commissioningSession`'s own long-lived storage, valid for the brief `TurnoutStation` constructor call that copies the characters out — `commissioningSession` itself outlives the entire program, so this is safe.)

- [ ] **Step 4: Add the Serial line-buffer globals**

Immediately after the existing `RouteExecutionService routeExecutionService(...)` line:

```cpp
RouteExecutionService routeExecutionService(turnoutCommandPort, clock, ROUTE_COMMAND_SPACING_MS);

char serialLineBuffer[SERIAL_LINE_BUFFER_SIZE] = {};
int serialLineLength = 0;
```

- [ ] **Step 5: Begin Serial in `setup()`**

```cpp
void setup()
{
    Serial.begin(UART_BAUD_RATE);

    route1.addTurnout(101, TurnoutPosition::Thrown);
    route1.addTurnout(102, TurnoutPosition::Closed);
    route2.addTurnout(103, TurnoutPosition::Closed);
    route2.addTurnout(104, TurnoutPosition::Closed);

    route1ButtonInput.begin();
    route2ButtonInput.begin();

    for (auto& station : stations)
    {
        station.begin();
    }

    LocoNet.init();
}
```

- [ ] **Step 6: Process Serial input in `loop()`**

Add this block as the very last statement in `loop()`, after the existing `routeExecutionService.update();` line:

```cpp
    routeExecutionService.update();

    while (Serial.available())
    {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r')
        {
            if (serialLineLength > 0)
            {
                serialLineBuffer[serialLineLength] = '\0';
                if (strcmp(serialLineBuffer, "show") == 0)
                {
                    for (int i = 0; i < MegaConfig::kStationCount; ++i)
                    {
                        const MegaStationConfig& station = commissioningSession.draft().stations[i];
                        Serial.print("Station ");
                        Serial.print(i + 1);
                        Serial.print(": address=");
                        Serial.print(station.address);
                        Serial.print(" name=");
                        Serial.println(station.name.c_str());
                    }
                }
                else
                {
                    const FixedString32 response = commissioningSession.handleLine(serialLineBuffer);
                    Serial.println(response.c_str());
                }
                serialLineLength = 0;
            }
        }
        else if (serialLineLength < SERIAL_LINE_BUFFER_SIZE - 1)
        {
            serialLineBuffer[serialLineLength++] = c;
        }
    }
}
```

(Handles both `\n` and `\r` line endings; the `serialLineLength > 0` guard means a `\r\n` pair only processes the line once, not twice.)

- [ ] **Step 7: Build the Mega environment**

```bash
pio run -e megaatmega2560
```

Expected: builds clean (`SUCCESS`). Note the reported RAM/Flash percentages — Task 6 needs the exact post-change figures.

- [ ] **Step 8: Build the other 2 hardware environments as a regression check**

```bash
pio run -e esp32node4
pio run -e esp32dev
```

Expected: both `SUCCESS`, byte-for-byte unaffected — this task touches no shared `lib/` code outside `McsCore`, which neither ESP32 environment's composition root references for this new functionality.

- [ ] **Step 9: Run the native test suite**

```bash
pio test -e native
```

Expected: unaffected by this task specifically (61 suites passing, same as after Task 4 — `main.cpp` has no native test target).

**Do NOT launch this command with a backgrounding mechanism and then wait across turns for it.** Run it and wait for it to finish in the same turn.

- [ ] **Step 10: Commit**

```bash
git add src/mega/main.cpp
git commit -m "$(cat <<'EOF'
@ F Wire persistent config into Mega panel main.cpp

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Document Milestone 13 and run the staleness sweep

**Files:**
- Modify: `CLAUDE.md`
- Modify: `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`
- Modify: `README.md`
- Modify: `docs/HARDWARE_BRINGUP_CHECKLIST.md`

**Interfaces:**
- Consumes: Task 5's completed wiring and its reported RAM/Flash build figures.
- Produces: nothing — this is the last task in this plan.

- [ ] **Step 1: Update the roadmap doc's Milestone 13 section**

In `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`, find the current `## Milestone 13: Add Persistent Configuration` section. Replace it with:

```markdown
## ✅ Milestone 13: Add Persistent Configuration
**Status:** COMPLETE

### Tasks

- [x] Define configuration data structures — `MegaStationConfig`/`MegaConfig`
  (`lib/McsCore/src/domain/`), covering address and name per station. Pin
  assignments stay compile-time (never persisted), unlike this section's
  original "pin assignments" language — see the completion-criteria note
  below for why.
- [x] Add `ConfigurationRepository` — built with the ESP32 side's
  `ConfigStore` API shape (`load()` returns a config value, `save(config)`
  takes one) rather than this section's own original bool-returning
  sketch (§5.7), for consistency with the only other persistence port in
  this project.
- [x] Create an in-memory repository first — `InMemoryConfigurationRepository`,
  a real, reusable implementation (not merely a test double), matching
  this project's §8 "allows configuration tests without EEPROM" intent
  exactly.
- [x] Add EEPROM persistence — `EepromConfigurationRepository`, a 139-byte
  schema (2-byte magic + 1-byte version + the `MegaConfig` struct) against
  the ATmega2560's 4096-byte EEPROM.
- [x] Add schema versioning — the 1-byte schema version in the EEPROM
  header; a mismatch (or magic mismatch) is treated as "no valid config,"
  triggering the compile-time fallback.
- [x] Add safe defaults when configuration is invalid — `MegaCommissioningSession`
  falls back to the compile-time `STATION_CONFIGS` defaults whenever the
  loaded config fails `validate()` (out-of-range or duplicate addresses,
  or nothing ever saved).

### Completion Criteria

- [x] Turnout addresses and pin assignments can be loaded from storage —
  **narrowed with Paige during brainstorming**: only addresses (and
  names) are persisted; pin assignments stay compile-time, matching the
  ESP32 side's own `NodeConfig` precedent (which never persists GPIO
  pins either) and avoiding a stored config that could point at hardware
  that doesn't exist.
- [x] Invalid configuration does not leave the system in an unsafe state —
  an invalid or absent EEPROM config falls back to the known-good
  compile-time defaults rather than booting with garbage turnout
  addresses.
```

- [ ] **Step 2: Update CLAUDE.md's milestone summary lines**

Find the current lines:

```
**Completed milestones:** 1-12 (foundation, ports, Button, Indicator, Turnout domain model, TurnoutIndicator, TurnoutControl, hardware integration programming, LocoNet output, LocoNet feedback, multiple turnouts, routes). `pio run -e megaatmega2560` compiles successfully (RAM 18.1%, Flash 3.1% with 4 stations + routes). Milestones 9-12 are complete on the programming side only — physical wiring and on-hardware verification (button-to-LED and LocoNet send/receive against a real DR5000/DR4018, across all 4 stations, plus the 2 new route buttons) are still outstanding — see the "Milestone 8 hardware" note below.

**Next milestones:** persistent configuration (13)
```

Replace with (filling in the exact RAM/Flash percentages from Task 5's `pio run -e megaatmega2560` report — do not guess; use the real reported figures):

```
**Completed milestones:** 1-13 (foundation, ports, Button, Indicator, Turnout domain model, TurnoutIndicator, TurnoutControl, hardware integration programming, LocoNet output, LocoNet feedback, multiple turnouts, routes, persistent configuration). `pio run -e megaatmega2560` compiles successfully (RAM <TASK5_RAM_PCT>, Flash <TASK5_FLASH_PCT> with 4 stations + routes + persistent config). Milestones 9-13 are complete on the programming side only — physical wiring and on-hardware verification (button-to-LED and LocoNet send/receive against a real DR5000/DR4018, across all 4 stations, plus the 2 route buttons, plus serial commissioning) are still outstanding — see the "Milestone 8 hardware" note below.

**Next milestones:** none — this was the last item on the Mega panel's original roadmap. Milestone 14 (Production Hardening) exists as a roadmap section but was never part of this project's active milestone sequence tracked here; pick it up as its own future effort if wanted.
```

- [ ] **Step 3: Add new class entries and a Milestone 13 documentation section to CLAUDE.md**

In CLAUDE.md's description of `lib/McsCore/src/domain/`, `ports/`, `adapters/`, and `application/` classes, add entries for `MegaStationConfig`/`MegaConfig`, `ConfigurationRepository`, `InMemoryConfigurationRepository`/`EepromConfigurationRepository`, and `MegaCommissioningSession` — short descriptions matching the terse style of their neighbors.

Add a new section following this project's established per-milestone documentation pattern (see the "Milestone 12: Routes (Mega panel)" section for the expected level of detail: net effect, test coverage added, non-goals). Cover: the `ConfigurationRepository` port and its two implementations; the EEPROM schema (139 bytes, magic+version+config); `MegaCommissioningSession`'s 3 commands and its draft-then-save design (no EEPROM wear from every keystroke); the `show` command living in `main.cpp` rather than the session class, and why (a 4-station dump doesn't fit in the single 32-byte `FixedString32` response every other command returns — a correction made during plan-writing, not the original design doc, worth noting explicitly the way this project corrects discovered spec imprecisions in place rather than silently); the new `Serial.begin()` call (the Mega's first-ever use of Serial); test coverage added (`test_mega_config` 6 cases, `test_in_memory_configuration_repository` 3 cases, `test_mega_commissioning_session` 11 cases, plus one new case in the existing `test_fixed_string32` suite — 57 → 61 suites, `test_fixed_string32` itself not counted as new since it already existed); non-goals (no pin persistence, no route persistence, no CRC/checksum, no generic command-parsing pipeline, no `Logger` class, no `reset`-to-defaults command).

- [ ] **Step 4: Update README.md**

Find the current "Next Steps" Mega bullet:

```
- **Mega 2560 panel:** verify the electrical LocoNet interface and confirm DR5000/DR4018
  behavior on hardware across all 4 stations, both send and feedback (finish Milestone 8's
  hardware pass), plus the 2 new route buttons (Milestone 12, done in firmware — see
  `CLAUDE.md`'s Milestone 12 section); then persistent configuration (Milestone 13).
```

Replace with:

```
- **Mega 2560 panel:** verify the electrical LocoNet interface and confirm DR5000/DR4018
  behavior on hardware across all 4 stations, both send and feedback (finish Milestone 8's
  hardware pass), plus the 2 route buttons (Milestone 12) and serial commissioning
  (Milestone 13, done in firmware — see `CLAUDE.md`'s Milestone 13 section). This was the
  last item on the Mega panel's original roadmap; only physical hardware bring-up remains.
```

Also re-read and update the "## Current Status" Mega paragraph (its exact current wording — "Milestones 1-12 are complete...") to say 1-13, and its closing sentence about persistent configuration "not yet started" to reflect it's done on the programming side.

- [ ] **Step 5: Add a persistent-config bring-up subsection to the hardware checklist**

In `docs/HARDWARE_BRINGUP_CHECKLIST.md`, immediately after the existing `### 1.5 Route buttons (Milestone 12)` subsection and before the `## Part 2` separator, add:

```markdown
### 1.6 Persistent configuration (Milestone 13)

**Reference:** `src/mega/main.cpp` for the serial baud rate (115200,
matching the ESP32 side's own convention) and the 3 commissioning commands
(`address`, `name`, `save`) plus the `show` command.

1. Connect via `pio device monitor` (or any serial terminal) at 115200
   baud. Type `show` and confirm it prints all 4 stations' current
   address/name — on a factory-fresh board (nothing ever saved), these
   should be the compile-time defaults (101-104, "Turnout 1".."Turnout 4").
2. Type `address 1 999` then `show` again — confirm station 1's address
   is now 999 in the draft, but a power cycle at this point (before
   `save`) should revert it back to the default, since nothing was
   persisted yet.
3. Type `address 1 999` then `save`, then power-cycle the board. Type
   `show` again — confirm station 1 now shows 999, surviving the power
   cycle (this is the actual EEPROM round-trip, not just the in-memory
   draft).
4. Type `name 1 MainYard` then `save`, power-cycle, `show` — confirm the
   name persists the same way.
5. Confirm station 1 still throws/closes correctly over LocoNet using
   its *new* address (999) after the change — the effective address used
   by `TurnoutStation`/`TurnoutControl` is resolved once at boot from
   whatever's in EEPROM, not re-read live.

**Done when:** `show`/`address`/`name`/`save` all work as described, and
a changed address/name survives a power cycle.
```

Also correct the existing note near this file's `### 1.2 Flash and boot` subsection (which currently says something like "This firmware currently has no serial startup banner... silence after upload is the expected 'it's running' signal") — this is now stale, since the Mega firmware calls `Serial.begin()` for the first time as of this milestone and genuinely responds to typed commands. Annotate in place (don't delete) to note that serial commissioning now exists, so a terminal is no longer expected to stay silent if the operator types something — read the live file first to get the exact current wording before editing.

- [ ] **Step 6: Run the staleness sweep**

```bash
grep -ni "milestone 13\|persistent config\|not yet started\|next milestone\|configurationrepository" CLAUDE.md internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md README.md docs/HARDWARE_BRINGUP_CHECKLIST.md
```

Read every hit's surrounding context (not just the matched line) and confirm every claim this milestone makes false is now corrected by Steps 1-5 above. If the sweep surfaces a location not already covered, annotate it in place rather than ignoring it.

**Known, deliberately out-of-scope finding (same one Milestone 12's plan already flagged):** the roadmap doc's large master checklist section further down the file still shows long-done items like "TDD `Indicator`"/"TDD `Button`" as unchecked `[ ]` — this drift predates both Milestone 12 and 13 and spans the whole document. Do not attempt to reconcile it here either.

- [ ] **Step 7: Verify the sweep is clean**

Re-run the grep from Step 6. Confirm every hit that was made false by this milestone is now annotated, and that no hit describing already-correct, still-true behavior was altered.

- [ ] **Step 8: Commit**

```bash
git add CLAUDE.md internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md README.md docs/HARDWARE_BRINGUP_CHECKLIST.md
git commit -m "$(cat <<'EOF'
. d Document Milestone 13 (Mega panel persistent config)

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
