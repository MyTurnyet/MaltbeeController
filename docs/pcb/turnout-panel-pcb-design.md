# Turnout Panel Controller PCB — Design Document

Design record for a single-board ESP32 controller that drives 12 turnout
position indicators (red/green LED pairs) and reads 12 momentary turnout
buttons via a scanned matrix. Derived from `docs/led-wiring.md` and
`docs/button-wiring.md`; this document captures the PCB-level decisions
and the reasoning behind them.

---

## 1. Summary

| Property | Value |
|---|---|
| Board size | 160 × 160 mm |
| Layers | 2 |
| Thickness | 1.6 mm |
| Assembly | All through-hole, hand-soldered |
| Turnouts supported | 12 |
| Power | 5 V DC via pluggable terminal (USB for flashing) |
| GPIOs used | 19 of the ESP32's available pins |
| Controller | ELEGOO ESP-WROOM-32 DevKit, 38-pin, **1.0" (25.4 mm) row spacing** |
| Panel connection | 12 × six-way pluggable right-angle terminal blocks |

---

## 2. Design decisions and rationale

Decisions are recorded with their reasoning so future revisions don't
re-litigate settled questions or accidentally undo a deliberate choice.

### 2.1 Single board, not per-turnout modules

An earlier direction placed a small PCB at each turnout, daisy-chained or
star-wired back to a hub. That topology earns its keep **only when the
switch and LEDs are soldered onto the module**, because then the module
*is* the panel component and the hub-to-module cable is the only run.

Once the decision was made to connect buttons and LEDs via screw terminals
(for component swappability), the module stopped reducing anything: five
long wires would run hub-to-module, then five short wires module-to-parts.
Same long-run count as wiring the parts directly to a single board, plus 12
extra PCBs and 24 extra connectors.

**Conclusion:** one board. Each turnout needs three unique signals from the
ESP32 (LED GPIO, matrix row, matrix column) plus two shared rails. No
passive local circuitry can compress that, so the wire count is fixed
regardless of topology.

> The only thing that *would* reduce wiring is active local circuitry —
> a shift register or I²C expander per module, reducing the bus to ~4
> wires. This was considered and rejected: it requires a firmware rewrite
> and abandons the single-GPIO bidirectional LED drive the project is
> built around.

### 2.2 Six terminals per turnout, not five

Because the red LED is wired in reverse (cathode returns to the GPIO),
it is *possible* to share one post between green's anode and red's cathode,
reducing the block from six posts to five and saving one wire per turnout.

This was rejected. With six posts, each LED gets its own clearly-labelled
`+` and `−`, and the board handles the polarity reversal internally. The
person wiring the panel connects both LEDs the same obvious way — long leg
to `+`. The five-post version requires them to understand *why* two
different LED legs share a screw, which is an error waiting to happen.

Twelve saved wires is not worth that.

### 2.3 No matrix diodes

Ghosting in a scanned matrix requires **three** switches pressed at three
corners of a rectangle; the scanner then reads a phantom fourth press at
the remaining corner. One or two simultaneous presses always read
correctly.

Diodes would prevent this in hardware, but each diode drops voltage in the
switch path. At the ~330 µA a 10 kΩ pull-up allows, a 1N4148 sits near
0.5 V. The ESP32 reads logic low below roughly 0.825 V, so the noise
margin would shrink from ~825 mV to ~325 mV — a meaningful reduction on a
panel with long, interference-prone wire runs.

**Conclusion:** no diodes. Ghosting is handled in firmware (§7.1) by a rule
that is both simpler and stricter than rectangle detection.

### 2.4 All through-hole

With the diodes gone, the only surface-mount candidates were 24 identical
resistors on a single board. Assembly setup fees dominate at that scale
(~$10–15 more than hand-soldering), and machine assembly would add a BOM
match, a pick-and-place file, and component rotation verification — all
first-order sources of error.

Hand-soldering 24 more resistors alongside the 12 terminal blocks, 38
header pins, and 4 pull-ups already on the list adds perhaps 30 minutes.
The board also stays fully repairable with an iron.

### 2.5 Pluggable right-angle terminal blocks

Fixed screw blocks were the original assumption. Pluggable blocks were
chosen instead because **72 wires** land on this board (12 turnouts × 6
posts). With fixed blocks, removing the board means loosening all 72 screws
and then getting all 72 back in the correct holes.

Pluggable blocks split into a header soldered to the PCB and a removable
plug holding the wires. The harness is wired once — comfortably, at the
bench, with the plugs in hand — then pushed on. Removing the board becomes
unplugging twelve connectors.

