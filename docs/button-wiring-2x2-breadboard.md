# 2×2 Test Board — Breadboard Placement (ESP32 panel)

Breadboard-placement detail for the **2×2 test board** (T1, T2, T5, T6) from
`docs/button-wiring.md`. That file gives the row/column net diagram for the
matrix; this file maps those same nets onto actual breadboard columns and
rows, the same way `docs/led-wiring.md` maps the LED circuit onto a
breadboard.

## Circuit (recap)

```
Row GPIO ──wire──┤ SW ├──wire── Column GPIO ── 10kΩ ── 3.3V
                  momentary, N.O.   (external pull-up — columns 34/35/36/39
                                     are input-only, no internal pull-up)
```

Two things make the matrix different from a single switch: **row nets and
column nets are each shared by two buttons**, and each button's two legs sit
on opposite sides of the breadboard's center gap so they land on separate
nodes.

## Worked example — T1 (Row 1 × Column 1)

```
                            SW1 — T1
                      (straddles center gap)
                                │
          ┌─────────────────────┴─────────────────────┐
          │                                            │
   column 3, row A                             column 3, row F
   (Row 1 net)                                 (Column 1 net)
          │                                            │
   also column 8, row A                        also column 5, row F
   (SW2 / T2's row leg)                         (SW3 / T5's column leg)
          │                                            │
          ▼                                         [10kΩ]
    ESP32 GPIO 18                                      │
                                              red (+) 3.3V bus
                                                        │
                                                  → ESP32 3.3V

                               column 3, row J ──────── → ESP32 GPIO 34
```

T1's row leg lands in the **same breadboard column** as the GPIO 18 wire
(column 3) — that column's rows A–E are all one electrical node, so no extra
jumper ties them together. The same is true one block down: T1's column leg,
the pull-up resistor, and the GPIO 34 wire all land in column 3's rows F–J,
which is a *different* node from A–E because the center gap separates them.

## Full 2×2 placement table

| Signal | From | Through | To |
|---|---|---|---|
| GPIO 18 signal wire | ESP32 GPIO 18 | — | column 3, row A |
| SW1 — T1 | column 3, row A (row leg) | straddles center gap | column 3, row F (column leg) |
| Row 1 jumper | column 3, row A | spans columns 3→8 | column 8, row A (SW2 / T2's row leg) |
| SW2 — T2 | column 8, row A (row leg) | straddles center gap | column 8, row F (column leg) |
| GPIO 19 signal wire | ESP32 GPIO 19 | — | column 5, row B |
| Row 2 jumper | column 5, row B | spans columns 5→10 | column 10, row B (SW4 / T6's row leg) |
| SW3 — T5 | column 5, row B (row leg) | straddles center gap | column 5, row F (column leg) |
| SW4 — T6 | column 10, row B (row leg) | straddles center gap | column 10, row F (column leg) |
| Column 1 jumper | column 3, row F | spans columns 3→5 | column 5, row F (SW3 / T5's column leg) |
| Column 2 jumper | column 8, row F | spans columns 8→10 | column 10, row F (SW4 / T6's column leg) |
| Resistor (Column 1 pull-up) | column 3, row I | 10kΩ | red (+) 3.3V rail |
| Resistor (Column 2 pull-up) | column 8, row I | 10kΩ | red (+) 3.3V rail |
| GPIO 34 signal wire | column 3, row J | — | ESP32 GPIO 34 |
| GPIO 35 signal wire | column 8, row J | — | ESP32 GPIO 35 |
| 3.3V jumper | red (+) rail | — | → ESP32 3.3V |

Row jumpers sit above the gap (rows A–E block), column jumpers and resistors
sit below it (rows F–J block) — the row/column split in the wiring maps
directly onto the breadboard's top/bottom split.

## Notes

- Only **2 resistors** total for the whole 2×2 board — one per column, not
  one per button. A resistor pulls up the shared column net, not an
  individual switch.
- No pull-up is needed on the row pins (GPIO 18/19) — they're driven
  push-pull outputs, not read as inputs. Same as in `button-wiring.md`.
- Row 1 and Row 2's jumpers (rows A and B) and Column 1 and Column 2's
  jumpers (both row G) can use any free row in their block — A/B and G were
  picked so parallel jumpers don't fight for the same holes, not because
  those specific rows matter electrically.
- Only one button is expected pressed at a time, so no anti-ghosting diodes
  are needed, same as the schematic in `button-wiring.md`.

## Scaling to the full 3×4 matrix

Same pattern, more columns per bus: each row jumper spans 4 buttons instead
of 2 (one per breadboard-column group), each column jumper spans 3 buttons
instead of 2, and there are 4 pull-up resistors total (one per column) instead
of 2. The GPIO-to-row/column map for all 12 buttons is in the
[full matrix table](button-wiring.md#full-3×4-matrix-all-12-buttons) in
`button-wiring.md`.

Indicator LED wiring (for context, not covered by this button-focused
diagram) is in [`led-wiring.md`](led-wiring.md).
