# Panel Button Wiring (ESP32 panel)

Reference for wiring the 12 turnout pushbuttons to the ESP32 as a 3×4 matrix.
Unlike the Mega panel (one GPIO per button, see git history if you need that
version), the ESP32 panel scans a matrix to fit 12 buttons on 7 GPIOs: 3 row
outputs (driven LOW one at a time) and 4 column inputs (read for a LOW while
each row is active). See `docs/ESP32_Turnout_Panel_Implementation.md` for the
full hardware plan this diagram is drawn from.

Two interactive breadboard diagrams were built as Claude artifacts; this file
is the durable, offline copy of both — a minimal **2×2 test board** to prove
the wiring/scanning before committing to hardware, and the **full 3×4
matrix** as the build reference.

## Circuit (per button)

```
Row GPIO ──wire──┤ SW ├──wire── Column GPIO ── 10kΩ ── 3.3V
                  momentary, N.O.   (external pull-up — columns 34/35/36/39
                                     are input-only, no internal pull-up)
```

- Idle: the column reads HIGH (pulled up to 3.3V through its 10kΩ resistor).
- Scanning: one row is driven LOW at a time; the rest are inactive.
- Pressed: a button connects its row to its column, so if that button's row
  is the one currently driven LOW, its column reads LOW too.
- Only one button is expected pressed at a time, so no anti-ghosting diodes
  are needed per button.

## 2×2 test board (T1, T2, T5, T6)

Rows 1–2 (GPIO 18, 19) × columns 1–2 (GPIO 34, 35) — the smallest slice that
exercises the real scan algorithm.

```
        GPIO 18 (Row 1) ───┬──────────────────────┬─── GPIO 19 (Row 2)
                            │                        │
                          ┌─┴─┐  T1            ┌─┴─┐  T5
                          └─┬─┘                └─┬─┘
                            │                     │
                  Col 1 ────┴─────────────────────┘──── GPIO 34
                            │                              │
                          [10kΩ]                     (pull-up)
                            │                              │
                       3.3V bus ◄──────────────────────────┘

        GPIO 18 (Row 1) ───┬──────────────────────┬─── GPIO 19 (Row 2)
                            │                        │
                          ┌─┴─┐  T2            ┌─┴─┐  T6
                          └─┬─┘                └─┬─┘
                            │                     │
                  Col 2 ────┴─────────────────────┘──── GPIO 35
                            │                              │
                          [10kΩ]                     (pull-up)
                            │                              │
                       3.3V bus ◄──────────────────────────┘
```

| Turnout | Row | Column | Row GPIO | Column GPIO |
|---------|-----|--------|----------|--------------|
| T1 | 1 | 1 | GPIO 18 | GPIO 34 |
| T2 | 1 | 2 | GPIO 18 | GPIO 35 |
| T5 | 2 | 1 | GPIO 19 | GPIO 34 |
| T6 | 2 | 2 | GPIO 19 | GPIO 35 |

Breadboard placement: each button straddles the center gap on its own
breadboard column (so its two legs land on genuinely separate nodes). The
top-block leg (row A–E) buses to that button's row GPIO; the bottom-block leg
(row F–J) buses to that button's column net, which also carries the 10kΩ
pull-up to the 3.3V rail and the wire out to the column GPIO.

Example: Row 2 driven LOW, Column 1 reads LOW → **T5 is pressed**.

## Full 3×4 matrix (all 12 buttons)

Rows 1–3 (GPIO 18, 19, 21) × columns 1–4 (GPIO 34, 35, 36, 39) — same wiring
pattern as the test board, just 4 button groups (one per column) instead of
2, each with 3 buttons (one per row) instead of 2.

| | Col 1 (GPIO 34) | Col 2 (GPIO 35) | Col 3 (GPIO 36) | Col 4 (GPIO 39) |
|---|---|---|---|---|
| **Row 1 (GPIO 18)** | T1 | T2 | T3 | T4 |
| **Row 2 (GPIO 19)** | T5 | T6 | T7 | T8 |
| **Row 3 (GPIO 21)** | T9 | T10 | T11 | T12 |

Drawn on one extended breadboard strip for clarity in the artifact diagram —
in practice, wiring 3 rows typically spans two joined half-size breadboards,
since a single strip only gives two independent row-groups (A–E / F–J) and a
3-row matrix needs a third.

## Notes

- No pull-up is needed on the row pins — they're driven push-pull outputs,
  not read as inputs.
- GPIO 34, 35, 36, 39 are **input-only** on the ESP32 and have no usable
  internal pull-up, unlike the Mega's `INPUT_PULLUP` buttons (see the Mega
  version of this doc in git history) — the external 10kΩ resistor is
  required, not optional.
- Debouncing and new-press edge detection (not repeat-while-held) are a
  software concern, mirroring what `Button` already does on the Mega side,
  just against a matrix-scanned input instead of a single `digitalRead`.
- Pins intentionally avoided elsewhere on this board: GPIO 1/3 (USB serial),
  GPIO 6–11 (flash), GPIO 0/2/5/12/15 (boot-strapping) — see "Pins
  intentionally avoided" in the implementation doc.

## Repeat per turnout

Indicator LED wiring (for context, not covered by this button-focused
diagram) is in [`led-wiring.md`](led-wiring.md).