**Right-angle** rather than straight: the plug inserts horizontally from
the board edge, so wires exit sideways rather than needing vertical
clearance above a board mounted flat behind a panel.

Generic equivalents (Degson, Ningbo Kangnex, etc.) share the 5.08 mm pitch
and footprint at a fraction of genuine Phoenix Contact prices, and are
entirely adequate for switching 3.3 V at a few milliamps.

### 2.6 5 V terminal as primary power

The board is powered from a regulated 5 V supply through a 2-way pluggable
terminal, matching the connector family used everywhere else. USB remains
available for flashing.

Rationale: DevKit USB connectors are surface-mount and lift their pads when
the cable is tugged — the most likely hardware failure on a permanently
installed panel. A screw terminal is mechanically robust and allows the
supply to live wherever is convenient.

A series Schottky on the input was considered and rejected: it drops ~0.4 V,
consuming most of the AMS1117's dropout headroom for a benefit a silkscreen
warning already provides.

### 2.7 Star-routed columns

Each column net serves three turnouts. Columns use GPIO 34/35/36/39, which
are **input-only and have no internal pull-up** — so if the external pull-up
path fails there is no software fallback.

Columns are therefore routed as a star from the pull-up point to each of
the three terminal blocks, rather than daisy-chained block to block. A
broken trace then costs one turnout instead of everything downstream.

Redundant parallel pull-ups were considered and rejected as solving a
failure mode that doesn't realistically occur on an inspectable soldered
joint.

---

## 3. Circuit recap

### 3.1 LED pair (per turnout, one GPIO drives both colours)

```
GPIO ── 1kΩ ── green anode → cathode ── GND

3.3V ── 1kΩ ── red anode → cathode ── GPIO
```

| GPIO level | Green | Red |
|---|---|---|
| HIGH (~3.3 V) | On | Off |
| LOW (~0 V) | Off | On |

- **HIGH:** both ends of the red branch sit near 3.3 V, so no current flows
  through red. Current flows GPIO → resistor → green → GND.
- **LOW:** both ends of the green branch sit near 0 V, so green is off.
  Current flows 3.3 V → resistor → red → GPIO (sinking).

Both LEDs off is not achievable. Unknown/unconfirmed turnout state is shown
by blinking between colours.

### 3.2 Button matrix

```
Row GPIO ──┤ SW ├── Column GPIO ── 10kΩ ── 3.3V
            momentary, N.O.
```

Rows are driven low one at a time; columns are read. Columns are held high
by external pull-ups because GPIO 34/35/36/39 have none internally.

12 buttons arranged 3 rows × 4 columns.

---

## 4. Pin map

| Turnout | LED GPIO | Row (GPIO) | Column (GPIO) | Board edge |
|---|---|---|---|---|
| T1 | 4 | 1 (18) | 1 (34) | Top |
| T2 | 13 | 1 (18) | 2 (35) | Top |
| T3 | 14 | 1 (18) | 3 (36) | Top |
| T4 | 16 | 1 (18) | 4 (39) | Top |
| T5 | 17 | 2 (19) | 1 (34) | Left |
| T6 | 22 | 2 (19) | 2 (35) | Left |
| T7 | 23 | 2 (19) | 3 (36) | Left |
| T8 | 25 | 2 (19) | 4 (39) | Left |
| T9 | 26 | 3 (21) | 1 (34) | Right |
| T10 | 27 | 3 (21) | 2 (35) | Right |
| T11 | 32 | 3 (21) | 3 (36) | Right |
| T12 | 33 | 3 (21) | 4 (39) | Right |

LED GPIO assignments are unchanged from `ESP32_Turnout_Panel_Implementation.md`.
Row 3 on GPIO 21 is the only new pin. **Confirmed unused in current firmware.**

Four turnouts per edge and four turnouts per matrix row means **each board
edge is one matrix row**, so row traces are short buses running along each
edge. Only the four column nets cross the board interior.

### 4.1 Pins deliberately unused

| Pin(s) | Reason |
|---|---|
| GPIO 6–11 (SD0–SD3, CMD, CLK) | Wired to SPI flash. Connecting anything here prevents boot. Mark on silkscreen. |
| GPIO 12 (MTDI) | Strapping pin; sets flash voltage at boot. Wrong level can brick the board. |
| GPIO 0, 2, 5, 15 | Strapping pins; avoid to keep boot behaviour predictable. |
| GPIO 1, 3 (TX0/RX0) | USB serial console. |

