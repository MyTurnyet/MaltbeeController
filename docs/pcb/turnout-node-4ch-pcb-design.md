# MaltBee 4-Turnout Node Board — Design Document

Design record for a family of identical ESP32 node boards, each driving **four**
turnout position indicators (red/green LEDs) and reading four momentary turnout
buttons directly from GPIO. Each node is its own MQTT client. A cluster of
turnouts (the 16-turnout yard, a crossover, a siding) is built from as many
boards as it needs.

This is the successor direction to the 12-turnout panel board in
`turnout-panel-pcb-design.md`. It captures the PCB-level decisions from the
October 2026 redesign discussion and the reasoning behind them. Statuses:
**Decided** = stated by Paige; **Proposed** = recommended in the discussion
and not yet confirmed; open items are in §11.

Related documents:
- `turnout-panel-pcb-design.md` — the 12-turnout matrix board this replaces for new nodes
- `esp32-turnout-node-rev-a/` — an 8-channel KiCad sketch already in the repo; this design is not derived from it (reconcile or retire it when the schematic starts, §11)
- `../ESP32_Turnout_Panel_Implementation.md`, `../led-wiring.md`, `../button-wiring.md`

---

## 1. Summary

| Property | Value |
|---|---|
| Turnouts per board | 4 |
| Board size | Target 80 × 70 mm; final size set by layout (§11) |
| Layers / thickness | 2 / 1.6 mm |
| Assembly | All through-hole, hand-soldered |
| Controller | ELEGOO ESP-WROOM-32 DevKit, **30-pin**, on female headers (S3 evaluated, not chosen) |
| Power | 5 V DC only, from a per-cluster LM2596 buck module fed by the separate 12 V bus; two parallel 2-way terminals; USB for flashing |
| Panel connection | 4 × shielded RJ45 (8P8C) jacks, one per turnout, no integrated magnetics |
| Buttons / LEDs | Always off-board, on the panel plate |
| GPIOs used | 17 of 19 safe pins; 2 spare. GPIO 0 is not on the 30-pin header (BOOT button only) |
| Configuration | Captive portal (unchanged) |

Scaling: 1–2 turnout spots populate only the jacks they need. The 16-turnout yard
is four identical boards, each a separate node with its own node ID.

---

## 2. Design decisions and rationale

### 2.1 Four turnouts per board, one board design — Decided

Four turnouts need 4 button lines and 8 LED lines. That is 12 GPIOs, which fit
directly on an ESP32, so the board needs no shift registers, no I/O expanders and
no button matrix. Four boards cover the yard with no stranded positions (two
12-turnout boards leave 8 unused). A 1–2 turnout location wastes at most a couple
of connectors.

An 8-turnout board was considered. It needs 24 lines and therefore two 74HC595
and one 74HC165, which would add 3.3 V/5 V logic-level concerns and a
shift-register firmware adapter. Four avoids all of it.

One board design was chosen over a small and a large variant, accepting some
overkill on the small nodes. The cost is more ESP32 modules (four for the yard
instead of two) and more nodes to commission through the portal.

### 2.2 Direct GPIO, no matrix — Decided

The four buttons go straight to the input-only pins GPIO 34/35/36/39, each with an
external pull-up (these pins have no internal pull-up). With no matrix there is no
ghosting, so the single-key ghost rule and column self-test from the 12-turnout
design are not needed on this board.

### 2.3 Two GPIOs per turnout, not one shared GPIO — Decided

The 12-turnout board drives a red and green LED from one GPIO (HIGH = green,
LOW = red), which cannot show "both off". This board gives each colour its own
GPIO, wired as two ordinary anodes with a shared ground. That makes off, red,
green and both-on (amber, e.g. for UNKNOWN/inconsistent) all possible.

Cost: 8 LED GPIOs instead of 4. With four turnouts per board this fits easily.

