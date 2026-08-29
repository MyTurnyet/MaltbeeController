# ESP32 Matrix Button Scan (Sub-project #3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the software that scans the ESP32 panel's 3×4 button matrix
(12 turnout pushbuttons on 7 GPIOs) and exposes each cell as a plain
`DigitalInput` port, so the existing `Button` domain class can debounce and
edge-detect a matrix cell exactly like it already does a single GPIO —
without any change to `Button` itself.

**Architecture:** Two new classes in `lib/McsEsp32/` (not `lib/McsCore/` —
this is ESP32-only; the Mega uses one GPIO per button and never needs this,
and `McsCore` must stay AVR/STL-safe while this code can freely use
`std::array`). `MatrixScanner` (domain) owns the row/column ports and
advances one row per `update()` call, caching that row's column readings.
`MatrixDigitalInput` (adapter) implements `McsCore`'s `DigitalInput` port
for one fixed `(row, col)` cell by forwarding to the scanner's cache.

**Tech Stack:** C++17, PlatformIO, Catch2 (native tests). No hardware, no
Arduino-guarded code, and no `esp32dev`/`src/esp32/main.cpp` changes in this
plan — the row↔column↔turnout mapping and real GPIO construction are
composition-root work (sub-project #7), not this plan's.

## Global Constraints

- Domain and port headers must compile under `native` with no `Arduino.h`
  (this plan touches no Arduino-guarded files at all).
- `lib/McsEsp32` targets `native` and `esp32dev` only, both with full
  libstdc++ — `std::array` is fine throughout, no `FixedString32`.
- No mocking framework. `FakeDigitalInput`/`FakeDigitalOutput`
  (`test/support/`) already exist and are reused unchanged by both tasks.
- Row/column indices are 0-based throughout this API (`row` ranges 0–2,
  `col` ranges 0–3) — matching plain array indexing, not
  `docs/button-wiring.md`'s 1-based human-readable table.
- `MatrixScanner::update()` must never assert more than one row output at
  once, and after the very first call must never leave zero rows asserted —
  both are real electrical requirements (see the design spec's "Row
  electrical polarity" section), not just style preferences.
- No settle-delay/timing logic — read a row's columns immediately after
  asserting it, within the same `update()` call. This was a deliberate
  design choice (see
  `docs/superpowers/specs/2026-08-29-esp32-matrix-button-scan-design.md`),
  not an oversight to fix.
- This plan does **not** touch `src/esp32/main.cpp`, `Button`,
  `TurnoutControl`, or `TurnoutStation` — all of that is sub-project #7's
  composition-root work, or already-unmodified reuse.
- Commit messages use this project's Arlo's Commit Notation (ACN) —
  `<risk symbol> <intention letter> <description>` — per `CLAUDE.md`.

---

### Task 1: `MatrixScanner`

**Files:**
- Create: `lib/McsEsp32/src/domain/MatrixScanner.h`
- Create: `lib/McsEsp32/src/domain/MatrixScanner.cpp`
- Test: `test/test_matrix_scanner/test_main.cpp`

**Interfaces:**
- Consumes: `DigitalOutput`/`DigitalInput` (existing, `lib/McsCore/src/ports/`,
  rooted includes `"ports/DigitalOutput.h"`/`"ports/DigitalInput.h"`,
  matching `McsLoconet`'s existing convention for the same kind of
  cross-library dependency); `FakeDigitalOutput`/`FakeDigitalInput`
  (existing, `test/support/`).
- Produces: `class MatrixScanner { static constexpr int kRowCount = 3;
  static constexpr int kColumnCount = 4; MatrixScanner(std::array<
  DigitalOutput*, kRowCount> rows, std::array<DigitalInput*, kColumnCount>
  columns); void update(); bool isActive(int row, int col) const; }`. Task 2
  consumes this class by reference.

- [ ] **Step 1: Write the failing tests**

Create `test/test_matrix_scanner/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/MatrixScanner.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeDigitalOutput.h"

TEST_CASE("isActive returns false for every cell before any update() call")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    for (int row = 0; row < MatrixScanner::kRowCount; ++row)
    {
        for (int col = 0; col < MatrixScanner::kColumnCount; ++col)
        {
            REQUIRE_FALSE(scanner.isActive(row, col));
        }
    }
}

TEST_CASE("the first update() call asserts row 0 and only row 0")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    scanner.update();

    REQUIRE(row0.isSet());
    REQUIRE_FALSE(row1.isSet());
    REQUIRE_FALSE(row2.isSet());
}

TEST_CASE("each update() deasserts the previous row before asserting the next")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    scanner.update();
    scanner.update();

    REQUIRE_FALSE(row0.isSet());
    REQUIRE(row1.isSet());
    REQUIRE_FALSE(row2.isSet());

    scanner.update();

    REQUIRE_FALSE(row0.isSet());
    REQUIRE_FALSE(row1.isSet());
    REQUIRE(row2.isSet());
}

TEST_CASE("scanning wraps back to row 0 after a full cycle")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    scanner.update();
    scanner.update();
    scanner.update();
    scanner.update();

    REQUIRE(row0.isSet());
    REQUIRE_FALSE(row1.isSet());
    REQUIRE_FALSE(row2.isSet());
}

TEST_CASE("a row's cached readings reflect the column state at the moment it was scanned")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    col0.active = true;
    col2.active = true;

    scanner.update();

    REQUIRE(scanner.isActive(0, 0));
    REQUIRE_FALSE(scanner.isActive(0, 1));
    REQUIRE(scanner.isActive(0, 2));
    REQUIRE_FALSE(scanner.isActive(0, 3));
}

TEST_CASE("a row's cached readings are not clobbered by scanning other rows afterward")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    col1.active = true;
    scanner.update();

    col1.active = false;
    col3.active = true;
    scanner.update();

    REQUIRE(scanner.isActive(0, 1));
    REQUIRE(scanner.isActive(1, 3));
    REQUIRE_FALSE(scanner.isActive(1, 1));
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_matrix_scanner`
Expected: FAIL to compile — `domain/MatrixScanner.h` does not exist yet.

- [ ] **Step 3: Write `lib/McsEsp32/src/domain/MatrixScanner.h`**

```cpp
#pragma once

#include <array>

#include "ports/DigitalInput.h"
#include "ports/DigitalOutput.h"

class MatrixScanner
{
public:
    static constexpr int kRowCount = 3;
    static constexpr int kColumnCount = 4;

    MatrixScanner(std::array<DigitalOutput*, kRowCount> rows,
                  std::array<DigitalInput*, kColumnCount> columns);

    void update();

    [[nodiscard]] bool isActive(int row, int col) const;

private:
    std::array<DigitalOutput*, kRowCount> rows_;
    std::array<DigitalInput*, kColumnCount> columns_;
    std::array<std::array<bool, kColumnCount>, kRowCount> cache_{};
    int currentRow_ = -1;
};
```

- [ ] **Step 4: Write `lib/McsEsp32/src/domain/MatrixScanner.cpp`**

```cpp
#include "MatrixScanner.h"

MatrixScanner::MatrixScanner(std::array<DigitalOutput*, kRowCount> rows,
                              std::array<DigitalInput*, kColumnCount> columns)
    : rows_(rows), columns_(columns)
{
}

void MatrixScanner::update()
{
    if (currentRow_ >= 0)
    {
        rows_[currentRow_]->set(false);
    }

    currentRow_ = (currentRow_ + 1) % kRowCount;
    rows_[currentRow_]->set(true);

    for (int col = 0; col < kColumnCount; ++col)
    {
        cache_[currentRow_][col] = columns_[col]->isActive();
    }
}

bool MatrixScanner::isActive(const int row, const int col) const
{
    return cache_[row][col];
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_matrix_scanner`
Expected: PASS, all 6 test cases green.

- [ ] **Step 6: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every existing suite plus the new one green (24 suites
total — 23 existing + 1 new).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/domain/MatrixScanner.h lib/McsEsp32/src/domain/MatrixScanner.cpp test/test_matrix_scanner/test_main.cpp
git commit -m "! F Add MatrixScanner for the ESP32 panel's 3x4 button matrix"
```

---

### Task 2: `MatrixDigitalInput`

**Files:**
- Create: `lib/McsEsp32/src/adapters/MatrixDigitalInput.h`
- Create: `lib/McsEsp32/src/adapters/MatrixDigitalInput.cpp`
- Test: `test/test_matrix_digital_input/test_main.cpp`

**Interfaces:**
- Consumes: `MatrixScanner` (Task 1, by reference); `DigitalInput`
  (existing, `lib/McsCore/src/ports/DigitalInput.h`, rooted include).
- Produces: `class MatrixDigitalInput final : public DigitalInput {
  MatrixDigitalInput(MatrixScanner& scanner, int row, int col); bool
  isActive() const override; }`. No later task in this plan consumes this —
  sub-project #7 constructs 12 of these directly.

- [ ] **Step 1: Write the failing tests**

Create `test/test_matrix_digital_input/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/MatrixDigitalInput.h"
#include "domain/MatrixScanner.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeDigitalOutput.h"

TEST_CASE("MatrixDigitalInput reflects the scanner's cached reading for its cell")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});
    MatrixDigitalInput input(scanner, 1, 2);

    REQUIRE_FALSE(input.isActive());

    col2.active = true;
    scanner.update();
    scanner.update();

    REQUIRE(input.isActive());
}