### 4.2 Default-peripheral pin conflicts

The ESP32's default peripheral pins overlap this design. Any library that
initialises a peripheral without explicit pin arguments will conflict:

| Peripheral | Default pins | Conflicts with |
|---|---|---|
| I²C | 21 (SDA), 22 (SCL) | Row 3, and T6's LED |
| VSPI | 18, 19, 23, 5 | Row 1, Row 2, and T7's LED |

Any `Wire.begin()` or `SPI.begin()` call must pass explicit pins on
non-conflicting GPIOs, or not be used at all.

**Known cosmetic behaviour:** GPIO 14 outputs a clock signal during boot and
GPIO 4 has an internal pulldown, so T1 and T3 will flash briefly at power-up.
The startup `clear()` call resolves this.

---

## 5. Board layout

```
        ┌─ T1 ──┬─ T2 ──┬─ T3 ──┬─ T4 ──┐
        │                                │
      ┌─┤                                ├─┐
      T5│                                │T9
      ├─┤                                ├─┤
      T6│        ESP32 DevKit            │T10
      ├─┤        (USB faces down)        ├─┤
      T7│                                │T11
      ├─┤                                ├─┤
      T8│                                │T12
      └─┤                                ├─┘
        │   [5V]      ▓▓▓ USB ▓▓▓        │
        └────────────────────────────────┘
              free edge — keep clear
```

- **Terminal blocks:** 4 per edge on three edges. 5.08 mm pitch pluggable
  right-angle headers, roughly 33 mm wide including housing. Four per edge
  is ~132 mm; on a 160 mm edge with 5 mm margins that leaves ~6 mm between
  blocks.
- **Plug protrusion:** right-angle plugs insert horizontally and extend
  15–20 mm **beyond the board outline** on three sides. Any enclosure must
  be ~40 mm larger than the PCB in both directions.
- **5 V input:** 2-way pluggable right-angle header on the free edge,
  beside the USB overhang.
- **Free edge:** no blocks. The DevKit is positioned so its USB connector
  **overhangs the board edge by 2–3 mm**. USB cables have chunky overmolded
  plugs; a flush-mounted connector cannot be fully seated.
- **Pull-ups:** placed adjacent to the DevKit's column pins (34/35/36/39,
  which cluster together on one side of the module), then star-routed out.
- **Mounting:** 4 × M3 holes at corners, inset ~5 mm. Add mid-edge standoffs
  if the board flexes when tightening terminal screws.
- **Strain relief:** two small holes ~10 mm behind the USB connector for a
  zip tie. DevKit USB connectors are surface-mount and lift pads when the
  cable is tugged.

---

## 6. Terminal block pinout and silkscreen

Each block, left to right:

| Post | Silkscreen | Net |
|---|---|---|
| 1 | `SW` | Column (34/35/36/39 per §4) |
| 2 | `SW` | Row (18/19/21 per §4) |
| 3 | `G +` | Green anode, via 1 kΩ from LED GPIO |
| 4 | `G −` | GND |
| 5 | `R +` | Red anode, via 1 kΩ from 3.3 V |
| 6 | `R −` | LED GPIO |

Silkscreen also carries:
- Turnout number (`T1` … `T12`) above each block
- Grouping brackets labelled `SWITCH`, `GREEN LED`, `RED LED`
- A single legend near the terminal field:
  - *LED long leg (anode) → + post*
  - *LED short leg (flat side) → − post*
  - *Both LEDs wire the same way — board handles polarity*

Switch posts are both labelled `SW` because a momentary switch has no
polarity; distinguishing them would imply a constraint that doesn't exist.

Additional silkscreen elsewhere on the board:
- Beside the 5 V terminal: **`5V DC ONLY — do not use with USB connected`**
- Beside the DevKit socket: a mark on the GPIO 6–11 positions warning that
  they are SPI flash and must stay unconnected
- Row spacing note at the socket: `25.4mm (1.0")`

### 6.1 Footprint

Six pads at 5.08 mm pitch, right-angle pluggable header. The mating plug is
purchased separately and is not a PCB component — see BOM.

---

## 7. Firmware requirements

### 7.1 Ghost rejection

Ghosting requires three genuine presses to manufacture a fourth. With one
key down the reading is always correct; with two keys down it is also
always correct.

**Rule: accept a scan only when exactly one key reads as pressed. Otherwise
discard the scan.**

A turnout panel never legitimately needs two simultaneous presses, so
nothing is lost, and phantoms become structurally impossible rather than
filtered out. This is a pure function from scan result to key event.

