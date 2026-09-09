# BootModeIndicator HAL friction (step 5 input)

Recorded while adding `LedPairFlashIndicator` in MaltbeeControllerSystem
(step 2 of the shared WifiCommissioning plan). No ports were changed.

- This project's `Clock` is `nowMilliseconds() -> unsigned long`.
  MaltbeeTurnoutController's is `now() -> Instant`.
- This project's `DigitalOutput` is `set(bool)` / `isSet()`.
  MaltbeeTurnoutController's is `write(Level)`.
- `LedPairFlashIndicator` needed neither port. It composes
  `LedPairDriver` (this project's domain) only. Shared `McsHal` is
  not required for BootModeIndicator extraction.
