# ESP32 Matrix Button Scan (Sub-project #3) — Design

This is sub-project **#3** in the ESP32 panel decomposition (see
`docs/superpowers/specs/2026-08-28-esp32-node-config-commissioning-design.md`'s
sub-project table): matrix-scan button input. It has no dependency on 2a/2b/6
(NodeConfig, JMRI transport, and the turnout-control wiring are all
independent of raw button scanning) and feeds into sub-project #5 (ESP32
hardware adapters for #3/#4) and #7 (composition root).

## Problem

The ESP32 panel wires 12 turnout pushbuttons onto a 3×4 matrix (3 row GPIOs,
4 column GPIOs) to fit them on 7 GPIOs instead of 12 — see
`docs/button-wiring.md` for the full breadboard reference and exact GPIO
assignments (rows 18/19/21, columns 34/35/36/39). Reading a matrix cell isn't
a plain `digitalRead()`: a given button's state is only meaningful for the
brief window its row is driven low, so something has to continuously cycle
through the rows and cache each column's reading per row.

This is a UX difference from the Mega panel worth calling out explicitly:
the Mega uses two physical buttons per turnout (`throwButtonPin`/
`closeButtonPin`, see `TurnoutConfig` in
`lib/McsCore/src/adapters/TurnoutStation.h`), but the ESP32's 12-button
matrix implies one toggle-style button per turnout instead. What a press
*means* (toggle vs. throw/close) is explicitly **not** this sub-project's
concern — see Non-goals.

## Decision

Two new classes, both in `lib/McsEsp32/` — not `lib/McsCore/` — since matrix
scanning is ESP32-only (the Mega never needs it and stays on one GPIO per
button) and `McsCore` must stay AVR/STL-safe while this code can freely use
`std::array`.