### 7.2 Column self-test at boot

With no rows driven, every column should read high. Any column reading low
at startup indicates a short or a failed pull-up.

This matters because of an unintuitive symptom: a column shorted to ground
reads as pressed on every row, so the single-key rule discards every scan
and the **entire panel** appears dead. The self-test turns that into a
named fault.

### 7.3 Existing behaviour retained

- Configure LED GPIOs as outputs; call `TurnoutIndicator::clear()` on every
  turnout at boot before anything else.
- CLOSED → green → GPIO HIGH. THROWN → red → GPIO LOW.
- Unknown state → blink between colours.

---

## 8. Bill of materials

| Qty | Item | Notes |
|---|---|---|
| 1 | PCB, 160 × 160 mm, 2-layer, 1.6 mm | ~$20–30 for 5 at typical fabs |
| 1 | ELEGOO ESP-WROOM-32 DevKit, 38-pin | **1.0" (25.4 mm) row spacing — verified** |
| 2 | Female header, 1×19, 2.54 mm | DevKit socket |
| 12 | Pluggable header, 6-way, 5.08 mm, right-angle | Soldered to PCB |
| 12 | Mating plug, 6-way, 5.08 mm | Holds panel wiring; not a PCB part |
| 24 | Resistor, 1 kΩ, ¼ W | LED current limiting |
| 4 | Resistor, 10 kΩ, ¼ W | Column pull-ups |
| 1 | Capacitor, 100 µF electrolytic, 16 V | Bulk decoupling |
| 1 | Capacitor, 100 nF ceramic | Decoupling |
| 1 | Pluggable header, 2-way, 5.08 mm, right-angle | 5 V input |
| 1 | Mating plug, 2-way, 5.08 mm | 5 V supply lead |
| 1 | Regulated 5 V DC supply | Sized for ~300 mA minimum |
| 4 | M3 standoffs and screws | Mounting |

Not on the board: 12 momentary switches, 24 LEDs, and panel wire — chosen
freely by the builder, which was the point of the terminal blocks.

### 8.1 LED brightness note

At 1 kΩ from 3.3 V the LED current is roughly 1.3–1.5 mA. That is fine for
high-brightness LEDs but **dim for standard ones** in a lit room. If the
panel is hard to read, drop to 470 Ω (~3 mA). Total load even then is well
within the DevKit's regulator: only 12 LEDs are lit at once.

---

## 9. Fabrication notes

- Two layers is sufficient. Only the four column nets cross the interior;
  row buses run along their edges and LED traces fan out individually.
- Boards over 100 × 100 mm leave the cheapest fab tier. Since no arrangement
  of twelve six-post blocks fits under that, size for buildability rather
  than squeezing.
- Shipping frequently exceeds board cost. Order enough spares to make one
  shipment worthwhile — 5 is the usual minimum and costs little more than 2.
- Order the pluggable **plugs** at the same time as the headers, and buy a
  few spares. Mismatched pitch or an incompatible plug family is the most
  likely parts error, since headers and plugs are sold separately.
- Nothing on this board is polarity-critical at fabrication time except the
  electrolytic capacitor (C1). Everything else is resistors, headers, and
  sockets.

---

## 10. Resolved decisions log

| Item | Resolution |
|---|---|
| DevKit row spacing | **25.4 mm (1.0")**, confirmed by breadboard test — pins land in rows A and I, which is exactly 1.0" apart (0.4" across A–E, 0.3" centre channel, 0.3" F–I). The product listing claiming 22.86 mm was wrong. |
| Terminal block type | Pluggable, right-angle, 5.08 mm, 6-way. Generic equivalents acceptable. |
| 5 V terminal | Populated; primary power source. USB retained for flashing only. |
| GPIO 21 for row 3 | Confirmed unused in current firmware. |

## 11. Remaining verification before ordering

- [ ] Confirm the exact housing width of the specific pluggable header
      purchased. Design assumes ~33 mm for a 6-way; if the part runs wider,
      the ~6 mm inter-block gap absorbs a few millimetres but not more.
- [ ] Verify KiCad footprint names resolve in your library version. The
      netlist's *connections* are correct regardless; only footprint
      references may need substituting.
- [ ] Read the DevKit's pin labels against §4 before routing. The pin map
      assumes the standard 38-pin DevKitC order, which the row-spacing
      error shows is worth confirming independently.
- [ ] Audit firmware for `Wire.begin()` / `SPI.begin()` without explicit
      pins (see §4.2).
