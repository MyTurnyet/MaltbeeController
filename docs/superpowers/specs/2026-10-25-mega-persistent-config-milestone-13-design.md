# Mega Panel Persistent Configuration (Milestone 13) — Design

**Status:** Approved

## Problem

The project's roadmap lists Milestone 13 ("Add Persistent Configuration") as
the final item on the Mega 2560 / LocoNet panel's original roadmap,
following Milestone 12 (Routes, just completed). Confirmed by investigation
before this design was written: nothing exists yet. There is no
`ConfigurationRepository` class, no `Logger` class, and no EEPROM usage
anywhere in `lib/` or `src/` — unlike Milestone 12, where `Route`/
`RouteService` already existed (just built against the wrong model). This
milestone is genuinely greenfield.

The roadmap's own completion criteria:

- Turnout addresses and pin assignments can be loaded from storage.
- Invalid configuration does not leave the system in an unsafe state.

Three real, previously-unsettled design questions were resolved with Paige
before any architecture was locked in (see Decision below for the outcome
of each):

1. **API shape.** The roadmap's own §5.7 sketch has `load()`/`save()` both
   returning `bool`, implying internal state queried via separate getters.
   The ESP32 side's actual, working `ConfigStore` port instead has `load()`
   return a filled config value directly and `save(config)` take one — a
   cleaner interface, and the only other persistence port in this project.
   **Decided: follow the ESP32 `ConfigStore` shape**, keeping the roadmap's
   own name `ConfigurationRepository` for the port itself (an established
   vocabulary term in this project's design docs, independent of the API
   ergonomics question).
2. **Scope of what's persisted.** The roadmap's completion criterion
   literally says "turnout addresses AND pin assignments." Pins are
   normally fixed by physical wiring, and the ESP32 side's own config
   (`NodeConfig`) never persists GPIO pins, only addresses. **Decided: only
   addresses and display names become runtime-configurable; pins stay
   compile-time**, matching the ESP32 precedent and avoiding a stored
   config that could point at hardware that doesn't exist. Routes (the 2
   placeholders from Milestone 12) are explicitly **out of scope** — no
   milestone or roadmap section ever asked for route persistence, and this
   keeps the increment focused, matching this project's established
   scope-narrowing pattern.
3. **Commissioning mechanism.** The Mega has zero serial/command
   infrastructure today (confirmed: `src/mega/main.cpp` never calls
   `Serial.begin()` or reads any input). The ESP32 side's full
   `CommandLineParser`/`ParsedCommand`/`CommissioningSession` layering was
   considered and **rejected as disproportionate** to this milestone's much
   smaller command set. **Decided: a minimal, hand-parsed serial interface**
   — one small, native-testable application class handling a handful of
   line commands directly, no generic parser pipeline mirrored from the
   ESP32 side.

## Decision

### Domain (`lib/McsCore/src/domain/`)

```cpp
// MegaStationConfig.h
#pragma once

#include "FixedString32.h"

struct MegaStationConfig
{
    int address = 0;
    FixedString32 name;
};
```

```cpp
// MegaConfig.h
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

`validate()` returns `true` only if every station's address is within
`[kMinAddress, kMaxAddress]` and no two stations share an address — the
Mega always has exactly 4 physically-wired stations (unlike the ESP32
panel's partial-commissioning model), so there's no "0 means unconfigured"
exception here; every station is expected to have a real address at all
times, whether loaded or defaulted.

### Ports (`lib/McsCore/src/ports/`)

```cpp
// ConfigurationRepository.h
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

### Adapters (`lib/McsCore/src/adapters/`)

**`InMemoryConfigurationRepository`** — a real, reusable implementation
(not merely a test double; the roadmap's own §8/§5.7 explicitly lists it as
one of `ConfigurationRepository`'s intended implementations, for
"configuration tests without EEPROM"). Holds a `MegaConfig` in a member
variable; `load()` returns it, `save()` stores it and returns `true`. No
Arduino dependency — used by native tests and available standalone.

**`EepromConfigurationRepository`** — `#ifdef ARDUINO`-guarded, real
hardware, using the AVR `<EEPROM.h>` library directly (`EEPROM.read()`/
`EEPROM.write()`/`EEPROM.put()`/`EEPROM.get()`). Schema, written starting
at EEPROM address 0:

| Offset | Size | Field |
|---|---|---|
| 0 | 2 bytes | magic constant (`0xBEEF`, a fixed `uint16_t`) |
| 2 | 1 byte | schema version (`1`) |
| 3 | 34 bytes × 4 | 4 × `MegaStationConfig` (2-byte `int` address + 32-byte `FixedString32` name) |

Total: **3 + 136 = 139 bytes**, against the ATmega2560's 4096-byte EEPROM
(3.4% used) — no capacity concern at all. `load()` reads the magic and
version first; if either doesn't match, returns a `MegaConfig` with all
addresses `0` (immediately fails `validate()`, triggering the compile-time
fallback described below) rather than attempting to interpret
uninitialized or wrong-schema EEPROM bytes as real station data. No CRC or
checksum beyond the magic/version check — a deliberate scope decision
(noted under Non-goals); the chance of corrupted bytes coincidentally
matching both the magic constant and version byte is vanishingly small in
practice, and the roadmap's stated completion criteria don't ask for
corruption-detection beyond "safe defaults when invalid."