Firmware impact (§7): the shared-GPIO `LedPairDriver` exists only because a shared
pin cannot go dark; it blinks instead. The Mega target already drives each colour
from its own GPIO through the unmodified `TurnoutIndicator`/`Indicator` stack, and
`ToggleTurnoutStation` already takes the two colours as separate `DigitalOutput&`
ports, so the station layer does not change. The blink-when-unconfirmed behaviour
has to move into an adapter for independent outputs.

### 2.4 One RJ45 per turnout — Decided (pinout Proposed)

Shielded RJ45 jacks replace the 6-post pluggable blocks. Patch cables are cheap,
the latch is more secure than a screw terminal, and a four-conductor interface
(switch, red, green, ground) fits easily in 8 conductors.

**One jack per turnout, not two.** Two turnouts per jack would need a custom
splitter on every panel, and one bad cable or jack would take out two turnouts.
The panel end terminates in a cheap RJ45 screw-terminal breakout with labelled
terminals, so wire colour never matters. The spare conductors go unused.

Hazards to design around:
- **No integrated magnetics.** Ethernet "MagJack" jacks block DC, so button and
  LED signals would not pass. Use plain shielded 8P8C jacks.
- **Not Ethernet.** These jacks must not be connected to network switches or
  passive-PoE injectors (24 V/48 V on the spare pairs).
- **LocoNet RJ12 plugs.** The layout uses RJ12 (6P6C) for LocoNet. A 6P6C plug is
  narrower than an 8P8C jack and enters it, landing its six contacts on jack
  pins 2–7. Silkscreen the jacks `NOT ETHERNET / NOT LOCONET`, and mount them away
  from LocoNet jacks (§11 asks for a bench check).

### 2.5 Power: per-cluster buck, 5 V-only boards — Decided (protection chain Proposed)

The 12 V bus is a separate regulated supply, so no bridge rectifier is needed and
nodes keep power when track power is cut. A buck converter steps it to 5 V
upstream of the boards. Boards never see 12 V.

**Converter module** (supplied by Paige):

| Spec | Value |
|---|---|
| Model | LM2596 buck converter + voltmeter |
| Size | 66.04 × 35.05 mm |
| Input | 4.2–40 V DC |
| Output | 1.25–37 V adjustable; input must be at least 1 V above output |
| Current | 2 A normal and stable, 3 A max |
| Ripple | About 100 mV peak at about 150 kHz |
| Meter error | ±0.1 V |
| Note | Keep output under 2 A (15 W); heat sink for long continuous load |

Usage rules:
1. Set the output to 5.0 V with a separate meter before any board is connected, then lock the trim pot (nail polish or paint).
2. Fuse the module's 12 V feed (about 2 A). The listed specification gives no input protection.
3. Plan one module per cluster of boards, mounted within a foot or two of them. Initial limit is about 1.2 A per module; replace it with the measured figure (§11).
4. The 150 kHz switching noise is far from the 2.4 GHz radio. The risk is ripple on the 5 V rail, handled by the bulk capacitor on each board.

A single layout-wide 5 V bus was rejected: it outgrows one module as nodes are
added, one converter failure would take every panel down, and longer 5 V runs lose
more voltage than 12 V runs. Wire drop is not a concern at cluster scale: 18 AWG
solid is about 6.4 Ω per 1000 ft, so a 10 ft run out and back (20 ft) is about
0.13 Ω, or roughly 0.17 V at 1.3 A.

**Two 2-way input terminals, wired in parallel.** Either terminal accepts power, so
boards can be tapped individually from a bus strip or chained from board to board.
Both sit ahead of the board's protection, so each board's fuse protects only its
own load. The supply side of a chain still needs its own fuse sized to the wire.

**Per-board protection chain, in order:**

```
5V terminals ─ PTC fuse ─ Schottky (series) ─┬─ TVS ──┐
                                              ├─ bulk cap + 100 nF
                                              └─ dev kit 5V/VIN pin
```

