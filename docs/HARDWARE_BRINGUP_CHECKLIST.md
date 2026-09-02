# Hardware Bring-Up Checklist

Both firmware targets (Mega 2560 / LocoNet panel, ESP32 / MQTT panel) are
programming-complete and merged to `main`, verified via `pio test -e
native` and both hardware build environments. Nothing below requires
writing or changing code — it's the physical verification pass that's
been deferred since each milestone/sub-project was built. Work through
whichever panel you have hardware for; the two are independent.

This doc doesn't repeat pin tables that already live elsewhere — it
points to them and tells you what to *do* with them, in what order, and
what a working result looks like. If you hit something these docs don't
explain, or a step behaves differently than described, that's real
signal — come back and we'll fix the code or the docs, not just the wiring.

---

## Part 1: Mega 2560 / LocoNet Panel

**Reference:** `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`
(Milestones 8-11) for what's being verified; `src/mega/main.cpp` for the
exact pin table (`STATION_CONFIGS[0..3]`).

### 1.1 Prerequisites

- A Digitrax DR5000 (or equivalent LocoNet command station) powered up
  and on the LocoNet bus.
- A DR4018 (or equivalent stationary decoder) configured for at least one
  turnout address on that same LocoNet bus, wired to a real or simulated
  turnout motor/point (a pair of LEDs across the DR4018's outputs is
  enough to see it switch, if you don't have a motor wired yet).
- The Mega 2560 connected via USB, LocoNet interface wired to the bus.
  **This project has not yet nailed down the exact LocoNet physical
  interface pin** — `LocoNet.init()` in `src/mega/main.cpp:47` is called
  with no argument, so it uses the MRRWA LocoNet library's own default
  pin/timer configuration for the Mega 2560. Check the [MRRWA LocoNet
  library's own documentation](https://github.com/mrrwa/LocoNet) for
  which pin that is on a Mega before wiring the physical interface
  (opto-isolator or direct DR5000 connection) — don't guess from another
  AVR board's pinout, since the default can differ by chip.

### 1.2 Flash and boot

```bash
pio run -e megaatmega2560 --target upload
pio device monitor
```

At 115200 baud, expect no crash/reboot loop. This firmware currently has
no serial startup banner (unlike the ESP32 side) — silence after upload
is the expected "it's running" signal. If you want a heartbeat while
debugging, temporarily add one; don't leave debug prints in committed code.

### 1.3 Per-station wiring and verification (4 stations)

Wire **at least one station** end-to-end before moving to the rest — it's
the fastest way to catch a wiring mistake before repeating it four times.
Station pin assignments are in `src/mega/main.cpp`'s `STATION_CONFIGS`
table (throw button / close button / thrown LED / closed LED, one row per
station).

For each station, in order:

1. **Wire the buttons and LEDs only** (skip LocoNet for this first pass).
   With `NullTurnoutCommandPort` no longer wired in (real LocoNet has
   replaced it as of Milestone 9), a button press will still attempt a
   real LocoNet send — that's fine, it just won't do anything useful yet
   without the bus connected. Confirm:
   - Pressing the throw button lights the thrown LED, close lights closed.
   - Only one LED is ever lit at a time.
2. **Connect the LocoNet interface** (per 1.1 above) and confirm on the
   DR5000's own display/software that the Mega is present on the bus
   (LocoNet is a shared bus — the DR5000 doesn't need to "see" a specific
   node the way an addressed device would, but a monitoring tool like
   LocoBuffer/JMRI's LocoNet monitor can show traffic).
3. **Confirm the send path:** press a button, watch for a `requestSwitch`
   LocoNet packet (via a LocoNet monitor, or watch the DR4018's own
   output LEDs if it's configured for that station's address). The pulse
   is timed by `PulsingLocoNetTransport` (`LOCONET_PULSE_DURATION_MS =
   250` in `src/mega/main.cpp` — an untuned placeholder; if the DR4018
   doesn't reliably latch, this is the first thing to adjust).
4. **Confirm the feedback path:** operate that same turnout from a
   *different* controller (JMRI, a throttle, or directly toggling the
   DR4018) and confirm the panel's LED updates to match — this proves
   `LocoNetFeedbackDecoder`/`applyFeedback()` are wired correctly, not
   just the send side.
5. Repeat for the remaining 3 stations. Once all 4 work independently,
   operate two at once (in any combination) and confirm neither
   interferes with the other — this exercises `TurnoutControl`'s
   per-address self-filtering.

### 1.4 Done when

- All 4 stations: button → correct LED, independently and reliably.
- All 4 stations: an external turnout change → correct LED update.
- No station's button/feedback affects another station's LED.

---

## Part 2: ESP32 / MQTT Panel

**Reference:** `docs/ESP32_Turnout_Panel_Implementation.md` for GPIO
tables, wiring diagrams, and the MQTT topic scheme; `src/esp32/main.cpp`
for the exact composition. This panel has substantially more surface area
than the Mega side (wireless commissioning, presence, collision
detection, identify-blink all layered on top of basic turnout control) —
work through the sections below roughly in order, since later ones
assume earlier ones already work.

**Status as of 2026-09-02:** two real ESP32 boards are built and
commissioned against a live MQTT broker/JMRI. Confirmed working: 2.1
(broker running, real JMRI names assigned), 2.2 (flash/boot/commissioning),
2.3 (button → LED → JMRI turnout control, including turnout 12/GPIO4 —
wired, working, and power-cycled multiple times with no boot-time LED
glitch), and 2.4 (the BOOT-button wireless setup gesture, including
joining the now-open setup AP and submitting the form). One board also
runs entirely off 5V/VIN with no USB connected, confirming the
external-power open question. **Still outstanding:** 2.5
(presence/collision — feasible now with two boards, just not yet
exercised) and 2.6 (identify-blink).

### 2.1 Prerequisites

- An MQTT broker reachable from the ESP32's WiFi network (Mosquitto or
  similar), with JMRI's MQTT connection configured against it (MQTT Channel
  left blank in JMRI's connection preferences).
- `jmri/panel_mqtt_turnout_bridge.py` installed in JMRI as a Startup script
  (Edit → Preferences → Startup → Add → "Jython script"), listed *above*
  any panel file that creates the LocoNet turnouts it bridges — it
  discovers turnouts to bridge at script-run time by scanning for every
  registered `LT`-prefixed turnout, so they need to already exist by then.
  Restart JMRI after adding it; the System Console should log `"Panel <->
  MQTT <-> turnout bridge active for N turnouts"` on startup.
- JMRI turnout system names decided for at least a few channels (the
  panel doesn't require all 12 to be configured to boot — partial
  commissioning is explicitly supported).
- The ELEGOO ESP32 board wired per `docs/ESP32_Turnout_Panel_Implementation.md`'s
  GPIO Assignment section (3×4 button matrix, 12 LED pairs) — wire
  however many turnouts you're bringing up. The wireless-setup gesture
  (2.4) uses the board's own onboard BOOT button, not any turnout wiring,
  so no turnout needs to be wired first just to exercise it.

### 2.2 Flash, boot, and bench-serial commissioning

```bash
pio run -e esp32dev --target upload
pio device monitor
```

At 115200 baud, expect one of two startup lines: `"MaltBee panel ready
(configured)."` or `"MaltBee panel needs commissioning. Type 'show'."` —
a factory-fresh board should show the latter. Commission it over serial:

```
id 5
wifi <your-ssid> <your-password>
broker <broker-host-or-ip> 1883
turnout 1 name LT1
show
save
```

`save` validates and persists to NVS; `reboot` (or a manual reset) applies
it. After reboot, confirm the panel connects — watch for MQTT activity on
your broker (e.g. `mosquitto_sub -t 'panel/#' -v` shows `panel/5/status`
go retained-`online`, per sub-project #2d-a).

**If commissioning behaves unexpectedly:** the exact command syntax and
every response string are in `lib/McsEsp32/src/application/CommissioningSession.cpp`
and covered by `test/test_commissioning_session/` — check there before
assuming the hardware is at fault.

### 2.3 Matrix buttons, LEDs, and turnout control

For each turnout channel you've wired and named in commissioning:

- Press its button. LED should update once JMRI confirms the state (not
  optimistically on press) — expect a brief delay, not instant.
- Confirm the LED blinks (unconfirmed/disconnected state) if you
  temporarily stop the MQTT broker, and resumes normal display once it's
  back.
- Confirm two different turnouts' buttons don't cross-trigger each other.

### 2.4 Wireless setup (captive portal)

This is the escape hatch a technician uses without a serial cable —
worth verifying even though you *do* have serial access right now, since
it's the only path once panels are deployed.

1. Hold the ESP32 board's **BOOT button for 3 seconds**, then release.
   Serial should log `"Entering wireless setup..."` and the panel reboots.
2. Once it reboots into wireless setup mode, confirm every LED pair
   you've wired flashes green/red in unison — this is the same fast
   flash MQTT identify-blink uses (`docs/ESP32_Turnout_Panel_Implementation.md`'s
   "Identifying a Physical Panel" section), reused here since the two
   states can never overlap.
3. On a phone or laptop, look for a WiFi network named `MaltBee-Setup-XXXX`
   (last 4 hex digits of the chip's MAC) and join it — no password
   required (documented in `docs/ESP32_Turnout_Panel_Implementation.md`'s
   "Wireless Setup Access Point" section).
4. A captive-portal prompt should appear automatically (or navigate to
   any HTTP address — all DNS is redirected). Confirm the form pre-fills
   already-commissioned values (node ID, SSID, channel names) — but never
   the real WiFi password, which should always show blank.
5. Submit a change (e.g. a new channel name) and confirm the panel reboots
   and applies it.
6. Confirm normal turnout operation and buttons are completely unaffected
   by being in this mode — while the AP is up, turnout control is
   intentionally suspended (see `BootMode::WirelessSetup` in
   `src/esp32/main.cpp`); confirm it resumes normally after the reboot in
   step 5, and confirm the LED flash from step 2 stops at the same time.

**If wireless setup ever triggers without anyone pressing BOOT** while
`pio device monitor` is open, check the USB-to-serial chip's DTR/RTS
auto-reset wiring to GPIO0 first — a steady monitor session shouldn't
hold the pin low for 3 seconds, but that circuit is the first place to
look before suspecting a firmware bug.

### 2.5 Presence and collision detection (requires two panels)

Skip this section if you only have one ESP32 board — it needs two panels
to exercise. Reference: `CLAUDE.md`'s "Presence + collision detection"
section.

1. With one panel commissioned and running, subscribe to `panel/<nodeId>/mac`
   on your broker and confirm it holds a retained value (the panel's own
   MAC, last 4 hex digits).
2. Commission a **second** panel with the **same nodeId** as the first
   (deliberately — this is testing the collision path). Boot it.
3. Within a few seconds, confirm via serial monitor on *both* panels: the
   log line `"NodeId collision detected: this panel is <mac>, another
   panel claiming this node id is <mac>"` — each should report the
   other's MAC, not its own. This mutual-detection behavior is a
   consequence of the MQTT broker kicking whichever panel connects second
   (duplicate `clientId`) and each panel re-announcing on every reconnect
   — see `CLAUDE.md` if it doesn't happen as described.
4. Confirm all 12 buttons and turnout feedback stop responding on
   whichever panel(s) show the collision — this is expected suppression,
   not a hang.
5. Recommission one panel with a different nodeId and reboot. Confirm
   both panels return to normal operation.

### 2.6 Identify-blink (requires access to a broker CLI/tool)

```bash
mosquitto_pub -t panel/5/identify -n
```

(`-n` sends an empty, non-retained message — see `docs/ESP32_Turnout_Panel_Implementation.md`'s
"Identifying a Physical Panel" section for why non-retained matters.)

Confirm the target panel's 12 LEDs flash green/red in unison for 10
seconds, then automatically return to normal display. Confirm buttons and
turnout feedback are completely unaffected while it's flashing. Re-publish
partway through the window and confirm it extends the flash rather than
restarting a second, independent one.

### 2.7 Done when

- At least a few turnout channels: button → confirmed LED, matching JMRI.
- Wireless setup (2.4) completes a full commission-and-apply cycle.
- If you have two boards: collision detection (2.5) fires and clears
  correctly.
- Identify-blink (2.6) is visually obvious and auto-stops.

---

## Cross-cutting notes

- **Serial monitor, not print debugging, is the primary diagnostic tool
  for both panels** — every state transition this project cares about
  (boot mode, commissioning responses, collision detection, wireless
  setup) already logs a line. If something's silent where you'd expect a
  log line, that's itself informative.
- **Nothing in this checklist should require a code change to pass.** If
  a step genuinely can't be made to work as described, that's a real bug
  or a real design gap — not something to work around by editing the
  firmware to match what actually happened. Bring it back.
- Flash headroom on the ESP32 build is currently at 81.2% (per the
  #2d-b final review) — routes and persistent-configuration milestones
  are still ahead for this project's Mega side, and it's worth keeping an
  eye on this number if the ESP32 side grows further too.
