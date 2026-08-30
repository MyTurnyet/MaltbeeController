# ESP32 Wireless Setup Boot Mode & Trigger (Sub-project #2c-a) — Design

This is sub-project **#2c-a**, split out of sub-project #2c (Wireless
captive-portal commissioning) in the ESP32 panel decomposition. Sub-project
#2c's original scope — "add a WiFi AP + web form so a panel can be
commissioned without a serial connection" — turned out to hide genuine new
domain/adapter logic, not just composition, the same way sub-project #7
split into #7a/#7b. This spec covers that hidden piece: how the panel
decides which boot mode to enter, and how an already-commissioned panel
re-enters wireless setup without any physical trigger hardware. The actual
web form, captive-portal HTTP/DNS server, and `src/esp32/main.cpp` wiring
are sub-project #2c-b.

## Context

Sub-project 2a's design explicitly scoped both bench-serial (built, merged)
and wireless captive-portal (deferred to this slice) commissioning as wanted
for v1. The sibling project `../MaltbeeTurnoutController` already has a
working captive-portal implementation (`lib/McsCore/src/adapters/
CaptivePortalServer.h`, `WebFormCommissioningAdapter.h`,
`domain/SetupFormRenderer.h`) that this project's "port the design, adapt
the code" reuse strategy (established in the 2a spec) can draw from — that
part is thin and reuses `CommissioningSession`/`CommandLineParser` almost
unchanged, and is #2c-b's job.

What doesn't port directly is the sibling's **trigger** for entering
wireless setup: it reuses its spare GPIO0 (BOOT) button, held for 3 seconds
then released, detected live during normal runtime polling (not a boot-time
strapping-pin read — GPIO0 held through power-on puts the ROM into UART
download mode before any application code runs, so "held through power-on"
can never be observed from `setup()`/`loop()` at all; see the sibling's
`ButtonSetupModeTrigger.h`). This project's ESP32 turnout panel has no
equivalent spare button: GPIO0 is explicitly on this project's own "pins
intentionally avoided" list (`docs/ESP32_Turnout_Panel_Implementation.md`),
and all 19 usable GPIOs are already spoken for by the 3×4 button matrix (7
GPIOs) and 12 LED pairs (12 GPIOs).

## Decisions (confirmed via Q&A)

1. **Split #2c into #2c-a (this spec) and #2c-b**, mirroring #7/#7a/#7b.
2. **Trigger mechanism: hold two matrix buttons (T1 + T2) simultaneously for
   3000 ms, then release** — reuses the existing 3×4 matrix scan, zero new
   hardware. T1/T2 chosen because they're adjacent (same row), easiest to
   press one-handed.
3. **The combo buttons are also live turnout controls** (the matrix's 12
   cells are all already wired to real turnouts — there's no spare unused
   cell), so holding them would otherwise also fire two ordinary
   `Button.wasPressed()` edges and send two live toggle commands to JMRI
   before the combo is even recognized. **Decision: suppress normal
   button handling for T1/T2 for the duration of the joint hold**, via a
   small reusable gating primitive, rather than accepting the spurious
   commands.

## Components

### `BootMode` (`lib/McsEsp32/src/domain/BootMode.h`)

```cpp
enum class BootMode
{
    Normal,
    NeedsCommissioning,
    WirelessSetup
};
```

Transient boot-time state — not part of persisted `NodeConfig`, computed
fresh every boot.

### `BootModeSelector` (`lib/McsEsp32/src/domain/BootModeSelector.h`)

```cpp
class BootModeSelector
{
public:
    static BootMode select(const NodeConfig& config, bool wirelessSetupRequested)
    {
        if (wirelessSetupRequested)
        {
            return BootMode::WirelessSetup;
        }
        return config.validate().empty() ? BootMode::Normal : BootMode::NeedsCommissioning;
    }
};
```

A pending request wins outright regardless of config validity (matches the
sibling exactly) — so a technician can re-enter wireless setup on an
already-valid, already-running panel, not just on a factory-fresh one.

### `SetupModeRequestStore` (port, `lib/McsEsp32/src/ports/SetupModeRequestStore.h`)

```cpp
class SetupModeRequestStore
{
public:
    virtual ~SetupModeRequestStore() = default;
    virtual void requestOnNextBoot() = 0;
    virtual bool consumeRequest() = 0;  // read-and-clear
};
```

`NvsSetupModeRequestStore` (adapter, `lib/McsEsp32/src/adapters/
NvsSetupModeRequestStore.h`/`.cpp`, `#ifdef ARDUINO`-guarded) persists the
flag via ESP32 `Preferences` in its own namespace (`"mcs-boot"`, key
`"wsetup"`) — deliberately separate from `NvsConfigStore`'s configuration
namespace, since this is a one-shot boot-intent flag, not configuration.
`consumeRequest()` clears the flag as part of reading it, so a stale
request can never re-trigger on a later boot. `FakeSetupModeRequestStore`
(test double, `test/support/`) is in-memory, mirroring `FakeConfigStore`'s
shape.

### `GatedDigitalInput` (`lib/McsEsp32/src/adapters/GatedDigitalInput.h`/`.cpp`)