| Part | Purpose |
|---|---|
| Radial PTC fuse | Limits a fault on this board to this board |
| Series Schottky | Reverse-polarity protection, and blocks USB 5 V from back-feeding the buck module's output |
| TVS across the rails, after the Schottky | Clamps spikes; if the buck's trim pot fails high, it clamps and the fuse trips |
| 470–1000 µF low-ESR bulk cap + 100 nF | Supplies ESP32 WiFi transmit bursts at the 5 V input |

**This revisits §2.6 of `turnout-panel-pcb-design.md`**, which rejected a series
Schottky because its ~0.4 V drop eats the onboard regulator's dropout headroom. It
is reintroduced here because nodes are fed from a shared bus, where a miswire is a
realistic event and USB reflashing of a live node needs back-feed isolation. The
cost is real: 5.0 V minus about 0.3–0.4 V leaves about 4.6 V at the dev kit's 5 V
pin. The 30-pin kit's regulator is an AMS1117-3.3 (marked on the vendor pinout
diagram), which needs about 1.1 V of headroom above 3.3 V, so the margin is only
about 0.2 V during WiFi bursts. Expect to trim the buck module up to about 5.2 V,
after confirming the kit's other 5 V parts (the diagram shows a CP2102 USB chip)
tolerate it (§11). The 3.3 V rail must be measured under load on the first board.

Keep the `5V DC ONLY — do not use with USB connected` silkscreen until the 30-pin
kit's USB-side behaviour is verified. The Schottky blocks back-feed into the buck
module; it does not show what the kit does toward a USB host.

### 2.6 Dev kit stays the 30-pin ELEGOO — Decided

This revision keeps the ELEGOO 30-pin dev kit already in use. The ESP32-S3
DevKitC-1 was evaluated (36 GPIOs exposed, no input-only pins, native USB) and not
chosen for this version. Because a later revision may be produced for others, the
kit stays on standard female headers so a future revision can swap it or move to a
soldered bare module without redoing the rest of the board.

Facts from the vendor pinout diagram of the 30-pin kit (15 pins per side):
- Left: EN, 36, 39, 34, 35, 32, 33, 25, 26, 27, 14, 12, 13, GND, VIN (5 V).
- Right: 23, 22, TX0 (1), RX0 (3), 21, 19, 18, 5, TX2 (17), RX2 (16), 4, 2, 15, GND, 3V3.
- **GPIO 0 and GPIO 6–11 are not on the header.** GPIO 0 is reachable only as the kit's BOOT button.
- On the kit: AMS1117-3.3 regulator, CP2102 USB-UART, micro-USB, with the RST and BOOT buttons on either side of the USB port.

The diagram is a vendor rendering, not a measurement; see §11.

### 2.7 Status LEDs and portal button on board, no repeater header — Decided

All indicators and the portal button stay on the board; nothing is repeated to a
panel. Two LEDs: **network status** (blue) and **activity** (amber: button press,
command received). Placing them with the USB port on one **service edge** keeps
everything touched during setup and troubleshooting in one place.

**Setup (portal) button — Decided: its own GPIO 16** (§3.4). Firmware already
triggers wireless setup from GPIO 0, the dev kit's BOOT button, held 3 s then
released. GPIO 0 is not on the 30-pin header, so a parallel button would mean
soldering to the kit's BOOT pads. It is also a strapping pin: a stuck or bumped
button at power-up would boot the node into download mode and it would look dead.
A separate GPIO has neither problem, costs one of the spare pins, and allows a
labelled SETUP button for other builders. The BOOT button keeps working as a
second trigger (§7).

### 2.8 Mounting — Decided

M3 holes in all four corners, about 4 mm in from each edge, with a 6 mm clear zone
for screw heads and standoffs. Holes are unplated and unconnected, so metal
standoffs cannot create a ground loop with the shield wiring (§5). Mounting
surfaces contain no metal. Still keep the area under and around the dev kit's
antenna free of copper pour, traces and tall parts, and let the module overhang
the board's copper where possible.

