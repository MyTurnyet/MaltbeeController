# ESP32 JMRI Bridge Decommissioning (Sub-project #9c) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the now-orphaned JMRI-side MQTT bridge script and reconcile the two documentation files that still describe the retired JMRI-over-MQTT integration as the panel's current design.

**Architecture:** No `lib/`/`src/`/`test/` C++ code changes — this is a file deletion plus three documentation rewrites. Nothing here affects a build or test gate; verification is a final sanity check that both still succeed (trivially true, since nothing they compile changed) plus a grep sweep confirming no stale claims remain.

**Tech Stack:** Plain text/Markdown edits, `git rm`.

**Design doc:** `docs/superpowers/specs/2026-09-06-esp32-jmri-decommission-design.md` — read it for the full rationale; this plan is the executable breakdown of it.

## Global Constraints

- **No `lib/`, `src/`, or `test/` files are touched by this plan.** If a task ever seems to require a code change, stop and escalate — that would mean the spec's scope assessment was wrong.
- Do **not** touch `docs/superpowers/specs/`, `docs/superpowers/plans/`, `docs/Refactoring_Recommendations_Multi_Hardware.md`, or `internal_documents/` — all explicitly out of scope per the design doc (historical records, or unrelated to JMRI/Loco2MQTT).
- Within `docs/HARDWARE_BRINGUP_CHECKLIST.md`, only §2.1 and one phrase in §2.3 change — §2.2, §2.4, the rest of §2.5, §2.6, and all of Part 1 (Mega/LocoNet) are untouched.
- Within `docs/ESP32_Turnout_Panel_Implementation.md`, wiring tables, GPIO assignment tables, button matrix, LED wiring, the non-JMRI parts of the power section, physical assumptions, and "Suggested milestones" (explicitly headed "all shipped — kept for history") are untouched.
- Every commit in this repo goes through the **`arlo-commits` skill**, never a raw `git commit`.
- Run `pio test -e native` (and any filtered run) via **Bash/Git Bash, not PowerShell**, per this repo's CLAUDE.md.

---

### Task 1: Delete the JMRI bridge script and update `CLAUDE.md`

**Files:**
- Delete: `jmri/panel_mqtt_turnout_bridge.py`
- Modify: `CLAUDE.md`

**Interfaces:** None — this task has no code interfaces. It's the "the script is gone" fact plus the one documentation file that directly describes that fact.

- [ ] **Step 1: Delete the script and its now-empty directory**

```bash
git rm jmri/panel_mqtt_turnout_bridge.py
rmdir jmri 2>/dev/null || true
```