TEST_CASE("MatrixDigitalInput for a different cell is unaffected by another cell's state")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});
    MatrixDigitalInput inputA(scanner, 0, 0);
    MatrixDigitalInput inputB(scanner, 0, 1);

    col0.active = true;
    scanner.update();

    REQUIRE(inputA.isActive());
    REQUIRE_FALSE(inputB.isActive());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_matrix_digital_input`
Expected: FAIL to compile — `adapters/MatrixDigitalInput.h` does not exist
yet.

- [ ] **Step 3: Write `lib/McsEsp32/src/adapters/MatrixDigitalInput.h`**

```cpp
#pragma once

#include "../domain/MatrixScanner.h"
#include "ports/DigitalInput.h"

class MatrixDigitalInput final : public DigitalInput
{
public:
    MatrixDigitalInput(MatrixScanner& scanner, int row, int col);

    [[nodiscard]] bool isActive() const override;

private:
    MatrixScanner& scanner_;
    int row_;
    int col_;
};
```

- [ ] **Step 4: Write `lib/McsEsp32/src/adapters/MatrixDigitalInput.cpp`**

```cpp
#include "MatrixDigitalInput.h"

MatrixDigitalInput::MatrixDigitalInput(MatrixScanner& scanner, const int row, const int col)
    : scanner_(scanner), row_(row), col_(col)
{
}

