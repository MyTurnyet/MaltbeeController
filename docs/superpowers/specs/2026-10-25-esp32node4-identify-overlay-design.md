# ESP32 Node4 Identify-Overlay Wiring — Design

**Status:** Approved
**Sub-project:** #9 in the 4-turnout node board effort's own numbering (chronologically after #1–#8; see `CLAUDE.md`'s "4-channel node board: channel-count templating (sub-project #1)" section for the numbering convention)

## Problem

The 12-channel panel (`src/esp32/main.cpp`) has an identify-overlay: publishing
any message (payload ignored) to `panel/<nodeId>/identify` makes all 12 LED
pairs flash green/red in unison at a fast, fixed 150ms interval for 10
seconds, then automatically revert. A technician trying to find which
physical panel owns a given `nodeId` triggers this over MQTT and watches for
the flash.

The node4 board (`src/esp32node4/main.cpp`) has no equivalent. CLAUDE.md has
tracked this as a deferred item since sub-project #5 ("the identify-overlay
... deferred to whichever future sub-project adds it"), reaffirmed in #6, #7,
and #8's own non-goals. Every prerequisite is now in place:

- `IdentifyModeTimer` (generic, already used unmodified by the panel) needs
  no changes.
- `PresenceTopics::identifyTopic()` (also generic) needs no changes.
- `IndependentLedPairStation::setIdentifying(bool)` already exists (built in
  sub-project #4, mirroring `LedPairStation`) and is already exercised by
  `IndependentLedPairFlashIndicator` for boot-mode flashing (sub-project #5)
  — this sub-project is simply its second caller.
- Real WiFi/MQTT wiring (`WiFiLink`/`MqttLink`) landed in sub-project #7, and
  the `mqttLink.subscribe(...)` pattern this needs is already established by
  sub-project #8's `macTopic` subscription in the same `setup()` block.

This sub-project closes that gap: no new library code, pure composition-root
wiring in `src/esp32node4/main.cpp`, mirroring the panel's existing mechanism
exactly.

## Decision

### `src/esp32node4/main.cpp` changes

1. New include: `#include "domain/IdentifyModeTimer.h"`, placed alphabetically
   among the existing `domain/` includes (between `MacAddress.h` and
   `NodeConfig.h`).

2. New constant in the file's anonymous namespace, alongside the existing
   `BLINK_INTERVAL_MS`/`RETRY_INTERVAL_MS`/etc.:

   ```cpp
   constexpr unsigned long IDENTIFY_DURATION_MS = 10000;
   ```

   (matches the panel's own `IDENTIFY_DURATION_MS` value exactly).

3. New global, declared immediately after `presenceAnnouncer` and before
   `turnoutCommandPort` — mirroring the panel's exact declaration order:

   ```cpp
   IdentifyModeTimer identifyTimer(systemClock, IDENTIFY_DURATION_MS);
   ```

4. `setup()`: inside the existing `if (configValid) { ... }` block (the one
   that already calls `wifiLink.begin()`, `mqttLink.begin()`, and subscribes
   to `PresenceTopics::macTopic(...)` as of sub-project #8), add immediately
   after the `macTopic` subscribe:

   ```cpp
   mqttLink.subscribe(PresenceTopics::identifyTopic(runningConfig.nodeId),
                       [](const std::string&) { identifyTimer.trigger(); });
   ```

5. `loop()`: insert the following block after the feedback-apply-or-
   `clearIndicator()` block and *before* the existing
   `for (auto& ledStation : ledStations) { ledStation.update(); }` loop:

   ```cpp
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
   ```

   This carries over the panel's `bootMode == BootMode::Normal` gate and its
   exact comment verbatim. The gate is currently redundant on both boards —
   the MQTT subscribe that's the only way to trigger `identifyTimer` lives
   inside the post-`WirelessSetup`-return `configValid` branch, so
   `bootMode` can only be `Normal` or `NeedsCommissioning` by the time this
   line runs, and `NeedsCommissioning` means `configValid` was false, which
   means the subscribe never happened and `identifyTimer` can never have
   been triggered either way. The gate is kept anyway, for literal parity
   with the panel's code rather than relying on that chain of reasoning
   staying true if either file changes independently later.

### Why no change is needed to the `WirelessSetup` branch

The node4 board already has a *different*, pre-existing flash mechanism for
`BootMode::WirelessSetup` (sub-project #5's `IndependentLedPairFlashIndicator`
/ `bootModeIndicator.show(bootMode)`), and that branch `return`s before
reaching the identify block above — exactly as on the panel. The two
mechanisms never need to coexist: wireless setup mode never starts MQTT, so
the MQTT-triggered identify-overlay cannot fire while a board is in that
mode.

### Cross-board-family behavior

`PresenceTopics` hard-codes a single `panel/<nodeId>/...` prefix shared by
both board families (established fact, first documented for the collision
topic in sub-project #8's CLAUDE.md section). As a consequence of this
sub-project, a panel and a node4 board accidentally commissioned with the
same `nodeId` can now also trigger each other's identify-overlay flash, not
just two boards of the same family. This is the same kind of cross-family
interaction #8 already accepted for collision detection, restated here for
the identify topic — not a new category of problem, and not something this
sub-project needs to guard against.

### Testing

No new tests. `main.cpp` is a composition root with no native test target
(`test_build_src = false`), consistent with every prior `main.cpp`-only
change in this project (sub-projects #3, #4, #7, #8). Verification is:

- `pio run -e esp32node4` — builds clean.
- `pio run -e esp32dev` — unaffected (no changes to `src/esp32/main.cpp` or
  any shared library code), confirms no regression to the panel.
- `pio run -e megaatmega2560` — unaffected, same reasoning.
- `pio test -e native` — full 54-suite run stays unaffected (no production
  library code changes).

### Documentation

- `CLAUDE.md` gains a new "### Identify-overlay (sub-project #9)" section,
  following the established pattern of prior sub-project sections.
- Both now-standard staleness sweeps run as an explicit, required plan step:
  - `grep -n "future sub-project\|blocked on\|unwired\|not done\|still\|yet" CLAUDE.md docs/pcb/*.md`
  - `grep -n "Status as of sub-project" docs/pcb/*.md`
  Every hit this sub-project makes false gets annotated in place (never
  deleted), per the project's established convention.
- `docs/pcb/turnout-node-4ch-pcb-design.md` §7's status line and both
  reference lists (spec files, CLAUDE.md section names) get bumped to
  include this sub-project.

## Non-goals

- No change to `IdentifyModeTimer`, `PresenceTopics`, or
  `IndependentLedPairStation`/`IndependentLedPairDriver` — all reused
  unmodified.
- No change to the `WirelessSetup` branch's existing boot-mode flash
  mechanism (sub-project #5) — the two mechanisms are already mutually
  exclusive by construction.
- No new MQTT topic, no new library code, no new tests.
