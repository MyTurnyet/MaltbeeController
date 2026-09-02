# ESP32 Open Wireless-Setup AP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the WPA2 passphrase from the ESP32 panel's wireless-setup access point, making it an open network — physical access to the panel (the BOOT-button hold that opens the AP) remains the only gate.

**Architecture:** Drop the `passphrase` parameter from `CaptivePortalServer::begin()` and call `WiFi.softAP()` with only the AP name (the ESP32 Arduino core treats that as an open network). Update the one call site in `src/esp32/main.cpp` and remove the now-unused passphrase constant. No other class changes.

**Tech Stack:** C++17, PlatformIO (`esp32dev` build-check — `CaptivePortalServer` has no native test, matching its existing convention).

## Global Constraints

- `CaptivePortalServer` remains an `#ifdef ARDUINO`-guarded hardware shim with no dedicated native test — this change doesn't alter that convention.
- No timeout is being added to wireless setup mode — that tradeoff is explicitly accepted, not addressed, by this plan (see the design spec's Non-goals).

---

### Task 1: Remove the AP passphrase from `CaptivePortalServer` and `main.cpp`

**Files:**
- Modify: `lib/McsEsp32/src/adapters/CaptivePortalServer.h`
- Modify: `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp`
- Modify: `src/esp32/main.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `CaptivePortalServer::begin(const std::string& apName)` — the one caller (`src/esp32/main.cpp`) is updated in this same task, so no other file needs to change.

No test — matches `CaptivePortalServer`'s existing convention (verified via `pio run -e esp32dev` build success).

- [ ] **Step 1: Drop the parameter from the header**

In `lib/McsEsp32/src/adapters/CaptivePortalServer.h`, find:

```cpp
    void begin(const std::string& apName, const std::string& passphrase);
```

Replace with:

```cpp
    void begin(const std::string& apName);
```

- [ ] **Step 2: Drop the parameter from the implementation**

In `lib/McsEsp32/src/adapters/CaptivePortalServer.cpp`, find:

```cpp
void CaptivePortalServer::begin(const std::string& apName, const std::string& passphrase)
{
    WiFi.softAP(apName.c_str(), passphrase.c_str());
```

Replace with:

```cpp
void CaptivePortalServer::begin(const std::string& apName)
{
    WiFi.softAP(apName.c_str());
```

- [ ] **Step 3: Remove the passphrase constant and update the call site**

In `src/esp32/main.cpp`, find:

```cpp
    constexpr unsigned long IDENTIFY_DURATION_MS = 10000;
    constexpr const char* WIRELESS_SETUP_AP_PASSPHRASE = "maltbee-setup";
```

Replace with:

```cpp
    constexpr unsigned long IDENTIFY_DURATION_MS = 10000;
```

(Note: the exact surrounding lines may differ slightly if the constant order has changed — if so, just remove the `WIRELESS_SETUP_AP_PASSPHRASE` line wherever it sits in that anonymous-namespace constant block.)

Then find:

```cpp
        captivePortalServer.begin(apName, WIRELESS_SETUP_AP_PASSPHRASE);
```

Replace with:

```cpp
        captivePortalServer.begin(apName);
```

- [ ] **Step 4: Build-check the ESP32 target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 5: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All 40 suites still pass (unaffected — none of these three files are part of the native build).

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/adapters/CaptivePortalServer.h lib/McsEsp32/src/adapters/CaptivePortalServer.cpp src/esp32/main.cpp
git commit -m "$(cat <<'EOF'
! F Remove passphrase from wireless-setup AP, making it open

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CGYUBEe91zFYStbvaxPD3c
EOF
)"
```

---

### Task 2: Update documentation

**Files:**
- Modify: `docs/ESP32_Turnout_Panel_Implementation.md`
- Modify: `docs/HARDWARE_BRINGUP_CHECKLIST.md`

No test — documentation only.

- [ ] **Step 1: `docs/ESP32_Turnout_Panel_Implementation.md` — remove the passphrase paragraph**

Find:

```
The AP requires a WPA2 passphrase to join: **`maltbee-setup`**. This is
fixed and shared across every panel (not per-panel or MAC-derived) — write
it down for field commissioning, since a technician without it cannot join
the AP to reach the setup web form.
```

Replace with:

```
The AP is **open** (no password required to join) — physical access to
the panel (the BOOT-button hold that opens it) is the only gate.
Wireless setup mode has no timeout, so an abandoned mid-commissioning
panel stays open to anyone in range until someone completes the form or
power-cycles the board; don't leave a panel in setup mode unattended any
longer than necessary.
```

- [ ] **Step 2: `docs/HARDWARE_BRINGUP_CHECKLIST.md` — drop the passphrase-entry instruction**

Find:

```
3. On a phone or laptop, look for a WiFi network named `MaltBee-Setup-XXXX`
   (last 4 hex digits of the chip's MAC). Join it using the passphrase
   `maltbee-setup` (documented in `docs/ESP32_Turnout_Panel_Implementation.md`'s
   "Wireless Setup Access Point" section).
```

Replace with:

```
3. On a phone or laptop, look for a WiFi network named `MaltBee-Setup-XXXX`
   (last 4 hex digits of the chip's MAC) and join it — no password
   required (documented in `docs/ESP32_Turnout_Panel_Implementation.md`'s
   "Wireless Setup Access Point" section).
```

- [ ] **Step 3: Commit**

```bash
git add docs/ESP32_Turnout_Panel_Implementation.md docs/HARDWARE_BRINGUP_CHECKLIST.md
git commit -m "$(cat <<'EOF'
. d Update docs for open wireless-setup AP

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CGYUBEe91zFYStbvaxPD3c
EOF
)"
```

---

## Self-review notes

- **Spec coverage:** the design spec's single Decision (drop the passphrase, open AP) is covered by Task 1; both documentation updates it calls for are covered by Task 2; the Non-goals (no timeout added, no other commissioning-path changes) are respected — neither task touches anything else.
- **Placeholder scan:** no TBD/TODO; every step has complete, concrete code or exact find/replace text.
- **Type consistency:** `CaptivePortalServer::begin(const std::string& apName)`'s new single-parameter signature is used identically in both the header (Step 1) and the one call site (Step 3) — no other file references this method.