### Application (`lib/McsCore/src/application/`)

**`MegaCommissioningSession`** — fully native-testable, no Serial
dependency. Holds a `MegaConfig` draft (initialized from
`repository.load()`, falling back to the compile-time defaults if
`!draft.validate()`) and the `ConfigurationRepository&` it was constructed
with.

```cpp
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

`handleLine()` parses one of 4 commands and returns a short text response
(a `FixedString32`, this project's established fixed-capacity string type
— no `std::string` on AVR):

- `address <1-4> <value>` — stage a new address for that station in
  `draft_` (1-indexed to match how an operator thinks about "station 1"
  through "station 4"; converted to a 0-indexed array access internally).
  Validates the station number and address range before staging; returns
  an error message otherwise, without mutating `draft_`.
- `name <1-4> <text>` — stage a new display name (truncated to 31 characters
  plus the null terminator, `FixedString32`'s existing truncation behavior
  — no new truncation logic needed).
- `show` — returns a formatted dump of the current (possibly unsaved)
  draft's 4 stations.
- `save` — calls `repository_.save(draft_)`; returns success or failure
  based on the real return value, never assumed.

An unrecognized command, or a malformed one (wrong argument count, a
non-numeric address/station number, an out-of-range station number),
returns a short error message and leaves `draft_` unchanged — matching this
project's established "reject and report, don't guess" pattern (e.g.
`WebFormCommissioningAdapter`'s own validation on the ESP32 side).

### `src/mega/main.cpp` wiring

```cpp
EepromConfigurationRepository configRepository;
constexpr MegaConfig DEFAULT_CONFIG = { /* built from STATION_CONFIGS's existing address/name fields */ };
MegaCommissioningSession commissioningSession(configRepository, DEFAULT_CONFIG);
```

The effective address/name for each of the 4 `TurnoutStation`s come from
`commissioningSession.draft()` (which is already either the loaded,
validated config or the compile-time default, resolved once at
construction) — pins continue to come from the existing compile-time
`STATION_CONFIGS` table exactly as today, merged with the draft's
address/name at the point each `TurnoutStation` is constructed. Exact
merge mechanics (global-scope construction vs. `setup()`-time construction
of the final per-station `TurnoutConfig` values) are an implementation
detail worked out in the plan, informed by a fresh read of the current
file — not fixed here.

`main.cpp` gains a thin Serial-reading loop: accumulate characters into a
line buffer until a newline, then call
`commissioningSession.handleLine(buffer)` and print the returned response
— no generic command-parsing layer, consistent with the "minimal, hand-
parsed" decision above.

### Testing

- `test/test_mega_config/test_main.cpp` — `MegaConfig::validate()`: valid
  config passes; an out-of-range address (too low, too high) fails; a
  duplicate address between two stations fails.
- `test/test_in_memory_configuration_repository/test_main.cpp` — `save()`
  then `load()` round-trips the exact config; `load()` before any `save()`
  returns a default-constructed (all-zero-address, immediately-invalid)
  `MegaConfig`.
- `test/test_mega_commissioning_session/test_main.cpp` — `address`/`name`
  commands stage changes into the draft without touching the repository
  until `save`; `save` round-trips through the repository; `show` reflects
  unsaved changes; malformed commands (bad station number, non-numeric
  value, out-of-range address, wrong argument count) are rejected with an
  error and leave the draft unchanged; a fresh session backed by a
  repository with no prior `save()` starts from the compile-time defaults
  (since the in-memory repository's own `load()` returns an invalid,
  all-zero config, which fails `validate()` and triggers the fallback).
- `EepromConfigurationRepository` is `#ifdef ARDUINO`-guarded, no native
  test — same precedent as `TurnoutStation`, `MrrwaLocoNetSwitchDriver`,
  and every other hardware-shim adapter in this project.
- `src/mega/main.cpp` itself gets no native test — composition-root-only,
  consistent with every prior `main.cpp`-only change in this project.

## Non-goals

- GPIO pin assignments stay compile-time, never persisted.
- Route definitions (the 2 Milestone 12 placeholders) are not persisted by
  this milestone.
- No CRC/checksum beyond the 2-byte magic + 1-byte version check.
- No generic command-parsing pipeline mirrored from the ESP32 side
  (`CommandLineParser`/`ParsedCommand`/`CommissioningSession`) — a
  deliberately smaller, hand-parsed design for this milestone's much
  smaller command set.
- No `Logger` class (the roadmap's §5.6 design, never built) — this
  milestone's error/response reporting goes through `MegaCommissioningSession`'s
  own return values, printed directly by `main.cpp`, not a general logging
  abstraction.
- No `reset`-to-defaults serial command — not asked for by the roadmap's
  completion criteria; can be added later if wanted.
