# JMRI/MQTT Topic Self-Echo Resolution — Design

This resolves the design question the 2b final whole-branch review flagged as
blocking for sub-project #6 (JMRI turnout command/feedback wiring into
`TurnoutControl`) — see
`docs/superpowers/specs/2026-08-28-esp32-jmri-mqtt-transport-design.md`'s
"Post-implementation amendment: topic self-echo risk for sub-project #6".
This spec covers only that question; it does not cover #6's own wiring work,
which gets its own plan next.

## Problem

`JmriTurnoutCommandAdapter` publishes retained commands to
`track/turnout/<name>`, and `JmriFeedbackSource` was going to subscribe that
*same* topic for feedback. MQTT 3.1.1 (what this project's `PubSubClient`
dependency speaks) has no way for a client to suppress receiving its own
publishes back — that's an MQTT5-only "No Local" subscription option — so
once #6 wires these together, every button press would deliver the panel's
own outgoing command straight back to itself as if it were feedback.

This isn't just a theoretical risk: an earlier design doc
(`docs/ESP32_Turnout_Panel_Implementation.md`, 2026-07-21, predating the
current 2a/2b spec pair) already settled that the panel's LEDs must reflect
only JMRI/driver-confirmed state and must never update optimistically on a
button press (see its software responsibility #11 and its "Command" data-flow
note: *"`update()` never touches the indicator, so the LED does not change at
this point"*). Self-echo would silently violate that rule — the LED would
light immediately off the panel's own command, not off any real confirmation
that the turnout actually moved. That rule is confirmed still binding as of
this spec.

## Real-world context (from the sibling project's setup notes)

`../MaltbeeTurnoutController/internal_documents/mqtt-broker-jmri-setup.md`
documents the actual, currently-configured JMRI/broker topology this panel
will join:

- JMRI's MQTT connection has separate configurable Send and Receive topic
  prefixes, but this layout's JMRI is configured with them **the same** —
  so JMRI, the turnout-driver node, and (once wired) this panel all
  publish *and* subscribe on one shared topic per turnout,
  `/trains/track/turnout/<suffix>` in JMRI's real configured form
  (`track/turnout/<name>` in this project's own `TopicScheme`, per 2b's
  scope decision to reuse the sibling's protocol verbatim, re-keyed by
  JMRI name instead of numeric id).
- The driver node already tolerates self-echo on that shared topic today —
  harmless for it, since re-receiving its own just-published state is an
  idempotent "move to the position you're already at."
- JMRI's own feedback mode for these turnouts is MONITORING: JMRI's "known
  state" is just whatever's currently on the topic, with no hard
  distinction between "someone requested this" and "the hardware confirmed
  this."
- The sibling project's own design doc left this exact question open
  (item 10.4, "Send/receive MQTT topics: same, or split?") and picked
  "same" only because nothing consumed it yet at the time, explicitly
  noting it could be revisited "with zero cost if JMRI's actual
  configuration wants something different." This panel's requirement is
  that different need materializing.

This means the panel's self-echo isn't a bug introduced by this project's
code — it's a structural consequence of genuinely sharing one topic with
JMRI and the driver today. Payload content can't disambiguate origin (a
self-echo and a real confirmation carry identical bytes), so the fix has to
change the topology, not the message parsing.

## Decision

Add a second, dedicated topic per turnout that only the real driver hardware
ever publishes to, and have the panel subscribe there for feedback instead of
on the command topic:

- `track/turnout/<name>` — **unchanged.** Still the command topic.
  `JmriTurnoutCommandAdapter` keeps publishing here; JMRI and the driver
  keep consuming it exactly as today.
- `track/turnout/<name>/state` — **new.** Published only by the real
  turnout-driver hardware, only after it has actually executed a command.
  `JmriFeedbackSource` subscribes here instead.

This is deterministic (no timing race to reason about or test), matches the
2026-07-21 doc's "never optimistic" rule exactly, and is purely additive to
the existing shared-topic contract — JMRI's own configuration and the
driver's existing publish to the shared topic are untouched, so nothing
currently relying on that shared topic (JMRI's MONITORING view included)
changes behavior.

Two alternatives were considered and rejected:

- **Accept self-echo** (match JMRI/the driver's existing tolerant behavior,
  drop the never-optimistic rule) — rejected because it reintroduces exactly
  the failure mode that rule exists to prevent: the panel could show a
  turnout as confirmed-moved when it physically didn't (e.g. a stuck point
  or dead solenoid).
- **Timing-based suppression** (treat the first feedback message after the
  panel's own publish as self-echo and drop it, treat a later one as real)
  — rejected because it depends on network/broker ordering rather than a
  hard guarantee, making correctness harder to reason about and to test than
  a structurally-separate topic.

## Changes in this repo (`MaltbeeController`)

- `TopicScheme` (`lib/McsEsp32/src/domain/TopicScheme.h`) gains a second
  static method for the state topic, alongside the existing `topicFor()`
  (which stays the command topic, unchanged in both name and behavior):

  ```cpp
  static std::string topicFor(const std::string& jmriName);       // unchanged: command topic
  static std::string stateTopicFor(const std::string& jmriName);  // new: "track/turnout/" + jmriName + "/state"
  ```

- `JmriFeedbackSource` (`lib/McsEsp32/src/adapters/JmriFeedbackSource.cpp`)
  subscribes to `TopicScheme::stateTopicFor(jmriName)` instead of
  `TopicScheme::topicFor(jmriName)`. This is the only behavioral change in
  this class.
- `JmriTurnoutCommandAdapter` is unchanged — it already publishes to
  `TopicScheme::topicFor(jmriName)`, which remains correct.
- `PayloadCodec` is unchanged — same `CLOSED`/`THROWN` payload encoding on
  both topics.
- Existing native tests (`test_topic_scheme`, `test_jmri_feedback_source`)
  get new/updated cases: `TopicScheme::stateTopicFor()` builds the expected
  `.../state` topic, and `JmriFeedbackSource`'s subscription/delivery tests
  assert against the state topic rather than the command topic.

## Cross-project dependency (tracked here, not implemented here)

`../MaltbeeTurnoutController`'s `MqttPositionReporter`
(`lib/McsCore/src/adapters/MqttPositionReporter.h`) needs to publish state to
**both** the existing shared topic (`TopicScheme::topicFor(id)`, unchanged —
this is what JMRI's own MONITORING-mode view keeps consuming) **and** a new
state topic matching this scheme (for this panel's benefit). This is a small,
additive change confined to that one class in that repo, but it's a separate
project with its own release cadence — sub-project #6's implementation plan
in this repo will record it as an external dependency to raise with that
project, not something this repo's plan implements directly. Until that
change lands there, `JmriFeedbackSource` will simply receive no feedback
messages (a turnout's LED stays in "unconfirmed" blink mode indefinitely) —
a safe, already-designed-for degraded state, not a crash or incorrect
confirmation.

## Non-goals

- Wiring `JmriTurnoutCommandAdapter`/`JmriFeedbackSource` into
  `TurnoutStation`/`TurnoutControl` instances, or any `src/esp32/main.cpp`
  composition-root work — that's sub-project #7, and the remainder of #6's
  own wiring work gets its own plan next.
- Sub-projects 2c (wireless captive-portal commissioning) and 2d (field
  identification + multi-panel collision safety) — untouched by this spec.
- Any change to JMRI's own connection configuration (Send/Receive prefixes)
  — not needed by this decision, since the new state topic is additive
  alongside the existing shared topic JMRI already uses.