(The `rmdir` is a courtesy cleanup of the now-empty directory — `git rm` alone won't remove it, and an empty directory isn't tracked by git anyway, so this step is optional/idempotent; don't worry if it errors because the directory was already gone or non-empty for an unrelated reason.)

- [ ] **Step 2: Update `CLAUDE.md`'s `jmri/panel_mqtt_turnout_bridge.py` bullet**

Find this bullet in `CLAUDE.md` (in the `lib/McsEsp32/src/` file-layout list, it's a long paragraph currently prefixed "Orphaned as of sub-project #9a"):

```
- `jmri/panel_mqtt_turnout_bridge.py` — JMRI-side Jython startup script, outside the PlatformIO/C++ codebase entirely. **Orphaned as of sub-project #9a**: the ESP32 panel no longer talks to JMRI at all (see "Loco2MQTT turnout bridge" below) — nothing in `lib/`/`src/`/`test/` references this script's topics any more. Left in place, undeleted, pending its own removal in a future sub-project (#9c). The description below documents what it did for the panel's now-retired JMRI integration. It bridges the MQTT topics `JmriTurnoutCommandAdapter`/`JmriFeedbackSource` speak (`track/turnout/<jmriSystemName>` command, `track/turnout/<jmriSystemName>/state` state, `THROWN`/`CLOSED` payloads — see `TopicScheme`/`PayloadCodec`) directly to JMRI's real `Turnout` objects, with no shadow "MT" turnouts or Logix. It discovers which turnouts to bridge dynamically (every registered `LT`-prefixed turnout at script-run time), applies an incoming command via `setCommandedState()` and always publishes the resulting state back (even a no-op command, since that's exactly when a panel is out of sync and most needs telling), and separately republishes every discovered turnout's `KnownState` on any change regardless of cause (this panel, another panel, PanelPro, a dispatcher, physical feedback) via a `PropertyChangeListener` — this is what makes "LEDs reflect changes made elsewhere" work end to end. State publishes are **not retained**, so a panel that reconnects mid-session stays in blink/unconfirmed state for a turnout until that turnout's state next actually changes. See `docs/ESP32_Turnout_Panel_Implementation.md`'s "JMRI-side bridge script" section for installation steps.
```

Replace it with:

```
- `jmri/panel_mqtt_turnout_bridge.py` — **removed in sub-project #9c.** This was the JMRI-side Jython startup script that bridged MQTT to JMRI `Turnout` objects for the ESP32 panel's original JMRI-over-MQTT integration (`track/turnout/<jmriSystemName>` topics, no shadow "MT" turnouts or Logix). Orphaned by sub-project #9a's migration to Loco2MQTT (nothing in `lib/`/`src/`/`test/` referenced it any more) and deleted here. See `docs/ESP32_Turnout_Panel_Implementation.md`'s git history if you need to see what it did.
```

- [ ] **Step 3: Update the "Loco2MQTT turnout bridge (sub-project #9a)" section's phrasing**

Find:

```
`jmri/panel_mqtt_turnout_bridge.py` is now orphaned pending removal in a
future sub-project (#9c) — nothing in `lib/`/`src/`/`test/` references it
any more.
```

Replace with:

```
`jmri/panel_mqtt_turnout_bridge.py` was orphaned by this migration and
removed in sub-project #9c.
```

- [ ] **Step 4: Verify the script is gone and CLAUDE.md no longer references it as present**

```bash
ls jmri/ 2>&1
grep -n "panel_mqtt_turnout_bridge" CLAUDE.md
```

Expected: `ls` reports the directory doesn't exist (or is empty, harmless either way); `grep` shows exactly the two updated passages from Steps 2-3, both now describing the script in the past tense as removed — no line implies it still exists.

- [ ] **Step 5: Commit**

Commit via the `arlo-commits` skill (message should reflect: delete the orphaned JMRI bridge script and update `CLAUDE.md` to describe it as removed).

---

### Task 2: Rewrite `docs/ESP32_Turnout_Panel_Implementation.md`'s JMRI content

**Files:**
- Modify: `docs/ESP32_Turnout_Panel_Implementation.md`

**Interfaces:** None — pure documentation content. Consumes no other task's output; nothing depends on this task's output either (Task 3 is independent of this one, both only depend on Task 1 having already run so the "script removed" fact is consistent everywhere).

This is 12 separate find-and-replace edits (labeled A through L below) within the same file. Do them in order; each is independent of the others (none of the "find" text overlaps another edit's "find" text).

- [ ] **Step 1: Edit A — replace the "## Status" section**

Find the section starting at `## Status` and ending right before `## Relationship to the Mega/LocoNet system` (it's the first ~30 lines of the file: the Status heading, the "Implemented and merged" paragraph, the JMRI-side-integration paragraph, and the "Note on class names below" paragraph). Replace that entire span with:

```markdown
## Status

**Implemented and merged to `main`.** This document originally captured the
hardware design and software plan worked out in a ChatGPT conversation
(2026-07-21); since then all 6 "Suggested milestones" below have shipped —
matrix scanning, LED-pair driving, ESP32 hardware adapters, MQTT transport,
the `src/esp32/main.cpp` composition root, wireless commissioning,
multi-panel presence/collision detection, and identify-blink are all
programming-complete (see `CLAUDE.md` for the authoritative, currently-
maintained architecture description — this doc is kept for the hardware
design rationale and wiring tables, which haven't changed; its MQTT/topic
content was rewritten in sub-project #9c to match the current Loco2MQTT
design after sub-project #9a replaced JMRI). Only the physical hardware
bring-up pass remains — see `docs/HARDWARE_BRINGUP_CHECKLIST.md`.

The panel talks directly to Loco2MQTT, a companion device that bridges
LocoNet to MQTT via its own on-device broker — see "Loco2MQTT Communication
(MQTT)" below. The original design (JMRI over MQTT via an external broker
and a JMRI-side bridge script, `jmri/panel_mqtt_turnout_bridge.py`) was
replaced in sub-project #9a and that script was removed in sub-project #9c.

**Note on class names below:** the "Suggested per-turnout config" and
`MqttJmriTurnoutCommandAdapter`/`MqttJmriFeedbackSource`/`JmriCommandEncoder`/
`JmriFeedbackDecoder` names in this doc were the pre-implementation proposal.
The classes that actually shipped were named `JmriTurnoutCommandAdapter` and
`JmriFeedbackSource` (renamed to `Loco2MqttTurnoutCommandAdapter`/
`Loco2MqttFeedbackSource` in sub-project #9a), with the topic/payload
construction factored into standalone `TopicScheme` and `PayloadCodec` classes
(`lib/McsEsp32/src/domain/`) rather than separate encoder/decoder classes —
same native-testable-core-plus-thin-adapter shape as proposed, different
names/decomposition.
```

- [ ] **Step 2: Edit B — comparison table**

In "## Relationship to the Mega/LocoNet system", find:

```
| Turnout command transport | LocoNet (direct to DR5000) | Wi-Fi (MQTT) to JMRI |
```

Replace with:

```
| Turnout command transport | LocoNet (direct to DR5000) | Wi-Fi (MQTT) to Loco2MQTT |
```

Find:

```
| Feedback | LocoNet feedback (Milestone 10) | JMRI-confirmed state via MQTT (this doc) — LEDs reflect JMRI's reported state, not just what was commanded |
```

Replace with:

```
| Feedback | LocoNet feedback (Milestone 10) | Loco2MQTT-confirmed state via MQTT (this doc) — LEDs reflect Loco2MQTT's reported state, not just what was commanded |
```

- [ ] **Step 3: Edit C — "## Project Goal" section**

Find:

```
- An MQTT command sent to JMRI when the button is pressed
- LEDs that show JMRI's **confirmed** state for that turnout — updated only once
  JMRI publishes the resulting state back over MQTT (not optimistically on
  button press), and updated the same way when the turnout changes for any
  other reason (another panel, PanelPro, a dispatcher) — see "JMRI
  Communication (MQTT)" below
```

Replace with:

```
- An MQTT command sent to Loco2MQTT when the button is pressed
- LEDs that show Loco2MQTT's **confirmed** state for that turnout — updated
  only once Loco2MQTT publishes the resulting state back over MQTT (not
  optimistically on button press), and updated the same way when the
  turnout changes for any other reason (another panel, another controller,
  physical feedback) — see "Loco2MQTT Communication (MQTT)" below
```

- [ ] **Step 4: Edit D — controller hardware bullet**

Find:

```
- Wi-Fi used continuously to talk to JMRI
```

Replace with:

```
- Wi-Fi used continuously to talk to Loco2MQTT
```

- [ ] **Step 5: Edit E — power section**

Find:

```
- This ESP32 does **not** power turnout motors/decoders — it only handles
  button input, LED indication, and Wi-Fi/JMRI communication.
```

Replace with:

```
- This ESP32 does **not** power turnout motors/decoders — it only handles
  button input, LED indication, and Wi-Fi/Loco2MQTT communication.
```

- [ ] **Step 6: Edit F — Wireless Setup Access Point section (two edits)**

Find:

```
open the AP on its own; without the BOOT-button hold it boots waiting for
bench-serial commissioning instead (see "JMRI Communication (MQTT)" and
the bench-serial commissioning design for that path).
```

Replace with:

```
open the AP on its own; without the BOOT-button hold it boots waiting for
bench-serial commissioning instead (see "Loco2MQTT Communication (MQTT)"
and the bench-serial commissioning design for that path).
```

Find:

```
The panel's own current WiFi password is never displayed by the setup
form — a blank password field on submission keeps the existing password
unchanged. Turnout JMRI name fields work the opposite way: a blank field on
submission clears that turnout's assigned name.
```

Replace with:

```
The panel's own current WiFi password is never displayed by the setup
form — a blank password field on submission keeps the existing password
unchanged. Turnout address fields work the opposite way: a blank field on
submission clears that turnout's assigned address.
```

- [ ] **Step 7: Edit G — Identifying a Physical Panel section**

Find:

```
to make that one physical panel's LEDs flash so you can find it among
several deployed panels — useful when verifying commissioning worked,
debugging via JMRI/MQTT tooling, or telling apart two panels mid-collision
```

Replace with:

```
to make that one physical panel's LEDs flash so you can find it among
several deployed panels — useful when verifying commissioning worked,
debugging via MQTT tooling, or telling apart two panels mid-collision
```

- [ ] **Step 8: Edit H — replace the entire "## JMRI Communication (MQTT)" section**

Find the section starting at `## JMRI Communication (MQTT)` and ending right before `## Software responsibilities` (this spans the section intro, the adapter-class bullet list, "### Data flow", "### Connection loss and reconnection", and "### JMRI-side bridge script"). Replace that entire span with:

```markdown
## Loco2MQTT Communication (MQTT)

**Decision (2026-07-21, migrated to Loco2MQTT 2026-09-06 — sub-project
#9a):** the ESP32 talks directly to Loco2MQTT — a companion ESP32 device
that bridges the layout's LocoNet bus to MQTT via its own on-device broker
— both to send turnout commands and to receive turnout state-change
notifications. This replaced an earlier design where the panel talked to
JMRI over MQTT via an external broker and a JMRI-side bridge script; that
path is fully retired (`jmri/panel_mqtt_turnout_bridge.py` was removed in
sub-project #9c). MQTT itself was chosen over JMRI's WebSocket JSON server,
the WiThrottle/simple TCP protocol, and HTTP polling because it gives a
persistent, push-based connection for feedback (no polling) with a
comparatively simple client library on the ESP32 side — that reasoning
carried over unchanged to the Loco2MQTT design.

This reuses the existing Mega/LocoNet application layer almost unchanged —
`TurnoutCommandPort`, `TurnoutControl`, and `TurnoutFeedback`
(`lib/McsCore/src/ports/TurnoutCommandPort.h`,
`lib/McsCore/src/application/TurnoutControl.h`) are already transport-agnostic.
The ESP32-specific work is adapters that plug into those same interfaces,
mirroring the pattern already used for LocoNet on the Mega side
(`MrrwaLocoNetTurnoutAdapter` / `LocoNetFeedbackDecoder`):

- **`Loco2MqttTurnoutCommandAdapter`** (`lib/McsEsp32/src/adapters/`)
  implements `TurnoutCommandPort::send()`. Looks up the turnout channel's
  configured LocoNet address from `NodeConfig::channelTurnoutAddresses` and
  publishes the command over MQTT. Topic construction is factored into a
  pure, native-testable `TopicScheme` class (`lib/McsEsp32/src/domain/`) so
  only the actual MQTT `publish()` call is untestable off-hardware.
- **`Loco2MqttFeedbackSource`** — a poll-shaped port matching
  `LocoNetFeedbackSource::poll()`. The MQTT client library's subscribe
  callback pushes incoming `{topic, payload}` messages into a small queue;
  `poll()` drains one per call, non-blocking.
- **`PayloadCodec`** (`lib/McsEsp32/src/domain/`) — native-testable,
  encodes/decodes the `CLOSED`/`THROWN` payload words.
- **12× `TurnoutControl`**, unmodified, one per turnout, each wired to its
  button pair, LED-pair indicator, and the shared
  `Loco2MqttTurnoutCommandAdapter`.

### Data flow

**Command (panel → Loco2MQTT):** button press → `TurnoutControl::update()`
(unmodified) → `turnoutCommandPort_.send(address, position)` →
`Loco2MqttTurnoutCommandAdapter` publishes to `loconet/turnout/<address>/set`.
`update()` never touches the indicator, so the LED does not change at this
point.

**Feedback (Loco2MQTT → panel):** Loco2MQTT publishes a turnout's state on
`loconet/turnout/<address>/state` — whether that's because of this panel's
command or because it changed for any other reason on the layout (another
panel, a physical throw with feedback) — and re-publishes every known
turnout's state unconditionally every 30 seconds as its own workaround for
not honoring the MQTT retained flag. The ESP32 (subscribed to each
configured turnout's own state topic) queues the message via
`Loco2MqttFeedbackSource`; each `loop()` the composition root drains the
queue and calls the matching turnout's `TurnoutControl::applyFeedback(feedback)`
— which is the only place `indicator_.display()` gets called. A repeated
identical feedback message (from the 30-second re-publish) is a harmless
no-op here.

### Connection loss and reconnection

Unchanged from the original design: `TurnoutIndicator::clear()` is the hook
for "no confirmed state," and the ESP32 LED-pair driver treats "both off"
as blink mode. Startup, disconnect, and reconnect all behave exactly as
described below in "Startup sequence" — none of that logic depended on
which broker/backend was on the other end.

**State publishes are not retained** — this is now inherent to Loco2MQTT's
broker, which ignores the retained flag entirely (rather than a deliberate
choice on the JMRI-bridge side, as it was originally). A panel that
reconnects mid-session stays in blink/unconfirmed state for a given turnout
until that turnout's state next actually changes, or until the next
30-second unconditional re-publish arrives (see "Data flow" above) —
whichever comes first.

### Broker discovery

Loco2MQTT advertises itself via mDNS as `loco2mqtt.local`. The panel
resolves this once at boot (`EspMdnsResolver`/`BrokerAddressResolver`,
`lib/McsEsp32/src/`), falling back to a manually-configured broker
host/port if mDNS resolution fails. See `CLAUDE.md`'s "Loco2MQTT turnout
bridge (sub-project #9a)" section for the full contract and known
limitations (no periodic re-resolution after boot, etc.).
```

- [ ] **Step 9: Edit I — "## Software responsibilities" numbered list (two edits)**

Find:

```
2. Connect to / communicate with the JMRI server.
```

Replace with:

```
2. Connect to / communicate with Loco2MQTT.
```

Find:

```
8. Send the turnout command to JMRI over MQTT.
9. Subscribe to JMRI's turnout state topic(s) and decode incoming feedback.
10. Store/retrieve the last JMRI-confirmed turnout state (not the commanded
    state — see "JMRI Communication (MQTT)" above).
11. Drive the LED GPIO: HIGH for green, LOW for red, only once JMRI confirms
    the state — never optimistically on button press.
```

Replace with:

```
8. Send the turnout command to Loco2MQTT over MQTT.
9. Subscribe to Loco2MQTT's turnout state topic(s) and decode incoming feedback.
10. Store/retrieve the last Loco2MQTT-confirmed turnout state (not the commanded
    state — see "Loco2MQTT Communication (MQTT)" above).
11. Drive the LED GPIO: HIGH for green, LOW for red, only once Loco2MQTT confirms
    the state — never optimistically on button press.
```

- [ ] **Step 10: Edit J — "### Suggested per-turnout config" struct and its explanation**

Find the code block starting `struct TurnoutConfig {` through the `TurnoutConfig turnouts[] = { ... };` closing, plus the explanatory paragraph right after it (starting "`jmriSystemName` values above are placeholders..."). Replace the whole thing (code block plus paragraph) with:

````markdown
```cpp
struct TurnoutConfig {
    int turnoutNumber;
    int matrixRow;
    int matrixColumn;
    int ledGpio;
    int locoNetAddress;
};

TurnoutConfig turnouts[] = {
    {1, 0, 0, 4,  5},
    {2, 0, 1, 13, 6},
    {3, 0, 2, 14, 7},
    {4, 0, 3, 16, 8},
    {5, 1, 0, 17, 9},
    {6, 1, 1, 22, 10},
    {7, 1, 2, 23, 11},
    {8, 1, 3, 25, 12},
    {9, 2, 0, 26, 13},
    {10, 2, 1, 27, 14},
    {11, 2, 2, 32, 15},
    {12, 2, 3, 33, 16}
};
```

`locoNetAddress` values above are placeholders — must be set to match the
real layout's LocoNet turnout addresses before deployment. `locoNetAddress`
is also the value substituted into the MQTT command topic
(`loconet/turnout/<address>/set`) and state topic
(`loconet/turnout/<address>/state`) for that turnout — no separate topic
fields are needed in this struct.
````

- [ ] **Step 11: Edit K — "### Startup sequence"**

Find:

```
5. Connect to JMRI over MQTT; subscribe to turnout state topic(s).
6. Wait for JMRI to publish confirmed state for each turnout (each arrival
   calls that turnout's `TurnoutControl::applyFeedback()`, which stops that
   LED's blinking and shows the confirmed color) — turnouts JMRI hasn't
   reported yet keep blinking.
```

Replace with:

```
5. Connect to Loco2MQTT over MQTT (resolved via mDNS, falling back to a
   manually-configured broker host); subscribe to turnout state topic(s).
6. Wait for Loco2MQTT to publish confirmed state for each turnout (each
   arrival calls that turnout's `TurnoutControl::applyFeedback()`, which
   stops that LED's blinking and shows the confirmed color) — turnouts not
   yet reported keep blinking.
```

- [ ] **Step 12: Edit L — "## Open questions / follow-ups" checklist**

Find the entire checklist (every `- [x]` line under this heading, through the end of the last item, right before `## Suggested milestones`). Replace it with:

```markdown
- [x] Turnout MQTT communication protocol/transport — decided 2026-07-21:
      MQTT; migrated 2026-09-06 (sub-project #9a) from an external broker +
      JMRI-side bridge script to Loco2MQTT's on-device broker (see
      "Loco2MQTT Communication (MQTT)" above).
- [x] `UNKNOWN`-state handling — decided 2026-07-21: blink last-known/default
      color (see State Model above). Unaffected by the #9a migration.
- [x] Exact MQTT topic structure and payload format — implemented as
      `loconet/turnout/<address>/set` (command) and
      `loconet/turnout/<address>/state` (state), payload `"THROWN"` or
      `"CLOSED"` in both directions (unchanged from the original design's
      payload words — only the topic prefix and the value keying the
      address changed). See `TopicScheme`/`PayloadCodec`
      (`lib/McsEsp32/src/domain/`).
- [x] Whether turnout state is published as retained — resolved above under
      "Connection loss and reconnection": on-change plus an unconditional
      30-second re-publish, never retained (Loco2MQTT's broker ignores the
      retained flag entirely).
- [x] MQTT broker — as of sub-project #9a, the broker is Loco2MQTT itself
      (a companion ESP32 device with its own on-device broker), discovered
      via mDNS (`loco2mqtt.local`) with a manual-host fallback. No longer a
      separate Mosquitto-style broker + JMRI.
- [x] Real LocoNet turnout addresses for each channel — assigned during
      commissioning (`turnout N address <address>`), not hardcoded in
      `main.cpp` — same pattern the original JMRI-name assignment used,
      just an integer instead of a system-name string.
- [x] Verify GPIO 4 boot behavior on the actual ELEGOO board — confirmed
      2026-09-02: turnout 12 wired and working (throws/closes correctly,
      LEDs correct), and the board has been power-cycled multiple times
      with no stray flash/glitch on turnout 12's LED pair during boot.
      (This verification predates the #9a migration and was against the
      original JMRI setup — see `docs/HARDWARE_BRINGUP_CHECKLIST.md` for
      the current bring-up status.)
- [x] Confirm ESP32 board power behavior before ruling out external 5V/VIN —
      confirmed 2026-09-02: this board runs exclusively off 5V/VIN with no
      USB connected (and none intended for permanent deployment), working
      correctly.
```

- [ ] **Step 13: Verify no stale JMRI-as-current-design claims remain outside the untouched sections**

```bash
grep -n "JMRI" docs/ESP32_Turnout_Panel_Implementation.md
```

Expected: every remaining hit is inside the "Note on class names below" paragraph (Edit A — describing the *original pre-implementation proposal's* class names, correctly past-tense/historical), the "Suggested milestones" section (explicitly headed "all shipped — kept for history," untouched per Global Constraints), or otherwise clearly historical/past-tense phrasing (e.g. "the original JMRI setup"). No hit should describe JMRI as the panel's current live integration.

- [ ] **Step 14: Commit**

Commit via the `arlo-commits` skill (message should reflect: rewrite `docs/ESP32_Turnout_Panel_Implementation.md`'s JMRI Communication section, comparison table, and other current-state prose to describe Loco2MQTT).

---

### Task 3: Rewrite `docs/HARDWARE_BRINGUP_CHECKLIST.md` and final verification

**Files:**
- Modify: `docs/HARDWARE_BRINGUP_CHECKLIST.md`

**Interfaces:** None. Depends only on Task 1 (the script must already be gone for the "verification predates the migration" framing in Step 2 below to be accurate) — does not depend on Task 2.

- [ ] **Step 1: Edit A — replace §2.1 Prerequisites**

Find the entire "### 2.1 Prerequisites" section (its heading through the last bullet, right before "### 2.2 Flash, boot, and bench-serial commissioning"):

```markdown
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
```

Replace with:

```markdown
### 2.1 Prerequisites

- A Loco2MQTT device (see its own project documentation) running and
  reachable from the ESP32 panel's WiFi network — either discoverable via
  mDNS as `loco2mqtt.local`, or its IP/port known for manual configuration
  as a fallback.
- LocoNet turnout addresses decided for at least a few channels (the panel
  doesn't require all 12 to be configured to boot — partial commissioning
  is explicitly supported).
- The ELEGOO ESP32 board wired per `docs/ESP32_Turnout_Panel_Implementation.md`'s
  GPIO Assignment section (3×4 button matrix, 12 LED pairs) — wire
  however many turnouts you're bringing up. The wireless-setup gesture
  (2.4) uses the board's own onboard BOOT button, not any turnout wiring,
  so no turnout needs to be wired first just to exercise it.
```

- [ ] **Step 2: Edit B — §2.3 Matrix buttons, LEDs, and turnout control**

Find:

```
For each turnout channel you've wired and named in commissioning:

- Press its button. LED should update once JMRI confirms the state (not
  optimistically on press) — expect a brief delay, not instant.
- Confirm the LED blinks (unconfirmed/disconnected state) if you
  temporarily stop the MQTT broker, and resumes normal display once it's
  back.
```

Replace with:

```
For each turnout channel you've wired and addressed in commissioning:

- Press its button. LED should update once Loco2MQTT confirms the state
  (not optimistically on press) — expect a brief delay, not instant.
- Confirm the LED blinks (unconfirmed/disconnected state) if you
  temporarily stop Loco2MQTT, and resumes normal display once it's back.
```

- [ ] **Step 3: Edit C — Part 2's "Status as of 2026-09-02" note**

Find:

```
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
```

Replace with:

```
**Status as of 2026-09-02** (predates sub-project #9a's JMRI→Loco2MQTT
migration — this verification was against the original JMRI setup and has
not been re-run against Loco2MQTT; §2.1/§2.2/§2.3 below now describe the
current Loco2MQTT-based procedure, not what was actually exercised on
this date): two real ESP32 boards were built and commissioned against a
live MQTT broker/JMRI. Confirmed working: 2.1 (broker running, real JMRI
names assigned), 2.2 (flash/boot/commissioning), 2.3 (button → LED → JMRI
turnout control, including turnout 12/GPIO4 — wired, working, and
power-cycled multiple times with no boot-time LED glitch), and 2.4 (the
BOOT-button wireless setup gesture, including joining the now-open setup
AP and submitting the form). One board also ran entirely off 5V/VIN with
no USB connected, confirming the external-power open question. **Still
outstanding:** 2.5 (presence/collision — feasible now with two boards,
just not yet exercised) and 2.6 (identify-blink), plus **re-verifying
2.1-2.3 against the current Loco2MQTT procedure**, which has not been
tested on real hardware yet.
```

- [ ] **Step 4: Verify this file's remaining JMRI mentions are all correctly historical**

```bash
grep -n "JMRI" docs/HARDWARE_BRINGUP_CHECKLIST.md
```

Expected: only hits inside the Step 3 status note (describing what was verified *at the time*, now explicitly caveated as predating the migration) — nothing describing JMRI as the panel's current setup instructions.

- [ ] **Step 5: Final sanity check across the whole plan**

Run both build gates — trivially unaffected since no `lib/`/`src/`/`test/` file changed, but confirm nothing was accidentally broken:

```bash
pio test -e native
pio run -e esp32dev
```

Expected: `pio test -e native` shows all suites `[PASSED]`, 0 `[FAILED]`/`[ERRORED]` (same count as before this plan — this plan adds/removes no tests). `pio run -e esp32dev` shows `BUILD SUCCESS` with the same RAM/Flash percentages as before this plan (56800/327680 bytes RAM, 1098881/1310720 bytes Flash, i.e. 17.3%/83.8%) — if either percentage changed, something unexpected happened, since no compiled source changed.

Then run a final grep sweep across all four files this plan touched, confirming nothing was missed:

```bash
grep -rn "JMRI\|jmri" jmri/ CLAUDE.md docs/ESP32_Turnout_Panel_Implementation.md docs/HARDWARE_BRINGUP_CHECKLIST.md 2>/dev/null
```

Expected: `jmri/` reports no such file or directory (or is empty). Every remaining hit in the three doc files is one already accounted for and judged correctly-historical in Task 1 Step 4, Task 2 Step 13, and Task 3 Step 4 above.

- [ ] **Step 6: Commit**

Commit via the `arlo-commits` skill (message should reflect: rewrite `docs/HARDWARE_BRINGUP_CHECKLIST.md`'s Loco2MQTT-affected sections, caveat the pre-migration hardware verification status note).

---

## Out of scope for this plan

Matches the design doc's "Out of scope" section: `internal_documents/`, `docs/superpowers/specs/` and `docs/superpowers/plans/` (historical records), `docs/Refactoring_Recommendations_Multi_Hardware.md` (pre-ESP32-work historical planning doc), and re-running physical hardware verification against the current Loco2MQTT procedure (noted as still-outstanding in Task 3's updated status note, not performed here).
