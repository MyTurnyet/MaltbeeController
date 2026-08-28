# Turnout Panel PCB — Point-to-Point Wiring List

Derived from the BOM and pin map in `docs/pcb/turnout-panel-pcb-design.md`
(§4 Pin map, §6 Terminal block pinout). This document exists so the
schematic can be recreated net-by-net without re-deriving connections from
the narrative design doc. If the pin map or terminal pinout in the design
doc changes, regenerate this list from it rather than editing both by hand.

Format: `A → B → C` traces one net from the ESP32 pin to its terminal block
post (or shared rail).

---

## 1. Shared rails (build these once)

### Power input

- 5V DC supply `+` → 2-way pluggable header post 1 → DevKit `5V`/`VIN` pin
- 5V DC supply `−` → 2-way pluggable header post 2 → DevKit `GND` pin
- DevKit `5V` rail → 100 µF electrolytic capacitor `+` leg → (cap `−` leg → GND) — bulk decoupling
- DevKit `3.3V` rail → 100 nF ceramic capacitor → GND — decoupling

### Column pull-ups (4 total — one per column net, star-routed, not daisy-chained)

- 3.3V → 10 kΩ resistor → GPIO 34 → star to T1 post 1, T5 post 1, T9 post 1
- 3.3V → 10 kΩ resistor → GPIO 35 → star to T2 post 1, T6 post 1, T10 post 1
- 3.3V → 10 kΩ resistor → GPIO 36 → star to T3 post 1, T7 post 1, T11 post 1
- 3.3V → 10 kΩ resistor → GPIO 39 → star to T4 post 1, T8 post 1, T12 post 1

### Row buses (3 total — one per board edge, no resistor, direct GPIO)

- GPIO 18 → Top-edge bus → T1 post 2, T2 post 2, T3 post 2, T4 post 2
- GPIO 19 → Left-edge bus → T5 post 2, T6 post 2, T7 post 2, T8 post 2
- GPIO 21 → Right-edge bus → T9 post 2, T10 post 2, T11 post 2, T12 post 2

---

## 2. Per-turnout wiring pattern

Terminal block posts, left to right, per §6 of the design doc:
`1=SW(col) 2=SW(row) 3=G+ 4=G− 5=R+ 6=R−`

**Switch (posts 1–2, already covered above via row/column buses):**

- Column GPIO → post 1 → (off-board momentary switch, N.O.) → post 2 → Row GPIO

**Green LED branch:**

- LED GPIO → 1 kΩ resistor → post 3 (`G+`) → (off-board) green LED anode → green LED cathode → post 4 (`G−`) → GND

**Red LED branch:**

- 3.3V → 1 kΩ resistor → post 5 (`R+`) → (off-board) red LED anode → red LED cathode → post 6 (`R−`) → LED GPIO

### Per-turnout GPIO assignments

| Turnout | LED GPIO (posts 3/6) | Row GPIO (post 2) | Column GPIO (post 1) | Edge |
|---|---|---|---|---|
| T1  | GPIO 4  | GPIO 18 | GPIO 34 | Top |
| T2  | GPIO 13 | GPIO 18 | GPIO 35 | Top |
| T3  | GPIO 14 | GPIO 18 | GPIO 36 | Top |
| T4  | GPIO 16 | GPIO 18 | GPIO 39 | Top |
| T5  | GPIO 17 | GPIO 19 | GPIO 34 | Left |
| T6  | GPIO 22 | GPIO 19 | GPIO 35 | Left |
| T7  | GPIO 23 | GPIO 19 | GPIO 36 | Left |
| T8  | GPIO 25 | GPIO 19 | GPIO 39 | Left |
| T9  | GPIO 26 | GPIO 21 | GPIO 34 | Right |
| T10 | GPIO 27 | GPIO 21 | GPIO 35 | Right |
| T11 | GPIO 32 | GPIO 21 | GPIO 36 | Right |
| T12 | GPIO 33 | GPIO 21 | GPIO 39 | Right |

---

## 3. Worked example — T1 in full

- GPIO 4 → 1 kΩ resistor → T1 post 3 (`G+`) → green LED anode
- green LED cathode → T1 post 4 (`G−`) → GND
- 3.3V → 1 kΩ resistor → T1 post 5 (`R+`) → red LED anode
- red LED cathode → T1 post 6 (`R−`) → GPIO 4 *(same GPIO 4 as the green branch — one pin drives both LEDs)*
- GPIO 34 → 10 kΩ resistor → 3.3V *(shared with T5, T9)*
- GPIO 34 → T1 post 1 (`SW`) → switch → T1 post 2 (`SW`) → GPIO 18 *(row bus shared with T2, T3, T4)*

Every other turnout follows this identical pattern — substitute its row from
the table in §2.

---

## 4. Off-board parts (not on the PCB, per BOM footnote)

- 12 momentary switches — one per turnout, wired between post 1 and post 2 of that turnout's block
- 24 LEDs (12 green + 12 red) — wired anode → `+`, cathode → `−` per the branches above
- Panel hookup wire — builder's choice, connects into the 6-way mating plug per turnout, plus the 2-way mating plug for 5V

**Total GPIO count check:** 12 LED pins + 3 row pins + 4 column pins = 19 GPIOs, matching §1 of the design doc ("19 of the ESP32's available pins").
