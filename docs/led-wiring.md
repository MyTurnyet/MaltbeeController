# Indicator LED Wiring

Reference for wiring the thrown/closed panel LEDs to the Mega 2560. Each
turnout station drives two LEDs — **thrown** and **closed** — from a pair of
adjacent digital pins, active-high
(`ArduinoDigitalOutput(pin, activeLow=false)` in `TurnoutStation.cpp`), through
a series resistor straight to a shared ground bus. No external supply rail is
needed — the Mega pin itself sources the LED current.

An interactive breadboard diagram for Station 1 was built as a Claude
artifact; this file is the durable, offline copy of that reference.

## Circuit (per LED)

```
Mega Dx ──/\/\/\── 220Ω ──►|── LED (anode → cathode) ── GND
          resistor          flat edge / short leg = cathode
```

## Station 1 example (Turnout 101)

```
 Mega D8 ──wire──┐                    Mega D9 ──wire──┐
                  │                                    │
              [220Ω]  (red-red-brown-gold)         [220Ω]  (red-red-brown-gold)
                  │                                    │
                  ▼                                    ▼
                 ▲│▼  LED1 — red, THROWN              ▲│▼  LED2 — green, CLOSED
                  │  (flat edge = cathode)              │  (flat edge = cathode)
                  │                                    │
                  └──────────────┬─────────────────────┘
                                  │
                         blue (−) GND bus
                                  │
                            → Mega GND
```

Breadboard placement used in the diagram (terminal-strip columns/rows,
5×5 blocks A–E / F–J with the center gap between them):

| Signal            | From          | Through                      | To                     |
|--------------------|---------------|-------------------------------|-------------------------|
| Pin 8 signal wire   | Mega D8       | —                              | column 3, row A          |
| Resistor (thrown)   | column 3, row B | 220Ω, spans the row           | column 7, row B          |
| LED1 (thrown, red)  | column 7, row E (anode) | straddles center gap  | column 7, row F (cathode) |
| Ground jumper       | column 7, row J | —                              | blue (−) rail             |
| Pin 9 signal wire   | Mega D9       | —                              | column 11, row A          |
| Resistor (closed)   | column 11, row B | 220Ω, spans the row          | column 15, row B          |
| LED2 (closed, green)| column 15, row E (anode) | straddles center gap | column 15, row F (cathode) |
| Ground jumper       | column 15, row J | —                             | blue (−) rail             |

## Notes

- 220Ω is a safe default for a 5V logic pin into a standard 20mA LED — check
  your LED's datasheet forward voltage/current and retune if it runs dim or
  you want it dimmer.
- LED flat edge / shorter leg is the cathode — it's the leg that goes to
  ground, not to the resistor.
- Because outputs are active-high, `pinMode(OUTPUT)` + `digitalWrite(HIGH)`
  lights the LED; `begin()` drives the pin LOW at startup, so both LEDs power
  up off.
- Tie the breadboard's blue (−) bus back to a Mega `GND` pin once — both LED
  cathodes on a station share that same bus.
- LED colors (red = thrown, green = closed) follow standard railroad signal
  convention, not a hardware requirement — substitute whatever LEDs you have
  on hand as long as thrown/closed stay visually distinct.

## Repeat per station

From `STATION_CONFIGS` in `src/mega/main.cpp`:

| Station | Address | Thrown LED pin | Closed LED pin |
|---------|---------|-----------------|------------------|
| 1       | 101     | D8               | D9                |
| 2       | 102     | D10              | D11               |
| 3       | 103     | D12              | D13               |
| 4       | 104     | D14              | D15               |

Throw/close button pins (for context, not covered by this LED-focused
diagram): station 1 = D22/D23, station 2 = D24/D25, station 3 = D26/D27,
station 4 = D28/D29.
