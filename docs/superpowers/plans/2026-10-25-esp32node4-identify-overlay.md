# ESP32 Node4 Identify-Overlay Wiring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the identify-overlay (MQTT-triggered LED flash used to physically locate a board) onto the 4-channel node board, mirroring the 12-channel panel's existing, unmodified mechanism exactly.

**Architecture:** Pure composition-root wiring in `src/esp32node4/main.cpp` — one new global (`IdentifyModeTimer`), one new MQTT subscription in `setup()`, and one new `loop()` block that drives `IndependentLedPairStation::setIdentifying()` on all 4 stations. No library code changes; `IdentifyModeTimer`, `PresenceTopics::identifyTopic()`, and `IndependentLedPairStation::setIdentifying()` all already exist and are reused unmodified.

**Tech Stack:** C++17, PlatformIO (`esp32node4` environment), Catch2 (native suite, unaffected by this change).

## Global Constraints

- No new library code. `IdentifyModeTimer` (`lib/McsEsp32/src/domain/IdentifyModeTimer.h`), `PresenceTopics::identifyTopic()` (`lib/McsEsp32/src/domain/PresenceTopics.h`), and `IndependentLedPairStation::setIdentifying(bool)` (`lib/McsEsp32/src/adapters/IndependentLedPairStation.h`) are all pre-existing and must not be modified.
- `main.cpp` has no native test target (`test_build_src = false` in `platformio.ini`) — no new tests are possible or expected for this change, consistent with every prior `main.cpp`-only sub-project (#3, #4, #5, #7, #8).
- `IDENTIFY_DURATION_MS` must be `10000` — exact parity with the panel's own constant in `src/esp32/main.cpp`.
- The new `loop()` block must run **before** the existing `for (auto& ledStation : ledStations) { ledStation.update(); }` loop, gated by `if (bootMode == BootMode::Normal)`, and must carry the panel's exact explanatory comment verbatim (reproduced in Task 1 below) — this ordering is load-bearing, not stylistic: a fresh identify activation must take visible effect the same tick it arrives, and the comment is what stops a future edit from silently merging the two loops and reintroducing a one-tick lag.
- Must not touch `src/esp32/main.cpp`, `src/mega/main.cpp`, or any file under `lib/` — this sub-project is `src/esp32node4/main.cpp` plus documentation only.
- Every sub-project in this effort documents itself in `CLAUDE.md` under a new `### <Name> (sub-project #N)` heading, and in `docs/pcb/turnout-node-4ch-pcb-design.md` §7's status line and reference lists. Stale claims from earlier sub-projects that this one makes false must be annotated in place (never deleted), per the project's established convention.

---

### Task 1: Wire the identify-overlay into `src/esp32node4/main.cpp`

**Files:**
- Modify: `src/esp32node4/main.cpp`

**Interfaces:**
- Consumes: `IdentifyModeTimer(Clock&, unsigned long durationMs)` with methods `void trigger()` and `bool isActive() const` (from `lib/McsEsp32/src/domain/IdentifyModeTimer.h`); `PresenceTopics::identifyTopic(const std::string& nodeId)` returning `std::string` (from `lib/McsEsp32/src/domain/PresenceTopics.h`); `IndependentLedPairStation::setIdentifying(bool)` (from `lib/McsEsp32/src/adapters/IndependentLedPairStation.h`, already included via `adapters/IndependentLedPairStation.h`); `MqttTransport::subscribe(const std::string& topic, std::function<void(const std::string&)> callback)` (already used one line above the new subscribe call, via `mqttLink.subscribe(...)`).
- Produces: nothing new consumed by a later task — this is the only code task in this plan.

The file currently reads (relevant excerpts — full current content was verified immediately before writing this plan, at commit `e96456e`, with no intervening commits touching this file):

```cpp
#include "domain/MacAddress.h"
#include "domain/NodeConfig.h"
#include "domain/NodeIdentityGuard.h"
#include "domain/PresenceTopics.h"
#include "domain/SetupApName.h"
```

```cpp
namespace
{
    constexpr unsigned long UART_BAUD_RATE = 115200;
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
    constexpr unsigned long RETRY_INTERVAL_MS = 5000;
    constexpr unsigned long BROKER_RESOLVE_INTERVAL_MS = 60000;
}
```

```cpp
NodeIdentityGuard identityGuard(ownMac.lastFourHexDigits());
MqttPresenceAnnouncer presenceAnnouncer(mqttLink, systemClock, runningConfig.nodeId, ownMac.lastFourHexDigits());

Loco2MqttTurnoutCommandAdapter4 turnoutCommandPort(mqttLink, runningConfig.channelTurnoutAddresses);
Loco2MqttFeedbackSource4 feedbackSource(mqttLink, runningConfig.channelTurnoutAddresses);
```

```cpp
    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
        mqttLink.subscribe(PresenceTopics::macTopic(runningConfig.nodeId),
                            [](const std::string& payload) { identityGuard.onMacObserved(payload); });
    }
```

```cpp
    presenceAnnouncer.update(mqttLink.connected());

    if (configValid && mqttLink.connected() && !collision)
    {
        TurnoutFeedback feedback{};
        while (feedbackSource.poll(feedback))
        {
            for (auto& station : stations)
            {
                station.applyFeedback(feedback);
            }
        }
    }
    else
    {
        for (auto& station : stations)
        {
            station.clearIndicator();
        }
    }

    for (auto& ledStation : ledStations)
    {
        ledStation.update();
    }

    for (auto& station : stations)
    {
        station.update();
    }
}
```

- [ ] **Step 1: Add the `IdentifyModeTimer` include**

In the `domain/` include block, add the new include alphabetically between `domain/MacAddress.h` and `domain/NodeConfig.h`:

```cpp
#include "domain/IdentifyModeTimer.h"
#include "domain/MacAddress.h"
#include "domain/NodeConfig.h"
#include "domain/NodeIdentityGuard.h"
#include "domain/PresenceTopics.h"
#include "domain/SetupApName.h"
```

- [ ] **Step 2: Add the `IDENTIFY_DURATION_MS` constant**

In the anonymous namespace holding the other constants, add the new constant (exact value `10000`, matching the panel's own):

```cpp
namespace
{
    constexpr unsigned long UART_BAUD_RATE = 115200;
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
    constexpr unsigned long RETRY_INTERVAL_MS = 5000;
    constexpr unsigned long BROKER_RESOLVE_INTERVAL_MS = 60000;
    constexpr unsigned long IDENTIFY_DURATION_MS = 10000;
}
```

- [ ] **Step 3: Add the `identifyTimer` global**

Immediately after the `presenceAnnouncer` global, before the `Loco2MqttTurnoutCommandAdapter4`/`Loco2MqttFeedbackSource4` globals:

```cpp
NodeIdentityGuard identityGuard(ownMac.lastFourHexDigits());
MqttPresenceAnnouncer presenceAnnouncer(mqttLink, systemClock, runningConfig.nodeId, ownMac.lastFourHexDigits());
IdentifyModeTimer identifyTimer(systemClock, IDENTIFY_DURATION_MS);

Loco2MqttTurnoutCommandAdapter4 turnoutCommandPort(mqttLink, runningConfig.channelTurnoutAddresses);
Loco2MqttFeedbackSource4 feedbackSource(mqttLink, runningConfig.channelTurnoutAddresses);
```

- [ ] **Step 4: Subscribe to the identify topic in `setup()`**

Inside the existing `if (configValid) { ... }` block, immediately after the `macTopic` subscribe:

```cpp
    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
        mqttLink.subscribe(PresenceTopics::macTopic(runningConfig.nodeId),
                            [](const std::string& payload) { identityGuard.onMacObserved(payload); });
        mqttLink.subscribe(PresenceTopics::identifyTopic(runningConfig.nodeId),
                            [](const std::string&) { identifyTimer.trigger(); });
    }
```

- [ ] **Step 5: Drive `setIdentifying()` in `loop()`**

Insert the following block after the feedback-apply-or-`clearIndicator()` block (right after `presenceAnnouncer.update(mqttLink.connected());` and the `if (configValid && mqttLink.connected() && !collision) { ... } else { ... }` block) and **before** the existing `for (auto& ledStation : ledStations) { ledStation.update(); }` loop:

```cpp
    presenceAnnouncer.update(mqttLink.connected());

    if (configValid && mqttLink.connected() && !collision)
    {
        TurnoutFeedback feedback{};
        while (feedbackSource.poll(feedback))
        {
            for (auto& station : stations)
            {
                station.applyFeedback(feedback);
            }
        }
    }
    else
    {
        for (auto& station : stations)
        {
            station.clearIndicator();
        }
    }

    // Must run before the ledStation.update() loop below (and after
    // mqttLink.poll(), which is where a fresh trigger() lands) so a new
    // identify activation takes visible effect the same tick it arrives.
    // Merging the two loops would compile and pass every test while
    // silently reintroducing a one-tick activation lag.
    if (bootMode == BootMode::Normal)
    {
        const bool identifying = identifyTimer.isActive();
        for (auto& ledStation : ledStations)
        {
            ledStation.setIdentifying(identifying);
        }
    }

    for (auto& ledStation : ledStations)
    {
        ledStation.update();
    }

    for (auto& station : stations)
    {
        station.update();
    }
}
```

- [ ] **Step 6: Build all 3 hardware environments**

Run (via Bash/Git Bash, not PowerShell — see the project's standing caution about PowerShell producing false crash results on this machine):

```bash
pio run -e esp32node4
pio run -e esp32dev
pio run -e megaatmega2560
```

Expected: all three build clean (`SUCCESS`), with no errors or warnings about the new code. `esp32dev`/`megaatmega2560` are expected to be byte-for-byte unaffected by this change (no shared library code touched) — their build is a regression check, not something this task's code could plausibly break, but it must still be run and reported.

- [ ] **Step 7: Run the native test suite**

```bash
pio test -e native
```

Expected: unaffected — same suite count and pass count as before this task (54 suites, all passing). This task touches no code under `lib/` or `test/`, so this is purely a regression check.

**Do NOT launch this command with a backgrounding mechanism and then wait across turns for it.** `pio test -e native` on this machine takes several minutes; run it and wait for it to finish in the same turn, exactly as you would any other command whose output you need. (This exact mistake has recurred across sub-projects #7 and #8's dispatches — treat this instruction as load-bearing, not boilerplate.)

- [ ] **Step 8: Commit**

```bash
git add src/esp32node4/main.cpp
git commit -m "$(cat <<'EOF'
@ F Wire identify-overlay into node4 main.cpp

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(`@` risk symbol: `main.cpp` composition-root change with no possible native test, same classification as sub-project #8's Task 1 — matches this project's established ACN convention for composition-root-only commits.)

---

### Task 2: Document the identify-overlay and run the required staleness sweeps

**Files:**
- Modify: `CLAUDE.md`
- Modify: `docs/pcb/turnout-node-4ch-pcb-design.md`

**Interfaces:**
- Consumes: Task 1's completed wiring (this task describes what Task 1 built; it does not touch code).
- Produces: nothing consumed by a later task — this is the last task in this plan.

- [ ] **Step 1: Add the new CLAUDE.md section**

Add a new `### Identify-overlay (sub-project #9)` section to `CLAUDE.md`, immediately after the "### Presence/collision detection (sub-project #8)" section (which currently ends right before the "### Loco2MQTT turnout bridge (sub-project #9a)" section — note the pre-existing "#9a" is a *different*, older numbering sequence for an unrelated panel sub-project; this new "#9" is this node-board effort's own sequence, immediately following #8, exactly as the file's own "4-channel node board" intro paragraph already explains for how the two numbering sequences coexist).

Use this exact text:

```markdown
### Identify-overlay (sub-project #9)

Closes the last item on CLAUDE.md's "deferred to whichever future
sub-project adds it" list for the node board's identify mechanism — tracked
since sub-project #5, reaffirmed as still open in #6, #7, and #8.
`src/esp32node4/main.cpp` now wires the panel's own `IdentifyModeTimer`,
unmodified, mirroring `src/esp32/main.cpp`'s existing identify-overlay
exactly: a new `IdentifyModeTimer identifyTimer(systemClock,
IDENTIFY_DURATION_MS);` global (declared right after `presenceAnnouncer`,
`IDENTIFY_DURATION_MS = 10000`, matching the panel's own constant),
`setup()` subscribing to `PresenceTopics::identifyTopic(nodeId)` with a
lambda that ignores the payload and calls `identifyTimer.trigger()`
(added right after the existing `macTopic` subscribe, inside the same
`if (configValid)` block), and a `loop()` block — gated by `if (bootMode
== BootMode::Normal)`, carrying the panel's own ordering comment
verbatim — that computes `identifying` once per tick from
`identifyTimer.isActive()` and calls `ledStation.setIdentifying(identifying)`
on all 4 `IndependentLedPairStation`s, running before the existing
`ledStation.update()` loop for the same reason the panel's does: a fresh
activation must take visible effect the same tick it arrives, not one
tick later.

No new library code: `IdentifyModeTimer`, `PresenceTopics::identifyTopic()`,
and `IndependentLedPairStation::setIdentifying()` (which the node board
already exercises for boot-mode flashing, sub-project #5) are all reused
unmodified. The `bootMode == BootMode::Normal` gate is carried over
verbatim from the panel even though it is currently redundant on both
boards — the MQTT subscribe that is the only way to trigger
`identifyTimer` lives inside the post-`WirelessSetup`-return `configValid`
branch, so by the time this line runs `bootMode` can only be `Normal` or
`NeedsCommissioning`, and `NeedsCommissioning` implies `configValid` was
false, which means the subscribe never happened either. The gate is kept
for literal parity with the panel rather than relying on that chain of
reasoning staying true if either file changes independently later.

**Net effect:** publishing any message (payload ignored) to
`panel/<nodeId>/identify` now makes a node4 board's 4 LED pairs flash
green/red in unison at a fast, fixed 150ms interval for 10 seconds, then
automatically revert — identical behavior to the panel. Re-publishing
before the window expires extends it rather than stacking a second one
(`IdentifyModeTimer`'s own existing behavior, unchanged). Identify has no
button/feedback suppression and deliberately still works during a latched
`nodeId` collision, exactly as on the panel — that's the intended way to
tell two colliding boards apart. The `BootMode::WirelessSetup` branch
needs no change: its own LED flash (sub-project #5's
`IndependentLedPairFlashIndicator`/`bootModeIndicator`) is a separate
mechanism that the two can never overlap with, since wireless setup mode
never starts MQTT and that branch `return`s before reaching the new
`loop()` block above — exactly as on the panel.

Because `PresenceTopics` hard-codes a single `panel/<nodeId>/...` prefix
shared by both board families (first documented for the collision topic
in sub-project #8's section above), a panel and a node4 board
accidentally commissioned with the same `nodeId` can now also trigger
each other's identify-overlay flash, not just two boards of the same
family — the same kind of cross-family interaction #8 already documented
for collision detection, restated here for the identify topic.

**Test coverage added: none.** `main.cpp` is a composition root verified
by build only (`pio run -e esp32node4`/`esp32dev`/`megaatmega2560`), same
precedent as every prior `main.cpp`-only change in this project. Suite
count stays at 54.

**Non-goals, deferred:** status/activity LEDs (PCB doc §7 item 5) and the
4-channel commissioning form remain their own future sub-projects. The
hardware bring-up checklist (physically verifying buttons, LEDs, and
LocoNet send/receive against real hardware) also remains outstanding,
unrelated to this sub-project's scope.
```

- [ ] **Step 2: Run the general staleness sweep**

```bash
grep -n "future sub-project\|blocked on\|unwired\|not done\|still\|yet" CLAUDE.md docs/pcb/*.md
```

Read every hit. The following are already known, confirmed-current locations that this sub-project makes false and that MUST be annotated in place (never deleted) — using the exact in-place-annotation convention this file already uses elsewhere (append a parenthetical or sentence noting what closed the gap and pointing at this section, the way sub-project #8's own fixes did for #1/#5/#6/#7's stale claims):

1. **`CLAUDE.md` line ~265** (inside the "4-channel node board" intro paragraph): `"...WiFi/MQTT now exist as of sub-project #7 — see below; presence/collision was wired in sub-project #8 — see below; identify remains unwired, its own future sub-project)."` — change the identify clause to note it was closed by sub-project #9 (see below), matching the "see below" pattern already used for the #7/#8 clauses immediately before it.
2. **`CLAUDE.md` line ~334** (end of the "Independent-output turnout indicator (sub-project #2)" section): `"...sub-project #5's \`IndependentLedPairFlashIndicator\` is its first caller (see below) — no MQTT-triggered identify-overlay wiring exists for the node board yet."` — annotate that sub-project #9 added this wiring; keep the sentence's first half (about `IndependentLedPairFlashIndicator` being the first caller) intact since it's still true — `setIdentifying()` now has a second caller as of #9, not a replacement.
3. **`CLAUDE.md` lines ~359–362** (end of the same section's "Deferred at the time, now done (sub-project #4)" paragraph): `"Identify-overlay wiring for these LEDs is still **not done** (§7 item 4, its own future sub-project), so \`IndependentLedPairDriver::setIdentifying()\`'s *MQTT-triggered* identify-overlay use remains unwired — though sub-project #5 did wire a boot-mode-triggered call to it (see below)."` — annotate that this is now done, sub-project #9, pointing at the new section.
4. **`CLAUDE.md` line ~556** (end of the "Boot-mode LED indicator (sub-project #5)" section's opening paragraph): `"...(that sub-project turned out to be #7, which deliberately did NOT include the identify-overlay — it remains unwired, deferred further)."` — annotate that sub-project #9 closed this.
5. **`CLAUDE.md` lines ~608–611** (the "Boot-mode LED indicator (sub-project #5)" section's own "Non-goals, deferred" paragraph): `"**Non-goals, deferred:** identify-overlay wiring (\`IdentifyModeTimer\`, an MQTT \`panel/<nodeId>/identify\` subscription, a \`setIdentifying()\` loop driven by it) — no longer blocked (sub-project #7 landed that wiring) but still not done — deferred to whichever future sub-project adds it."` — annotate as closed by sub-project #9.
6. **`CLAUDE.md` lines ~630–631** (end of the "Loco2MQTT channel-count templating (sub-project #6)" section's opening paragraph): `"**Presence/collision detection was closed by sub-project #8 — see below.** The identify-overlay remains deferred to whichever future sub-project adds it."` — annotate the identify-overlay clause as closed by sub-project #9, matching the existing "see below" pattern.
7. **`CLAUDE.md` lines ~669–672** (opening paragraph of "Loco2MQTT WiFi/MQTT wiring (sub-project #7)"): `"...with presence/collision detection and the identify-overlay both left out — neither is part of this sub-project. **Presence/collision detection was closed by sub-project #8 — see below.** The identify-overlay remains deferred to whichever future sub-project adds it."` — annotate the identify-overlay clause as closed by sub-project #9.
8. **`CLAUDE.md` lines ~797–799+** (the "Presence/collision detection (sub-project #8)" section's own "Non-goals, deferred" paragraph): `"**Non-goals, deferred:** the identify-overlay (\`IdentifyModeTimer\`, an MQTT \`panel/<nodeId>/identify\` subscription, a \`setIdentifying()\` loop driven by it) — a separate choice from this sub-project's scope, deferred to whichever future sub-project adds it."` — read the full sentence (it continues past what's quoted here — re-read the live file at this point, since the quoted fragment may not be the complete sentence) and annotate the identify-overlay clause as closed by sub-project #9, leaving the "Status/activity LEDs...remain their own future sub-projects" clause that follows it untouched (that one stays true).

Do not treat this list as exhaustive — re-run the grep yourself and read every hit's surrounding context, the same way sub-project #8's own sweep found 3 more locations than its brief anticipated (2 by reading full context around a grep hit, 1 by contextual reading near a hit that wasn't a literal keyword match). If the grep surfaces a hit unrelated to the node board's identify-overlay (e.g. the panel's own, already-correct identify-overlay description, or `IdentifyModeTimer`'s own class description in the file listing — these describe already-shipped, still-true behavior and must NOT be edited), leave it alone; only touch a hit that is actually made false by this sub-project's work.

- [ ] **Step 3: Run the PCB-doc-specific staleness sweep**

```bash
grep -n "Status as of sub-project" docs/pcb/*.md
```

This currently matches one line in `docs/pcb/turnout-node-4ch-pcb-design.md` (§7, immediately after the "## 7. Firmware impacts (Proposed)" heading):

```
**Status as of sub-project #8 (2026-10-24):** items 1 (sub-project #1), 2
(sub-project #4), 3 (sub-projects #2 and #4 together), 4's boot/setup half
(sub-project #5), and 6 (sub-project #3) are done; item 4's identify half
and item 5 remain open. Sub-project #7 (2026-10-22) landed the real
WiFi/MQTT/Loco2MQTT wiring both of these would build on, and sub-project
#8 (2026-10-24) used that wiring to bring up presence/collision detection
(item 7) on the node board itself — neither identify nor status LEDs are
wired yet themselves, but the infrastructure they'll need now exists on
the node board. See
`docs/superpowers/specs/2026-10-21-esp32-node4-channel-count-design.md`,
`docs/superpowers/specs/2026-10-21-esp32-node4-independent-led-pair-driver-design.md`,
`docs/superpowers/specs/2026-10-21-esp32-node4-setup-trigger-wiring-design.md`,
`docs/superpowers/specs/2026-10-22-esp32-node4-turnout-station-wiring-design.md`,
`docs/superpowers/specs/2026-10-22-esp32-node4-boot-mode-indicator-design.md`,
`docs/superpowers/specs/2026-10-22-esp32-node4-loco2mqtt-channel-count-design.md`,
`docs/superpowers/specs/2026-10-22-esp32-node4-loco2mqtt-wiring-design.md`,
and
`docs/superpowers/specs/2026-10-24-esp32node4-presence-collision-design.md`,
plus `CLAUDE.md`'s "4-channel node board: channel-count templating",
"Independent-output turnout indicator", "Composite setup-mode trigger",
"Turnout station wiring", "Boot-mode LED indicator", "Loco2MQTT
channel-count templating", "Loco2MQTT WiFi/MQTT wiring", and
"Presence/collision detection" sections,
for the implementations.
```

Replace it with:

```
**Status as of sub-project #9 (2026-10-25):** items 1 (sub-project #1), 2
(sub-project #4), 3 (sub-projects #2 and #4 together), 4 (boot/setup half
in sub-project #5, identify half in sub-project #9), and 6 (sub-project #3)
are done; item 5 remains open. Sub-project #7 (2026-10-22) landed the real
WiFi/MQTT/Loco2MQTT wiring that both item 4's identify half and item 7
built on, sub-project #8 (2026-10-24) used it to bring up presence/collision
detection (item 7), and sub-project #9 (2026-10-25) used it to bring up the
identify-overlay (item 4's remaining half) — status LEDs (item 5) are the
one item with no wiring yet. See
`docs/superpowers/specs/2026-10-21-esp32-node4-channel-count-design.md`,
`docs/superpowers/specs/2026-10-21-esp32-node4-independent-led-pair-driver-design.md`,
`docs/superpowers/specs/2026-10-21-esp32-node4-setup-trigger-wiring-design.md`,
`docs/superpowers/specs/2026-10-22-esp32-node4-turnout-station-wiring-design.md`,
`docs/superpowers/specs/2026-10-22-esp32-node4-boot-mode-indicator-design.md`,
`docs/superpowers/specs/2026-10-22-esp32-node4-loco2mqtt-channel-count-design.md`,
`docs/superpowers/specs/2026-10-22-esp32-node4-loco2mqtt-wiring-design.md`,
`docs/superpowers/specs/2026-10-24-esp32node4-presence-collision-design.md`,
and
`docs/superpowers/specs/2026-10-25-esp32node4-identify-overlay-design.md`,
plus `CLAUDE.md`'s "4-channel node board: channel-count templating",
"Independent-output turnout indicator", "Composite setup-mode trigger",
"Turnout station wiring", "Boot-mode LED indicator", "Loco2MQTT
channel-count templating", "Loco2MQTT WiFi/MQTT wiring", "Presence/collision
detection", and "Identify-overlay" sections,
for the implementations.
```

- [ ] **Step 4: Update the §7 item 4 checklist bullet in the PCB doc**

The existing bullet (around line 501, immediately after the "Firmware work... item 4, boot/setup-mode half" bullet) reads:

```
- [x] **Firmware work** (§7) **item 4, boot/setup-mode half** — `IndependentLedPairFlashIndicator` mirrors `LedPairFlashIndicator` for the node board's independent outputs, sub-project #5 (2026-10-22). The identify-overlay half of item 4, and item 5 (status/activity LEDs), remain open — identify is blocked on real WiFi/MQTT wiring landing on the node board (own future sub-project).
```

Replace the second sentence (the checklist item itself stays a single `[x]` line — don't split it into two checklist items) so the full bullet reads:

```
- [x] **Firmware work** (§7) **item 4** — boot/setup-mode half: `IndependentLedPairFlashIndicator` mirrors `LedPairFlashIndicator` for the node board's independent outputs, sub-project #5 (2026-10-22). Identify-overlay half: `IdentifyModeTimer` wired via an MQTT `panel/<nodeId>/identify` subscription, sub-project #9 (2026-10-25) — item 4 is now fully done. Item 5 (status/activity LEDs) remains open.
```

- [ ] **Step 5: Verify both sweeps are clean**

Re-run both greps from Steps 2 and 3. Confirm every hit that was made false by this sub-project's work is now annotated, and that no hit describing already-correct, still-true behavior (e.g. the panel's own identify-overlay section, or `IdentifyModeTimer`'s class description in the file-listing paragraph) was altered.

- [ ] **Step 6: Commit**

```bash
git add CLAUDE.md docs/pcb/turnout-node-4ch-pcb-design.md
git commit -m "$(cat <<'EOF'
. d Document sub-project #9 identify-overlay wiring

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(`. d` — documentation only, zero behavior change, matching this project's established classification for doc-update commits.)