- **`MatrixScanner`** (domain) owns the row `DigitalOutput&`s and column
  `DigitalInput&`s (both existing `McsCore` ports, consumed via a rooted
  include, matching `McsLoconet`'s existing convention for the same kind of
  cross-library dependency). Each `update()` call advances exactly one row:
  deassert the previous row, assert the next, immediately read that row's
  columns into a cached grid. A full scan cycle takes as many `update()`
  calls as there are rows. No settle-delay timing logic is needed. The
  relevant comparison isn't loop cadence (the column read happens within the
  same `update()` call, microseconds after the row switch, not on a later
  loop tick) — it's the column line's RC recovery time against the read
  timing itself. The 10kΩ pull-up against the roughly 30–60pF of pin-plus-
  breadboard capacitance on a column line gives an RC recovery time on the
  order of 1–2 microseconds to clear the ESP32's ~2.5V input-high threshold,
  while an Arduino-core `digitalWrite`/`digitalRead` round trip on the ESP32
  is itself roughly 1 microsecond. So the real margin is more like 1x–2x,
  not orders of magnitude — tight enough to be worth watching, not tight
  enough to justify a settle-time state machine (mirroring
  `PulsingLocoNetTransport`'s pattern) that would add a timing parameter and
  extra states to test. If cross-row ghosting is ever observed during
  hardware bring-up (a button held on the previous row leaving its column
  line not-yet-recovered, so the next row's read sees a phantom press in
  that column), there's a zero-cost mitigation available with no new timing
  parameter, no new state, and no `Clock` dependency: reorder `update()` to
  read the *current* row's columns first, then deassert/advance/assert for
  the next row, which gives a full loop period of settling time instead of
  microseconds. This is a watch-item for sub-project #5/#8's hardware
  bring-up checklist, not something to implement now.
- **`MatrixDigitalInput`** (adapter) implements `McsCore`'s `DigitalInput`
  port for one fixed `(row, col)` cell: `isActive()` forwards to the
  scanner's cached reading for that cell. This is what lets the existing
  `Button` domain class (`lib/McsCore/src/domain/Button.h`, unmodified)
  consume a matrix cell exactly like it consumes a plain GPIO today.

The row↔column↔turnout mapping (matching `docs/button-wiring.md`'s table)
is **not** built into either class — `MatrixScanner`/`MatrixDigitalInput`
only ever deal in raw `(row, col)` coordinates, the same way
`ArduinoDigitalInput` only ever deals in a raw pin number. Wiring up all 12
`MatrixDigitalInput`s with their correct coordinates and handing them to 12
`Button`s is composition-root work (sub-project #7), matching how
`src/mega/main.cpp`'s `TurnoutConfig` table already does the equivalent
pin-to-turnout mapping today.

Sizing is fixed at 3 rows × 4 columns (12 cells), matching the currently
wired hardware exactly — not parameterized/templated. Nothing in this
project's roadmap calls for a different matrix size, and this matches the
project's existing YAGNI convention (e.g. `NodeConfig::kChannelCount` is
also a plain fixed constant, not a template parameter).

### Row and column electrical polarity (composition-root note, not this sub-project's code)

`docs/button-wiring.md`'s circuit requires the *currently scanned* row to be
driven LOW (so a pressed button on that row pulls its column LOW through
the row) and every *other* row to sit HIGH (so it can never affect any
column). `MatrixScanner` itself stays agnostic to this — it only ever calls
`DigitalOutput::set(true)` to mean "select this row" and `set(false)` to
mean "deselect it," and the port abstraction is exactly what makes that
correct regardless of wiring polarity. The composition root (#7) is
responsible for constructing each row's `ArduinoDigitalOutput` with
`activeLow = true`, so `true` (selected) drives LOW and `begin()`'s default
state is HIGH (deselected) — matching `ArduinoDigitalOutput`'s existing
`activeLow` parameter and `begin()` behavior
(`lib/McsCore/src/adapters/ArduinoDigitalOutput.cpp`) exactly, with no new
mechanism needed. This is called out here so it isn't lost before #7, but
there is no code in this sub-project that depends on it.

The four columns have the same trap, plus an extra parameter to get right.
`ArduinoDigitalInput`'s constructor is `(int pin, bool activeLow, bool
useInternalPullup)`
(`lib/McsCore/src/adapters/ArduinoDigitalInput.h`). Per
`docs/button-wiring.md`, the composition root (#7) must construct each of
the 4 column `ArduinoDigitalInput`s with `activeLow = true` **and**
`useInternalPullup = false`: `activeLow = true` because the column idles
HIGH via the external 10kΩ pull-up and a press pulls it LOW (same polarity
reasoning as the rows); `useInternalPullup = false` because GPIO 34/35/36/39
are input-only pins on the ESP32 with no usable internal pull-up at all —
passing `useInternalPullup = true` would silently do nothing on those pins,
which would look like correct configuration while masking a real wiring
mistake (a missing or miswired external 10kΩ resistor) instead of surfacing
it. See `docs/button-wiring.md`'s "Notes" section for the input-only-pin
detail this depends on.

### Residual same-column short risk (documentation caveat, not a defect)

The one-asserted-row invariant bounds short risk but doesn't fully eliminate
it: unselected rows are actively driven HIGH (push-pull outputs, not
floating), so if two buttons on the *same column* but *different rows* are
pressed at the same instant, the selected (LOW) row and an unselected (HIGH)
row end up connected through two closed switches — a direct output-to-output
short. `docs/button-wiring.md` waives this by assuming only one button is
ever pressed at a time ("Only one button is expected pressed at a time, so
no anti-ghosting diodes are needed per button"), which is a reasonable
operational assumption but is an assumption, not something enforced in
software or hardware. Small series resistors (~220Ω) on the three row
outputs would make even that simultaneous-same-column-press scenario
non-destructive — cheap insurance worth considering during hardware
bring-up (sub-project #8), though not something this sub-project's software
can address.

## Components

Row/column indices throughout this API are 0-based (`row` ranges 0–2,
`col` ranges 0–3), matching plain array indexing. `docs/button-wiring.md`'s
table labels them 1-based ("Row 1"–"Row 3", "Col 1"–"Col 4") for human
readability — sub-project #7 subtracts 1 when building its mapping table
from that doc.

### `MatrixScanner` (`lib/McsEsp32/src/domain/MatrixScanner.h` / `.cpp`)

```cpp
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

`update()`: if `currentRow_ >= 0`, deassert `rows_[currentRow_]`. Advance
`currentRow_ = (currentRow_ + 1) % kRowCount`. Assert `rows_[currentRow_]`.
Read all `kColumnCount` columns into `cache_[currentRow_]`. The `-1` initial
sentinel means the very first `update()` call selects row 0 without trying
to deassert a nonexistent "previous" row.

`isActive(row, col)`: returns `cache_[row][col]`. Before the first full
cycle completes, cells for not-yet-scanned rows read their default-
constructed `false` ("not pressed") — a safe default, matching how a fresh
`Button` also begins unpressed.

### `MatrixDigitalInput` (`lib/McsEsp32/src/adapters/MatrixDigitalInput.h`)

```cpp
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

Trivial forwarding: `isActive()` returns `scanner_.isActive(row_, col_)`.

## Testing

Fully native-testable with the existing `FakeDigitalInput`/
`FakeDigitalOutput` test doubles (`test/support/`) — no new fakes needed.
Construct a `MatrixScanner` with 3 `FakeDigitalOutput`s and 4
`FakeDigitalInput`s, drive the fakes' `active`/`isSet()` state, and assert:

- The first `update()` call asserts row 0 and only row 0 (`rows[0].isSet()
  == true`, `rows[1].isSet() == rows[2].isSet() == false`).
- Each subsequent `update()` deasserts the previous row before asserting
  the next — never two rows asserted at once, never zero.
- After `kRowCount` calls, the cycle wraps back to row 0.
- A row's cached readings reflect exactly the column fakes' state at the
  moment that row was scanned, and stay stable across later calls that scan
  other rows (not clobbered until that row comes up again).
- `isActive(row, col)` before any `update()` call returns `false` for every
  cell (safe default).
- `MatrixDigitalInput::isActive()` for a given `(row, col)` matches
  `MatrixScanner::isActive(row, col)` exactly, including as the scanner's
  cache changes across successive `update()` calls.

## Non-goals

- The row↔column→turnout mapping table (that's sub-project #7's
  composition-root config, mirroring `src/mega/main.cpp`'s `TurnoutConfig`).
- Constructing real `ArduinoDigitalOutput`/`ArduinoDigitalInput` instances
  for the 7 real GPIOs, or calling `begin()` on them — also #7.
- Any change to `Button`, `TurnoutControl`, or `TurnoutStation` — all reused
  completely unmodified via the `DigitalInput` port.
- Toggle-vs-throw/close command semantics for what a matrix button press
  should send — deferred to whichever sub-project designs the ESP32's
  single-button turnout-control variant (#6-adjacent or #7).
- Sub-project #4 (shared-GPIO LED-pair output) — unrelated, independent
  hardware concern, not touched here.