bool MatrixDigitalInput::isActive() const
{
    return scanner_.isActive(row_, col_);
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_matrix_digital_input`
Expected: PASS, both test cases green.

- [ ] **Step 6: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every suite green — 25 suites total (23 existing + 2 new
from Tasks 1-2).

- [ ] **Step 7: Build for the ESP32 and Mega targets to confirm both are unaffected**

Run: `pio run -e esp32dev`
Expected: SUCCESS — confirms both new files compile cleanly under the real
ESP32 toolchain too (via `McsEsp32`'s existing `lib_deps` entry in
`platformio.ini`), even though nothing here is `#ifdef ARDUINO`-guarded
(none of it touches Arduino APIs directly).

Run: `pio run -e megaatmega2560`
Expected: SUCCESS — `McsEsp32` is `lib_ignore`d there, so this plan's files
are never compiled for that target.

- [ ] **Step 8: Commit**

```bash
git add lib/McsEsp32/src/adapters/MatrixDigitalInput.h lib/McsEsp32/src/adapters/MatrixDigitalInput.cpp test/test_matrix_digital_input/test_main.cpp
git commit -m "! F Add MatrixDigitalInput adapter for the ESP32 button matrix"
```

---

## Definition of Done for this Plan

- [ ] `pio test -e native` passes, including the 2 new suites
      (`test_matrix_scanner`, `test_matrix_digital_input`) — 25 suites total
      (23 existing + 2 new).
- [ ] `pio run -e esp32dev` builds cleanly with both new files compiled in.
- [ ] `pio run -e megaatmega2560` still builds cleanly (unaffected —
      `McsEsp32` is `lib_ignore`d there — verify rather than assume).
- [ ] Two commits on `main` (or a feature branch), each `! F` — one per
      task.
- [ ] Nothing in this plan touches `src/esp32/main.cpp`, `Button`,
      `TurnoutControl`, `TurnoutStation`, or implements the row↔column↔
      turnout mapping table or toggle-vs-throw/close command semantics —
      all deferred to sub-project #7 (and #6-adjacent work for the command
      semantics), per
      `docs/superpowers/specs/2026-08-29-esp32-matrix-button-scan-design.md`'s
      Non-goals.