```cpp
class GatedDigitalInput final : public DigitalInput
{
public:
    explicit GatedDigitalInput(DigitalInput& inner);

    void setSuppressed(bool suppressed);
    [[nodiscard]] bool isActive() const override;

private:
    DigitalInput& inner_;
    bool suppressed_ = false;
};
```

`isActive()` returns `false` whenever suppressed, otherwise forwards to
`inner_.isActive()`. This class knows nothing about matrix buttons, combos,
or setup mode — it's a general-purpose gating primitive, reusable anywhere
a `DigitalInput` needs to be temporarily masked. No `#ifdef ARDUINO` guard;
depends only on the `DigitalInput` port.

### `ComboSetupModeTrigger` (`lib/McsEsp32/src/adapters/ComboSetupModeTrigger.h`/`.cpp`)

```cpp
class ComboSetupModeTrigger
{
public:
    ComboSetupModeTrigger(DigitalInput& buttonA, DigitalInput& buttonB, Clock& clock,
                           unsigned long minHoldMs);

    void update();
    [[nodiscard]] bool isHolding() const;
    [[nodiscard]] bool requested() const;

private:
    DigitalInput& buttonA_;
    DigitalInput& buttonB_;
    Clock& clock_;
    unsigned long minHoldMs_;
    bool holding_ = false;
    unsigned long holdStartMs_ = 0;
    bool requestedThisTick_ = false;
};
```

`update()`: computes `bothActive = buttonA_.isActive() && buttonB_.isActive()`.
On the transition into `bothActive` (from not-holding), records
`holdStartMs_` and sets `holding_ = true` — the hold timer starts from the
*later* of the two presses, not the first one alone. On the transition out
of `bothActive` (from holding), sets `holding_ = false` and, if the elapsed
time met `minHoldMs_`, sets `requestedThisTick_ = true` for that one
`update()` call only (edge-triggered, mirrors `Button`'s own
`wasPressed()` semantics). Releasing *either* button ends the joint hold
and is evaluated the same way — the gesture doesn't require both to release
at the exact same instant.

`isHolding()` is the hook `main.cpp` (in #2c-b) will use to drive
`GatedDigitalInput::setSuppressed()` on the two `ToggleTurnoutStation`s'
button inputs each tick — this class has no reference to `GatedDigitalInput`
or to which turnouts are involved, keeping detection and gating
independently testable and composable only in the composition root.

No debounce layer: a 3-second hold threshold is insensitive to any
electrical bounce on the sub-millisecond scale, so a raw per-scan read is
sufficient (matches the sibling's own `ButtonSetupModeTrigger`, which also
reads its input directly rather than through a debounce layer).

## Testing

All five pieces are native-testable except `NvsSetupModeRequestStore`
(build-check only via `pio run -e esp32dev`, same convention as
`NvsConfigStore`):

- **`BootModeSelector`**: a pending request returns `WirelessSetup`
  regardless of config validity (test both a valid and an invalid config
  with the flag set); no request + valid config → `Normal`; no request +
  invalid config → `NeedsCommissioning`.
- **`GatedDigitalInput`**: forwards `true`/`false` from the inner
  `FakeDigitalInput` when not suppressed; returns `false` when suppressed
  regardless of the inner reading; forwarding resumes correctly after
  `setSuppressed(false)`.
- **`ComboSetupModeTrigger`** (via `FakeDigitalInput`×2 + `FakeClock`):
  `isHolding()` is false initially and while only one input is active;
  becomes true once both are active; `requested()` stays false if released
  before `minHoldMs`; `requested()` fires exactly once on the release tick
  after meeting `minHoldMs`, then reads false again on the next `update()`;
  releasing only one of the two (other still held) still ends the joint
  hold and evaluates `requested()` the same way; a fresh press-hold-release
  cycle after a full release can trigger again.

## File layout

- `lib/McsEsp32/src/domain/`: `BootMode.h`, `BootModeSelector.h`
- `lib/McsEsp32/src/ports/`: `SetupModeRequestStore.h`
- `lib/McsEsp32/src/adapters/`: `NvsSetupModeRequestStore.h`/`.cpp`
  (guarded), `GatedDigitalInput.h`/`.cpp`, `ComboSetupModeTrigger.h`/`.cpp`
- `test/support/`: `FakeSetupModeRequestStore.h`
- `test/test_boot_mode_selector/`, `test/test_gated_digital_input/`,
  `test/test_combo_setup_mode_trigger/`

## Non-goals

- Wiring any of this into `src/esp32/main.cpp` — the actual `BootMode`
  selection at boot, constructing `ComboSetupModeTrigger` against the real
  T1/T2 `MatrixDigitalInput`s, wrapping those two stations' button inputs in
  `GatedDigitalInput`, and calling `ESP.restart()` on a satisfied request —
  is entirely #2c-b's job.
- The web form, `SetupFormRenderer`, `CaptivePortalServer`, and
  `WebFormCommissioningAdapter` — also #2c-b.
- Any change to `Button`, `MatrixScanner`, `MatrixDigitalInput`,
  `ToggleTurnoutStation`, or `ToggleTurnoutControl` — all reused completely
  unmodified. `GatedDigitalInput` sits *between* a `MatrixDigitalInput` and
  the `Button` a `ToggleTurnoutStation` owns, without either of those
  classes needing to know it exists.
