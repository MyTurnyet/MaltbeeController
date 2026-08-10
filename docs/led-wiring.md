# Indicator LED Wiring (ESP32 panel)

Reference for wiring the red/green turnout-position LEDs to the ESP32.
Unlike the Mega panel (one GPIO per LED, see git history if you need that
version), the ESP32 panel drives a red LED and a green LED from a single
GPIO by wiring them in opposite directions, so 12 turnouts need only 12
GPIOs for 24 LEDs. See `docs/ESP32_Turnout_Panel_Implementation.md` for the
full hardware plan this diagram is drawn from.

**Fix in this revision:** the breadboard placement table had one hole doing
double duty — column 13, row C was carrying both the red LED's anode leg
*and* the start of the red resistor. The red resistor's start has been
moved to column 13, row D (same electrical node — column 13's A–E block —
just a different physical hole), so nothing shares a hole anymore.

## Circuit (per LED pair)

```
GPIO ── resistor ── green LED anode → cathode ── GND

3.3V ── resistor ── red LED anode → cathode ── GPIO
```

The red LED is wired backwards relative to the green one — its cathode
returns to the GPIO pin instead of to ground, and its resistor goes to
3.3V instead of to the GPIO. Each LED gets its own resistor
(680Ω–1kΩ; 1kΩ recommended starting point) — never shared.

| GPIO level | Green | Red |
|---|---|---|
| HIGH (~3.3V) | On | Off |
| LOW (~0V) | Off | On |

- **HIGH:** no potential across the red LED (both ends near 3.3V), so it
  stays off; current flows GPIO → resistor → green LED → GND, so green
  lights.
- **LOW:** no potential across the green LED, so it stays off; current
  flows 3.3V → resistor → red LED → GPIO (sinking), so red lights.

Both LEDs off is not achievable with this wiring — firmware shows an
unconfirmed/unknown turnout state by blinking between the two colors, not
a true off (see "State Model" in the implementation doc).

## Turnout 1 example (GPIO 4)

```
                         ESP32 GPIO 4
                              │
              ┌───────────────┴───────────────┐
              │                                │
          [1kΩ]  (green branch)            (red branch, same row)
              │                                │
              ▼                          ▲│▼ LED — RED (thrown)
        ▲│▼ LED — GREEN (closed)         cathode → GPIO 4 node
        cathode → GND                    anode →
              │                                │
     blue (−) GND bus                      [1kΩ]
              │                                │
        → ESP32 GND                   red (+) 3.3V bus
                                              │
                                        → ESP32 3.3V
```

## Breadboard placement (corrected)

Terminal-strip columns/rows, 5×5 blocks A–E / F–J with the center gap
between them.

| Signal | From | Through | To |
|---|---|---|---|
| GPIO 4 signal wire | ESP32 GPIO 4 | — | column 3, row A |
| Resistor (green) | column 3, row B | 1kΩ, spans the row | column 8, row B |
| LED — green (closed) | column 8, row E (anode) | straddles center gap | column 8, row F (cathode) |
| Ground jumper | column 8, row J | — | blue (−) rail |
| LED — red (thrown) | column 3, row C (cathode) | spans the row, same column node as GPIO 4 | column 13, row C (anode) |
| Resistor (red) | column 13, row D | 1kΩ, spans the row | column 17, row D |
| 3.3V jumper | column 17, row E | — | red (+) rail |

The red LED's cathode leg lands in the same breadboard column as the
GPIO 4 wire (column 3) — that column's rows A–E are all one electrical
node, so no extra jumper is needed to tie them together. Likewise,
column 13's rows C and D are both part of the red LED's node, so moving
the resistor to row D keeps the same net without sharing a hole with the
LED's anode leg.

## Notes

- Use 680Ω–1kΩ per LED; 1kΩ is a safe starting point for a 3.3V GPIO
  into a standard LED — check your LED's datasheet and retune if it runs
  dim or you want it dimmer.
- Configure LED GPIOs as outputs and call `TurnoutIndicator::clear()` on
  every turnout before anything else at boot, so LEDs start in
  blink/unknown mode rather than an undefined level (see "Startup
  sequence" in the implementation doc).
- CLOSED → green on → GPIO HIGH. THROWN → red on → GPIO LOW. (Mapping can
  be reversed in software to match panel convention.)
- Green LED's cathode returns to the ESP32 GND bus; red LED's resistor
  returns to the ESP32 3.3V bus — both are shared rails, tie each back to
  the ESP32 once.

## Repeat per turnout

From the GPIO Assignment table in `docs/ESP32_Turnout_Panel_Implementation.md`:

| Turnout | GPIO | Turnout | GPIO |
|---|---|---|---|
| 1 (shown above) | 4 | 7 | 23 |
| 2 | 13 | 8 | 25 |
| 3 | 14 | 9 | 26 |
| 4 | 16 | 10 | 27 |
| 5 | 17 | 11 | 32 |
| 6 | 22 | 12 | 33 |

Button matrix wiring (for context, not covered by this LED-focused
diagram) is in `button-wiring.md`.