### 2.9 Reserved cable pair — Proposed

RJ45 pins 7/8 are not connected to any GPIO (spare pins are scarce). Each jack has
a 2-pin unpopulated pad next to it. If a per-turnout feature is added later, jumper
the pads to the I2C header (e.g. an MCP23017 expander). Anything beyond a button
and two LEDs per turnout can also be mapped through JMRI routes.

---

## 3. Circuit recap

### 3.1 Turnout LEDs (per colour)

```
GPIO ── 220 Ω ── RJ45 pin 1 (red) / pin 3 (green) ── LED anode → cathode ── RJ45 pin 2 / pin 6 ── GND
```

At 3.3 V with a standard 5 mm LED (Vf about 2.0–2.2 V), 220 Ω gives about 5–6 mA;
150 Ω gives about 8 mA. The 12-turnout board's 1 kΩ gave only 1.3–1.5 mA, which
its §8.1 notes is dim for standard LEDs. With one LED lit per turnout, four
turnouts draw about 20–25 mA in total. The resistors stay on the board so panel
plates carry no components, brightness is tuned in one place, and a cable shorted
to ground cannot overload a pin.

If bi-color LEDs are bought later: **three-lead common-cathode** type only
(common-anode would need a positive rail on the cable; two-lead types need
polarity reversal), standard green rather than "pure green" (about 3 V drop leaves
no headroom at 3.3 V), diffused lens. Red and green dies differ in brightness, so
tune each colour's resistor separately for the amber.

### 3.2 Buttons

```
3.3V ── 10 kΩ ──┬── GPIO (34/35/36/39)
                └── 1 kΩ ── RJ45 pin 4 ── switch ── RJ45 pin 5 ── GND
```

Idle reads 3.3 V. Pressed, the pin sits at 3.3 V × 1 k / 11 k ≈ 0.3 V, well below
the roughly 0.825 V logic-low threshold the 12-turnout doc uses. The 1 kΩ limits
current from a static discharge on the cable; debouncing is in software. No TVS or
RC filter is needed for runs under about a foot of shielded cable (a few feet at
most).

### 3.3 Status and power LEDs

Network status LED (GPIO 18) and activity LED (GPIO 17): 5 mm, 220 Ω each, upright
on the service edge. A power LED with a 1 kΩ resistor (about 3 mA) sits across the
5 V rail after the Schottky, independent of software.

### 3.4 Setup (portal) button

A 6 mm tactile switch from GPIO 16 to GND, using the internal pull-up (no external
parts). Firmware treats a 3 s hold then release as "enter wireless setup", the same
gesture as the dev kit's BOOT button (`ButtonSetupModeTrigger`, 3000 ms hold, 50 ms
release-settle window). Held or stuck at power-up it has no effect on boot, unlike
GPIO 0. Silkscreen it `SETUP — hold 3 s`. The kit's own BOOT button (GPIO 0, not on
the header) still works as a second trigger and for flashing.

### 3.5 I2C header

4-pin header: 3V3, GND, SDA (GPIO 21), SCL (GPIO 22). Footprints for 4.7 kΩ
pull-ups, unpopulated by default (most I2C breakouts carry their own).

---

## 4. Pin map

| Function | GPIO | Notes |
|---|---|---|
| Button 1–4 | 34, 35, 36, 39 | Input-only; external 10 kΩ pull-up each |
| T1 red / green | 32 / 33 | |
| T2 red / green | 25 / 26 | |
| T3 red / green | 27 / 13 | |
| T4 red / green | 23 / 19 | |
| Network status LED | 18 | |
| Activity LED | 17 | |
| I2C SDA / SCL | 21 / 22 | To header |
| Setup (portal) button | 16 | Internal pull-up; hold 3 s. The kit's BOOT button (GPIO 0, not on the header) is a second trigger |
| Spare | 14, 4 | 14 outputs a clock at boot; 4 has an internal pulldown at boot |

