# 2×2 Test Board — Breadboard Placement (ESP32 panel)

Breadboard-placement detail for the 2×2 test board (T1, T2, T5, T6) from
`docs/button-wiring.md`. That file gives the row/column net diagram for the
matrix; this file maps those same nets onto actual breadboard columns and
rows, the same way `docs/led-wiring.md` maps the LED circuit onto a
breadboard.

**Fix in this revision:** the original layout packed 3 wires/legs into a
single hole in a few places (e.g. column 3, row A had to carry the GPIO18
wire, SW1's leg, *and* the Row 1 jumper simultaneously). Every column's
row-block (A–E, or F–J) is still one electrical node, so members of the same
net don't need to share a hole — they just need to land *somewhere* in that
5-row block. This version gives each wire/leg its own row.

## Circuit (recap)

```
Row GPIO ──wire──┤ SW ├──wire── Column GPIO ── 10kΩ ── 3.3V
                  momentary, N.O.   (external pull-up — columns 34/35/36/39
                                     are input-only, no internal pull-up)
```

Row nets and column nets are each shared by two buttons, and each button's
two legs sit on opposite sides of the breadboard's center gap so they land
on separate nodes.

## Worked example — T1 (Row 1 × Column 1)

```
                            SW1 — T1
                      (straddles center gap)
                                │
          ┌─────────────────────┴─────────────────────┐
          │                                            │
   column 3, row B                             column 3, row F
   (Row 1 net)                                 (Column 1 net)
          │                                            │
   GPIO18 wire → column 3, row A                [10kΩ] → column 3, row I
   Row 1 jumper → column 3, row C                       │
   (to column 8, row A — SW2/T2's row leg)      red (+) 3.3V bus
                                                          │
                                                   → ESP32 3.3V
                               GPIO34 wire → column 3, row J
```

T1's row leg (column 3, row B) shares its electrical node with the GPIO18
wire (row A) and the Row 1 jumper (row C) only because they're all in
column 3's top block — not because they share a hole. Same idea below the
gap: SW1's column leg (row F), the Column 1 jumper (row G), the pull-up
resistor (row I), and the GPIO34 wire (row J) are four separate holes on
the same bottom-block node.

## Full 2×2 placement table

| Signal | Hole | Node (block) |
|---|---|---|
| GPIO 18 signal wire | column 3, row A | Row 1 (col 3, A–E) |
| SW1 — T1, row leg | column 3, row B | Row 1 (col 3, A–E) |
| Row 1 jumper (col 3 → col 8) | column 3, row C → column 8, row A | Row 1 net |
| SW2 — T2, row leg | column 8, row B | Row 1 (col 8, A–E) |
| GPIO 19 signal wire | column 5, row A | Row 2 (col 5, A–E) |
| SW3 — T5, row leg | column 5, row B | Row 2 (col 5, A–E) |
| Row 2 jumper (col 5 → col 10) | column 5, row C → column 10, row A | Row 2 net |
| SW4 — T6, row leg | column 10, row B | Row 2 (col 10, A–E) |
| SW1 — T1, column leg | column 3, row F | Column 1 (col 3, F–J) |
| Column 1 jumper (col 3 → col 5) | column 3, row G → column 5, row F | Column 1 net |
| SW3 — T5, column leg | column 5, row F | Column 1 (col 5, F–J) |
| Resistor (Column 1 pull-up) | column 3, row I → red (+) rail | Column 1 → 3.3V |
| GPIO 34 signal wire | column 3, row J | Column 1 (col 3, F–J) |
| SW2 — T2, column leg | column 8, row F | Column 2 (col 8, F–J) |
| Column 2 jumper (col 8 → col 10) | column 8, row G → column 10, row F | Column 2 net |
| SW4 — T6, column leg | column 10, row F | Column 2 (col 10, F–J) |
| Resistor (Column 2 pull-up) | column 8, row I → red (+) rail | Column 2 → 3.3V |
| GPIO 35 signal wire | column 8, row J | Column 2 (col 8, F–J) |
| 3.3V jumper | red (+) rail, hole near col 3 → ESP32 3.3V | 3.3V bus |

**One item per hole, guaranteed:**
- Each row-block (A–E or F–J) in a given column has at most 4 members
  spread across its 5 rows — never more than one wire/leg per row.
- Rows D/E and H are left free in every block as spares for probing or
  future expansion.
- The two rail connections (Column 1 resistor, Column 2 resistor) use
  different holes on the (+) bus, and the 3.3V-to-ESP32 jumper uses a
  third, separate rail hole — the bus strip is one node end-to-end, so
  they don't need to coincide.

Row jumpers sit above the gap (rows A–E block), column jumpers and
resistors sit below it (rows F–J block) — the row/column split in the
wiring still maps directly onto the breadboard's top/bottom split; the
only change is that each net's members now occupy distinct rows instead
of stacking in one hole.

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
