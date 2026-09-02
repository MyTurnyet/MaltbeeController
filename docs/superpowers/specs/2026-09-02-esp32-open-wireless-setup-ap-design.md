# ESP32 Open Wireless-Setup AP — Design

## Context

The wireless-setup access point (`CaptivePortalServer`, opened when a
panel enters `BootMode::WirelessSetup`) is currently protected by a fixed
WPA2 passphrase (`maltbee-setup`, `src/esp32/main.cpp`) shared across
every panel. The passphrase adds no real security on top of the physical
gate that already exists — a technician must hold the panel's BOOT button
for 3 seconds to open the AP in the first place — while adding a shared
secret that has to be remembered and distributed for field commissioning.

## Decision

Remove the passphrase requirement entirely. `WiFi.softAP()` is called
with only the AP name, which the ESP32 Arduino core treats as an open
(unencrypted) network. Physical access to the panel (the BOOT-button
hold) remains the only gate to opening the AP; once it's open, anyone in
WiFi range can join it, same as anyone standing at the panel already
could plug in a USB cable for bench-serial commissioning.

**Known, accepted tradeoff (raised during design, not a reason to
change scope):** wireless setup mode has no timeout — the AP stays open
indefinitely until someone submits the setup form (which reboots the
panel). Removing the passphrase means an abandoned mid-commissioning
panel is open to anyone in range for as long as it's left that way, not
just for the few minutes a normal commissioning pass takes. This is
pre-existing behavior (the lack of a timeout) that this change makes
somewhat more consequential, not a new gap introduced by it.

## Components

### `CaptivePortalServer::begin()` (`lib/McsEsp32/src/adapters/CaptivePortalServer.h`/`.cpp`)

Drop the `passphrase` parameter:

```cpp
// Before
void begin(const std::string& apName, const std::string& passphrase);
// ...
WiFi.softAP(apName.c_str(), passphrase.c_str());

// After
void begin(const std::string& apName);
// ...
WiFi.softAP(apName.c_str());
```

No native test exists for this class today (`#ifdef ARDUINO`-guarded
hardware shim, verified only via `pio run -e esp32dev`) — this change
doesn't alter that convention.

### `src/esp32/main.cpp`

Remove the `WIRELESS_SETUP_AP_PASSPHRASE` constant and update the one
call site:

```cpp
// Before
constexpr const char* WIRELESS_SETUP_AP_PASSPHRASE = "maltbee-setup";
// ...
captivePortalServer.begin(apName, WIRELESS_SETUP_AP_PASSPHRASE);

// After
captivePortalServer.begin(apName);
```

## Documentation

- `docs/ESP32_Turnout_Panel_Implementation.md`'s "Wireless Setup Access
  Point" section: remove the passphrase-entry instructions and state the
  AP is open (no password required to join).
- `docs/HARDWARE_BRINGUP_CHECKLIST.md` section 2.4, step 3: remove the
  "join it using the passphrase" instruction.

## Testing

Build-check only (`pio run -e esp32dev`, expect `SUCCESS`) and a full
`pio test -e native` run to confirm no regression — matches this class's
existing convention (no dedicated native test).

## Non-goals

- Adding a timeout to wireless setup mode (the tradeoff noted above is
  accepted, not solved, by this change).
- Any change to bench-serial commissioning, `NvsConfigStore`,
  `WebFormCommissioningAdapter`, or any other commissioning path.
- Per-panel or MAC-derived AP naming/security — out of scope, unrelated
  to this change.