17 of the 19 safe pins are used (4 input-only plus 15 output-capable, minus
strapping, flash and serial pins), leaving the two spares above. Every pin in this
table is on the 30-pin kit's header, checked against the vendor pinout diagram.

### 4.1 Pins deliberately unused

| Pin(s) | Reason |
|---|---|
| GPIO 0 | Strapping pin (boot mode). Not on the 30-pin header; reachable only as the kit's BOOT button |
| GPIO 6–11 | Wired to SPI flash. Not exposed on the 30-pin kit's header |
| GPIO 12 (MTDI) | Strapping pin; sets flash voltage at boot |
| GPIO 2, 5, 15 | Strapping pins |
| GPIO 1, 3 (TX0/RX0) | USB serial console |

### 4.2 Default-peripheral pin conflicts

Any library that initialises a peripheral without explicit pin arguments will
collide with this map:

| Peripheral | Default pins | Conflicts with |
|---|---|---|
| I2C | 21 (SDA), 22 (SCL) | None — these are the I2C header |
| VSPI | 18, 19, 23, 5 | Network LED (18), T4 green (19), T4 red (23) |
| HSPI | 14, 12, 13, 15 | T3 green (13), spare (14) |
| UART2 | 16 (RX), 17 (TX) | Setup button (16), activity LED (17) |

Any `Wire.begin()`, `SPI.begin()` or `Serial2.begin()` call must pass explicit pins
on non-conflicting GPIOs, or not be used.

---

## 5. RJ45 pinout (T568B pair colours, straight-through cable)

| Pin | T568B colour | Signal | Pair |
|---|---|---|---|
| 1 | white/orange | Red LED anode drive | Red signal + ground |
| 2 | orange | GND | |
| 3 | white/green | Green LED anode drive | Green signal + ground |
| 6 | green | GND | |
| 4 | blue | Switch | Switch + ground |
| 5 | white/blue | Switch return (GND) | |
| 7 | white/brown | Reserved, not connected (pad, §2.9) | Spare pair |
| 8 | brown | Reserved, not connected | |

Each signal shares a twisted pair with its own ground, which is free noise
insurance at DC. All three grounds join on the board and on the panel plate. The
spare pair is genuinely spare.

**Shield:** connect the jack shell/shield to board ground at the node end only;
leave it unconnected at the panel end (unshielded or isolated breakout) so no ground
loop forms between a node and its panel. Test every cable with a tester or a
continuity check before use.

**Panel end:** RJ45 screw-terminal breakout, labelled. Both LED cathodes go to the
shared ground terminal; red anode to the red signal, green anode to the green
signal (long leg is the anode). The two LEDs are independent, so no polarity trick
is involved.

---

## 6. Board layout

```
              FIELD EDGE (cables exit upward)
  ┌─────────────────────────────────────────────────┐
  │ ○    ┌T1──┐  ┌T2──┐  ┌T3──┐  ┌T4──┐            ○ │
  │      │RJ45│  │RJ45│  │RJ45│  │RJ45│              │
  │      └────┘  └────┘  └────┘  └────┘              │
  │                                                   │
  │ ANT  ┌───── ESP32 DevKit (30-pin) ─────┐  ● ●    │
  │ end  │                                 ├─USB     │  SERVICE EDGE
  │ clear└─────────────────────────────────┘  [PRT]  │  (● status LEDs,
  │                                                   │   [PRT] portal button)
  │ ○ [5V][5V] [I2C]  TP TP TP TP   (PWR)           ○ │
  └─────────────────────────────────────────────────┘
```

Indicative only; final placement is a KiCad decision.

