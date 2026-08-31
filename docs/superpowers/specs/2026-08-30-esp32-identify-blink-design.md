# ESP32 Identify-Blink (Sub-project #2d-b) — Design

This is sub-project **#2d-b**, the second and final half of sub-project #2d
(multi-panel field identification + collision safety), following the
now-complete #2d-a (presence + MQTT collision self-detection, merged to
`main`). This spec covers making a specific physical panel visually
identify itself on command, so a technician debugging multiple deployed
panels can tell which board corresponds to which `nodeId`.

## Context

#2d-a's own design deferred this exact feature: "Identify-blink (a way for
a technician to make a specific physical panel visually identify itself)
is a separate, independently valuable piece of work and gets its own
brainstorm as sub-project #2d-b." The need is real independent of #2d-a's
collision detection — verifying commissioning worked, debugging via
JMRI/MQTT tooling, or (now that #2d-a exists) telling apart two panels
mid-collision all require the same thing: make one specific board light up
differently from every other board on the layout, on command, from
wherever the technician's tooling is (not necessarily standing at the
panel — if they already knew which physical board was which, they
wouldn't need this).

The existing LED machinery (`lib/McsEsp32/src/domain/LedPairDriver.h`/`.cpp`)
drives one shared-GPIO red/green pair per turnout: steady green, steady
red, or a slow (500ms) blink between the last-displayed color and its
opposite, used today for "unconfirmed" state (MQTT disconnected, config
invalid, or — as of #2d-a — a latched `nodeId` collision).
`LedPairStation` (`lib/McsEsp32/src/adapters/LedPairStation.h`,
`#ifdef ARDUINO`-guarded) is the per-turnout composition helper owning one
`LedPairDriver` plus its two `LedPairOutput`s.

## Decisions (confirmed via Q&A)

1. **Trigger: MQTT only.** Subscribe to a new `panel/<nodeId>/identify`
   topic; any message on it starts (or refreshes) a fixed-duration
   identify flash. A technician trying to identify which physical panel
   is `nodeId 5` is at a computer with MQTT/JMRI tooling, not already
   standing at the panel with a USB cable — if they were already at the
   right panel, they wouldn't need this feature at all. No bench-serial
   trigger.
2. **Buttons and turnout feedback stay fully live during identify** — this
   is a pure visual overlay, not a lockout. Unlike #2d-a's collision
   suppression (which must stop commands because two panels sending
   conflicting commands is genuinely harmful), there's no reason to
   interfere with normal operation just because a panel is identifying
   itself.
3. **The visual pattern needs new `LedPairDriver` logic, unlike #2d-a's
   collision indicator.** #2d-a's collision state reused the existing
   blink/unconfirmed display because a technician investigating a
   collision would already be reading serial output or the `mac` topic,
   not judging collision status by LED color. Identify's entire purpose
   is being visually distinct enough to catch a technician's eye across
   a room, so reuse doesn't serve the goal here — a new fast, synchronized
   flash across all 12 pairs is added.
4. **Fixed 10-second duration, auto-stopping, no explicit "off" command.**
   Simpler than adding a second command/topic; a technician who wants more
   time just triggers it again (refreshes the window rather than stacking
   a second timer).

## Components

### `IdentifyModeTimer` (`lib/McsEsp32/src/domain/IdentifyModeTimer.h`/`.cpp`)

Simpler than `ComboSetupModeTrigger` — no edge/latch subtlety, just an
elapsed-time check computed on demand (no `update()` call needed):

```cpp
#pragma once

#include "ports/Clock.h"

class IdentifyModeTimer
{
public:
    IdentifyModeTimer(Clock& clock, unsigned long durationMs);

    void trigger();
    [[nodiscard]] bool isActive() const;

private:
    Clock& clock_;
    unsigned long durationMs_;
    bool triggered_ = false;
    unsigned long triggeredAtMs_ = 0;
};
```

```cpp
IdentifyModeTimer::IdentifyModeTimer(Clock& clock, const unsigned long durationMs)
    : clock_(clock), durationMs_(durationMs)
{
}

void IdentifyModeTimer::trigger()
{
    triggered_ = true;
    triggeredAtMs_ = clock_.nowMilliseconds();
}

bool IdentifyModeTimer::isActive() const
{
    return triggered_ && (clock_.nowMilliseconds() - triggeredAtMs_ < durationMs_);
}
```

`trigger()` always re-stamps `triggeredAtMs_`, so a second MQTT message
before the window expires extends it rather than starting a second,
independently-expiring timer.

### `PresenceTopics::identifyTopic()` (`lib/McsEsp32/src/domain/PresenceTopics.h`)

One additive static method on the existing class (built in #2d-a), rather
than a new topic-naming class for one method:

```cpp
static std::string identifyTopic(int nodeId)
{
    return "panel/" + std::to_string(nodeId) + "/identify";
}
```

### `LedPairDriver` — `setIdentifying(bool)` (modifies existing, already-tested class)

```cpp
// New public method:
void setIdentifying(bool active);

// New private state:
bool identifying_ = false;
bool identifyShowingGreen_ = true;
unsigned long identifyLastToggleMs_ = 0;
static constexpr unsigned long kIdentifyIntervalMs = 150;
```

```cpp
void LedPairDriver::setIdentifying(const bool active)
{
    if (active == identifying_)
    {
        return;
    }
    identifying_ = active;
    if (identifying_)
    {
        identifyShowingGreen_ = true;
        identifyLastToggleMs_ = clock_.nowMilliseconds();
        writeColor(LedPairColor::Green);
    }
    else
    {
        writeColor(currentColorToShow());
    }
}
```

`update()` gains a short-circuit at the top:

```cpp
void LedPairDriver::update()
{
    if (identifying_)
    {
        const unsigned long now = clock_.nowMilliseconds();
        if (now - identifyLastToggleMs_ >= kIdentifyIntervalMs)
        {
            identifyShowingGreen_ = !identifyShowingGreen_;
            identifyLastToggleMs_ = now;
            writeColor(identifyShowingGreen_ ? LedPairColor::Green : LedPairColor::Red);
        }
        return;
    }

    // ...existing blink-mode logic, unchanged below this point
}
```

**Idempotency is load-bearing, not incidental.** `main.cpp` calls
`setIdentifying(identifyTimer.isActive())` on all 12 stations every
`loop()` tick — if `setIdentifying(true)` reset `identifyLastToggleMs_`
on every call rather than only on the `false`→`true` transition, the
flash would never actually toggle (stuck showing green for the entire
10-second window, since the toggle-interval check would never see enough
elapsed time). The `if (active == identifying_) { return; }` guard at the
top makes it safe to call every tick with the same value, matching the
idempotency pattern `applyState()` already uses elsewhere in this class.

**Accepted interaction, not defended against:** if real turnout feedback
arrives mid-identify (`applyState()` runs because `greenRequested_`/
`redRequested_` changed), it writes directly to the GPIO via its own
`writeColor()` call, which can briefly desynchronize that one pair from
the rest of the panel's flash. The next identify tick (at most 150ms
later) overwrites it again. This is a sub-200ms cosmetic glitch on at
most one LED pair in the rare case a state change lands during an active
identify window — not worth gating `applyState()` on `!identifying_` and
then having to "catch up" the suppressed display change afterward.

### `LedPairStation` — `setIdentifying(bool)` (one-line forwarding, no test)

```cpp
void setIdentifying(bool active);
```

```cpp
void LedPairStation::setIdentifying(const bool active)
{
    driver_.setIdentifying(active);
}
```

`#ifdef ARDUINO`-guarded like the rest of the class — build-check only via
`pio run -e esp32dev`, matching its existing convention (no native test
for this class today).

### `src/esp32/main.cpp` changes

New constant in the existing anonymous namespace:

```cpp
constexpr unsigned long IDENTIFY_DURATION_MS = 10000;
```

New global, placed after `identityGuard`/`presenceAnnouncer` (order
relative to those two doesn't matter — no dependency between them):

```cpp
IdentifyModeTimer identifyTimer(systemClock, IDENTIFY_DURATION_MS);
```

In `setup()`, inside the existing `if (configValid)` block, alongside the
mac-topic subscription:

```cpp
mqttLink.subscribe(PresenceTopics::identifyTopic(runningConfig.nodeId),
                    [](const std::string&) { identifyTimer.trigger(); });
```

The payload is ignored — any message triggers it.

In `loop()`, a new per-tick loop, placed near the existing `ledStation`
loops (exact position doesn't interact with anything else — `setIdentifying()`
only touches each `LedPairDriver`'s own internal state):

```cpp
const bool identifying = identifyTimer.isActive();
for (auto& ledStation : ledStations)
{
    ledStation.setIdentifying(identifying);
}
```

Kept as its own loop rather than folded into the existing `ledStation.update()`
loop, so `LedPairStation::update()`'s signature stays unchanged.

## Testing

- **`IdentifyModeTimer`**, via `FakeClock`: `isActive()` is false before
  any `trigger()`; true immediately after; false once `durationMs_` has
  elapsed; a second `trigger()` before expiry extends the active window
  past what the first trigger alone would have covered.
- **`LedPairDriver`** (additive cases in the existing suite):
  `setIdentifying(true)` immediately shows green; `update()` after
  `kIdentifyIntervalMs` toggles to red, and again back to green;
  `setIdentifying(true)` called again while already active does not reset
  the toggle timer (idempotency — construct, activate, advance most of
  the way to the first toggle, call `setIdentifying(true)` again, advance
  the remaining time, confirm it still toggles on schedule rather than
  restarting); `setIdentifying(false)` reverts to steady green, steady
  red, and blink mode correctly in three separate cases (whichever the
  driver's normal state would currently show).
- **`PresenceTopics::identifyTopic()`**: one additive case in the existing
  suite, same style as `statusTopic()`/`macTopic()`.
- **`LedPairStation`, `src/esp32/main.cpp`**: no native test (matches
  existing convention for both) — verified via `pio run -e esp32dev`
  build success and a manual read-through for `main.cpp`.

## File layout

- `lib/McsEsp32/src/domain/`: `IdentifyModeTimer.h`/`.cpp` (new)
- Modify: `lib/McsEsp32/src/domain/PresenceTopics.h` (add `identifyTopic()`)
- Modify: `lib/McsEsp32/src/domain/LedPairDriver.h`/`.cpp` (add
  `setIdentifying()`)
- Modify: `lib/McsEsp32/src/adapters/LedPairStation.h`/`.cpp` (add
  one-line forwarding `setIdentifying()`)
- Modify: `src/esp32/main.cpp` (new constant, new global, new
  subscription, new per-tick loop)
- `test/test_identify_mode_timer/`, plus additive cases in
  `test/test_led_pair_driver/` and `test/test_presence_topics/`

## Non-goals

- Any explicit "stop identifying" command — the fixed 10-second timeout
  is the only way it ends (re-triggering extends it, it never needs to be
  cancelled early for this project's actual use case).
- Bench-serial identify trigger.
- Any change to `ComboSetupModeTrigger`, `NodeIdentityGuard`,
  `MqttPresenceAnnouncer`, or any other #2d-a/#2c-*/#7b class — all reused
  exactly as built.
- Suppressing turnout buttons or feedback during identify — it is a pure
  visual overlay by design (Decision 2).
- Engineering around the sub-200ms cosmetic glitch described under
  `LedPairDriver` above.