- **Field edge:** all four jacks on one edge so every cable exits the same way. Four shielded THT jacks side by side take roughly 65–70 mm.
- **Service edge:** USB port overhanging the board edge by 2–3 mm (a flush connector cannot be fully seated), with the status LEDs and portal button beside it. This is the edge to keep reachable once the board is mounted.
- **Antenna end:** opposite the USB end, pointing at an outer edge with no copper pour, traces or tall parts near it.
- **Power:** the two 5 V terminals on the remaining edge, power LED nearby.
- **Test points:** 5 V before the fuse, 5 V after the Schottky, 3V3, GND.
- **Silkscreen:** turnout numbers `T1`–`T4`; connector legend showing the §5 pinout; `5V DC ONLY — do not use with USB connected`; `NOT ETHERNET / NOT LOCONET`; board revision; the dev kit's measured row spacing next to its socket.

---

## 7. Firmware impacts (Proposed)

Grounded in the repo at the time of writing (`src/esp32/main.cpp`, `lib/McsEsp32/`).
**Status as of sub-project #6 (2026-10-22):** items 1 (sub-project #1), 2
(sub-project #4), 3 (sub-projects #2 and #4 together), 4's boot/setup half
(sub-project #5), and 6 (sub-project #3) are done; item 4's identify half
and item 5 remain open, both blocked on real WiFi/MQTT wiring landing on
the node board (sub-project #6 templated the Loco2MQTT classes that
wiring will use; the wiring itself is sub-project #7). See
`docs/superpowers/specs/2026-10-21-esp32-node4-channel-count-design.md`,
`docs/superpowers/specs/2026-10-21-esp32-node4-independent-led-pair-driver-design.md`,
`docs/superpowers/specs/2026-10-21-esp32-node4-setup-trigger-wiring-design.md`
and
`docs/superpowers/specs/2026-10-22-esp32-node4-turnout-station-wiring-design.md`,
plus `CLAUDE.md`'s "4-channel node board: channel-count templating",
"Independent-output turnout indicator", "Composite setup-mode trigger" and
"Turnout station wiring" sections, for the implementations.

1. **Turnout count — done (sub-project #1).** `NodeConfig::kChannelCount` is a compile-time `12`, `main.cpp` builds `TURNOUT_CONFIGS[12]`, 12 `LedPairStation`s and 12 `ToggleTurnoutStation`s, and the commissioning web form carries 12 channel fields. A 4-turnout node needs a per-board-variant constant (build flag or separate PlatformIO environment) driving all of these. *(Resolved via a channel-count template parameter on 8 commissioning classes plus a new `[env:esp32node4]` environment and `src/esp32node4/main.cpp` composition root — the latter is still a minimal placeholder with no turnout stations/LEDs/buttons wired, which items 2-5 below still need.)*
2. **Buttons.** Replace `MatrixScanner`/`MatrixDigitalInput` with four `ArduinoDigitalInput`s on GPIO 34/35/36/39 (active-low, no internal pull-up). The existing matrix column inputs use that same constructor form.
3. **LEDs.** Replace `LedPairStation` (shared GPIO) with two independent `ArduinoDigitalOutput`s per turnout and an adapter supplying the "unconfirmed → blink" behaviour, since "both off" is now physically possible. `ToggleTurnoutStation` is unchanged.
4. **Identify and setup-mode flashing.** `LedPairFlashIndicator` and the `panel/<nodeId>/identify` overlay both use `LedPairDriver::setIdentifying()`. They need equivalents for independent outputs.
5. **Status and activity LEDs.** New outputs, with no existing firmware owner. This is also where wireless-setup and WiFi/broker state get their own indicator instead of borrowing the turnout LEDs.
6. **Setup trigger — done (sub-project #3).** `ButtonSetupModeTrigger` takes any `DigitalInput&`. Construct it
   with a small composite input that is active when either the existing `bootButton`
   (GPIO 0) or a new `ArduinoDigitalInput(16, /*activeLow=*/true,
   /*useInternalPullup=*/true)` is active, so both buttons work. The hold and
   release-settle logic is unchanged. *(Resolved via a new `EitherActiveDigitalInput`
   adapter feeding the unmodified `ButtonSetupModeTrigger` — see `CLAUDE.md`'s
   "Composite setup-mode trigger" section.)*
7. **Unchanged:** `ButtonSetupModeTrigger`'s hold logic, Loco2MQTT per-channel addresses, `MqttPresenceAnnouncer`, collision detection. The MQTT client ID is already MAC-derived (`maltbee-esp32-<mac4>`), so several nodes on one broker do not collide. Each board still needs its own `nodeId`.

---

## 8. Bill of materials (per board)

| Qty | Item | Notes |
|---|---|---|
| 1 | PCB, about 80 × 70 mm, 2-layer, 1.6 mm | Under the 100 × 100 mm cheap tier |
| 1 | ELEGOO ESP-WROOM-32 DevKit, **30-pin** | Row spacing not yet measured (§11) |
| 2 | Female header, 1×15, 2.54 mm | DevKit socket |
| 4 | RJ45 8P8C shielded THT jack, **no magnetics** | Part and footprint TBD (§11) |
| 2 | Pluggable header, 2-way, 5.08 mm, right-angle | 5 V in, wired in parallel |
| 2 | Mating plug, 2-way, 5.08 mm | |
| 10 | Resistor 220 Ω, ¼ W | 8 turnout LEDs, 2 status LEDs |
| 1 | Resistor 1 kΩ, ¼ W | Power LED |
| 4 | Resistor 1 kΩ, ¼ W | Button series |
| 4 | Resistor 10 kΩ, ¼ W | Button pull-ups |
| 2 | Resistor 4.7 kΩ, ¼ W | I2C pull-ups, unpopulated by default |
| 1 | LED 5 mm blue | Network status |
| 1 | LED 5 mm amber | Activity |
| 1 | LED 5 mm, any colour | Power indicator |
| 1 | Tactile switch, 6 mm | Setup button, GPIO 16 to GND |
| 1 | Radial PTC fuse, 0.75 A hold | Initial value; size after measurement |
| 1 | Schottky 1N5819 (1 A), axial | Reverse polarity and back-feed block |
| 1 | TVS P6KE6.8A, axial, unidirectional | 5.8 V standoff; check datasheet before ordering |
| 1 | Electrolytic 470–1000 µF, 16 V, low-ESR, radial | Polarity-critical |
| 1 | Ceramic 100 nF | |
| 1 | Pin header 1×4 | I2C |
| 4 | Pin header 1×2 pads | Reserved pairs, unpopulated |
| 4 | Test points | 5 V pre-fuse, 5 V post-diode, 3V3, GND |
| 4 | M3 standoffs and screws | Mounting |

Per cluster (not on the board): LM2596 module with voltmeter (§2.5), inline 2 A
fuse on its 12 V feed, and a heat sink if run near 2 A continuously.

Per turnout (not on the board): shielded patch cable, RJ45 screw-terminal breakout
for the panel end, a switch, a red LED and a green LED.

---

## 9. Fabrication notes

- Two layers is sufficient. Nothing on the board is high-speed; keep the ground return solid under the dev kit and keep copper clear of the antenna end.
- Keep the board at or under 100 × 100 mm so it stays in the cheapest fab tier.
- Polarity-critical parts: the electrolytic, the Schottky, the unidirectional TVS and the LEDs. Everything else is resistors, headers, sockets and jacks.
- Order the pluggable plugs with the headers. Headers and plugs are sold separately, so a mismatched pitch or family is the likeliest parts error.
- Footprints and mounting holes: unplated, unconnected, M3.

---

## 10. Resolved decisions log

| Item | Resolution |
|---|---|
| Turnouts per board | **4**, one board design for every location (8 floated, not chosen) |
| Modularity | Separate nodes per cluster; no chaining between boards |
| Controller | ELEGOO 30-pin dev kit on sockets for this revision; S3 considered, not chosen |
| Power | 12 V bus (separate regulated supply) to per-cluster LM2596 to 5 V-only boards |
| Field wiring | RJ45, one jack per turnout; buttons and LEDs always wired off-board |
| Assembly | Through-hole, hand-soldered |
| LEDs | Standard 5 mm red and green, separate, one lit per turnout; bi-color deferred |
| Configuration | Existing captive portal retained |
| Status and setup button | On board (blue network, amber activity), no repeater header; setup button on its own GPIO 16, with the kit's BOOT button kept as a second trigger |
| 30-pin kit pinout | From the vendor diagram: GPIO 0 and 6–11 not on the header; AMS1117-3.3 regulator; CP2102 USB-UART; BOOT and RST beside the micro-USB port |
| Mounting | Corner M3 holes; no metal mounting surfaces |
| Boards per area | Multiple boards per cluster accepted |

## 11. Remaining verification and open items

- [ ] **Measure current draw** of the existing board (idle, all LEDs on, boot/WiFi association) with the incoming USB power meter; set boards per LM2596 module and the PTC fuse hold current from it.
- [ ] **Measure the 30-pin kit:** row spacing by breadboard test, as on the 38-pin kit. The vendor diagram is not dimensionally reliable. Also read the physical kit's silkscreen against §4, since the pin map was checked against that diagram only.
- [ ] **Measure the 3.3 V rail under load** on the first board with the Schottky in the path. The AMS1117-3.3 leaves about 0.2 V of margin at 4.6 V, so expect to trim the buck up to about 5.2 V. First confirm the kit's other 5 V parts (the CP2102) tolerate it, or use a lower-drop Schottky.
- [ ] **Check USB-side behaviour** of the kit (does a powered 5 V pin back-feed a USB host?) before dropping the "do not use with USB connected" silkscreen.
- [ ] **RJ45 part:** pick the jack, confirm it has no magnetics, confirm footprint and housing width, and confirm four fit in the 80 mm width.
- [ ] **RJ12 LocoNet plug in an RJ45 jack:** try it with parts in hand to see what the contacts hit (pins 2–7), then decide on labelling or keying.
- [ ] **TVS and fuse datasheets:** confirm P6KE6.8A standoff and clamping against the actual rail, and the PTC's hold/trip current.
- [ ] **Audit firmware** for `Wire.begin()`, `SPI.begin()` and `Serial2.begin()` calls without explicit pins (§4.2).
- [x] **Firmware work** (§7) **item 1** — per-variant channel count, done in sub-project #1 (2026-10-21).
- [x] **Firmware work** (§7) **item 6** — composite setup trigger (GPIO 0 or GPIO 16, 3 s hold-then-release), done in sub-project #3 (2026-10-21).
- [x] **Firmware work** (§7) **item 2** — four direct-GPIO `ArduinoDigitalInput` buttons on GPIO 34/35/36/39, wired into per-channel `ToggleTurnoutStation`s, done in sub-project #4 (2026-10-22).
- [x] **Firmware work** (§7) **item 3** — independent-LED adapter with "unconfirmed → blink" behaviour: `IndependentLedPairDriver`/`IndependentLedPairOutput` in sub-project #2 (2026-10-21), the GPIO-owning `IndependentLedPairStation` plus its real-GPIO wiring in sub-project #4 (2026-10-22). `ToggleTurnoutStation` needed no change, as predicted.
- [x] **Firmware work** (§7) **item 4, boot/setup-mode half** — `IndependentLedPairFlashIndicator` mirrors `LedPairFlashIndicator` for the node board's independent outputs, sub-project #5 (2026-10-22). The identify-overlay half of item 4, and item 5 (status/activity LEDs), remain open — identify is blocked on real WiFi/MQTT wiring landing on the node board (own future sub-project).
- [ ] **Reconcile** `esp32-turnout-node-rev-a/` with this design before starting the KiCad schematic.
