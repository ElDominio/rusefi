# Check Engine Light (MIL)

How rusEFI drives the check engine light / malfunction indicator lamp (MIL).

## The pin

- Config field: `malfunctionIndicatorPin` / `malfunctionIndicatorPinMode` (`firmware/integration/rusefi_config.txt:859-860`), set per-board in each board's `board_configuration.cpp` (e.g. `Gpio::Unassigned` on alphax-s550-pnp).
- Pin object: `enginePins.checkEnginePin` (`RegisteredOutputPin`, declared `firmware/controllers/system/efi_gpio.h:123`, bound to the `malfunctionIndicator` config offset in `efi_gpio.cpp:166`).
- `MILController` only runs if `isMilEnabled()` (i.e. `isBrainPinValid(malfunctionIndicatorPin)`) — boards without the pin assigned skip the whole subsystem.

There are two independent paths that toggle this pin.

## Path 1 — normal diagnostics (OBD codes, misfire, etc.)

1. Any subsystem calls `addError(ObdCode)` (`firmware/controllers/gauges/malfunction_central.cpp:33`), which inserts the code into a fixed-size, deduplicated `error_codes_set_s` (bounded by `MAX_ERROR_CODES_COUNT`).
2. `MILController` (`firmware/controllers/gauges/malfunction_indicator.cpp`), a `PeriodicController` ticking every 10ms:
   - Blips the pin once per tick whenever trigger sync is recent (`mostRecentSyncTime`).
   - Reads the active error set via `getErrorCodes()` and blinks out each code's digits through `DisplayErrorCode`/`blink_digits`, calling `enginePins.checkEnginePin.setValue(0/1)`.

This is the standard, recoverable-fault path — there is no dedicated "set MIL" function; everything funnels through `addError()`.

Example consumer: Misfire Detection. `MisfireController::registerMisfire()` (`firmware/controllers/algo/misfire_detection.cpp:68-77`) increments a per-cylinder count; once it crosses `getCustomPage()->misfireCountThreshold` it latches a persistent `misfireLatched` flag (survives engine stop) and calls `addError(ObdCode::OBD_Random_Misfire)` (P0300) — lighting the CEL purely via the shared `addError()` mechanism above.

Limp Mode does **not** call `addError`/MIL directly — it only reads severities via `obdCodeSeverity()` (`malfunction_central.cpp:52`, e.g. misfire severity = 5) to decide when to latch limp state. CEL illumination for those underlying faults still flows through whichever subsystem called `addError()`.

## Path 2 — fatal/critical errors

`firmwareError()` / `criticalError()` (`firmware/controllers/core/error_handling.h`/`.cpp`) → `criticalShutdown()` (macro: `TURN_FATAL_LED(); turnAllPinsOff();`) → `turnAllPinsOff()` (`firmware/controllers/system/efi_gpio.cpp:871`), which explicitly forces `enginePins.checkEnginePin.setValue(true)` — the one pin deliberately left ON while every other output is shut off.

## Summary

| Trigger | Mechanism | CEL behavior |
|---|---|---|
| Recoverable diagnostic fault | `addError(ObdCode::...)` | Picked up by `MILController`, blinks out the OBD code digits |
| Misfire latch | `addError(ObdCode::OBD_Random_Misfire)` from `MisfireController` | Same as above (P0300) |
| Fatal/unrecoverable error | `firmwareError()` / `criticalError()` | `turnAllPinsOff()` forces CEL pin solid ON |

## Notes

- The legacy mainline scaffold `firmware/controllers/modules/check_engine_light/check_engine_config.txt` (`cel_battery_min_v`/`cel_map_min_v`/`cel_iat_min_v`/`cel_tps_min_v` + `_max_v`, raw-voltage bands on page 1, shared by every rusEFI board) is unrelated and untouched — `checkEngineDefaults()` remains its no-op consumer. Check Engine Triggering (below) is a separate, AlphaX-only mechanism on page 5, kept deliberately isolated so it can be cleanly ported upstream later without dragging that legacy scaffold along.

## Check Engine Triggering (`EFI_CHECK_ENGINE_TRIGGERING`)

A module (`firmware/controllers/modules/check_engine_light/check_engine_light.{h,cpp}`, class `CheckEngineTriggering`, a 20Hz `EngineModule::onSlowCallback()`) that lets the tuner configure, from TunerStudio (TS page 5, `config_page_5.txt`), a fixed set of named threshold checks that can light the CEL — independent of (and in addition to) the existing Path 1 `addError()` callers described above.

### Why this is a separate mechanism from Path 1

`MILController` lights/blinks the CEL the instant *any* code is in the active error set — there is no severity gating on the lamp itself (only `getErrorSeverityTotal()` / `limpSeverityThreshold` gates Limp Mode latching, a different consumer of the same error set). Check Engine Triggering adds a **points-gated lamp** specifically for its own checks:

- Each check has its own enable bit and a tuner-configurable **points** weight (separate from the fixed, firmware-side `obdCodeSeverity()` table used for Limp Mode — these points are TS-adjustable per check, not hardcoded).
- `CheckEngineTriggering::onSlowCallback()` sums the points of all *currently tripped, enabled* checks each cycle into `celPointsTotal` (live data).
- Two global page-5 fields gate the lamp from that running total, mirroring the `limpSeverityThreshold` precedent:
  - `celBlinkPointsThreshold` — total points at/above this sets the `isCelPreWarning` live-data bit. `MILController` (`malfunction_indicator.cpp`) polls `isCelPreWarningActive()` each tick and, if no real error code is already active, blinks the pin directly (200ms on/off) as a soft pre-warning — no DTC is stored.
  - `celPointsThreshold` — total points at/above this calls `addError()` (and `removeError()` once no longer tripped) for every currently-tripped check's code, handing off to the existing Path 1 `MILController` digit-blink-out behavior.
  - Either threshold set to 0 disables that stage.
- Other `addError()` callers (misfire, etc.) are unaffected — they keep lighting the CEL immediately as today.

### Checks (fixed named fields on page 5, per [[AlphaX page-5 feature convention]])

| Check | Status | Fields (page 5) | OBD code(s) |
|---|---|---|---|
| TPS Stuck High/Low | **Implemented** | `tpsStuckCelEnable`, `tpsStuckCelAutoClear`, `tpsStuckHighThreshold`, `tpsStuckLowThreshold`, `tpsStuckCelTimeoutSec` (autoscale), `tpsStuckCelPoints` | `OBD_Throttle_Actuator_Stuck_Open` (P2111, stuck high), `OBD_Throttle_Actuator_Stuck_Closed` (P2112, stuck low) — activated in `obd_error_codes.h` |
| Oil pressure low | Reserved (enable + points only, not yet evaluated) | `oilPressureLowCelEnable`, `oilPressureLowCelPoints` | `P0524` Engine Oil Pressure Too Low (still commented in `obd_error_codes.h`) |
| Coolant temp high | Reserved (enable + points only, not yet evaluated) | `cltHighCelEnable`, `cltHighCelPoints` | `P0217` Engine Overtemp Condition (still commented) |
| AFR / lambda lean or rich | Reserved (enable + points only, not yet evaluated) | `afrCelEnable`, `afrCelPoints` | `P0171`/`P0172` System Too Lean/Rich, Bank 1 (still commented) |
| Battery/system voltage low or high | Reserved (enable + points only, not yet evaluated) | `voltageCelEnable`, `voltageCelPoints` | `OBD_System_Voltage_Low` (active, 562) / `P0563` System Voltage High (still commented) |

Plus two global fields: `celPointsThreshold` and `celBlinkPointsThreshold` (see above).

The four reserved checks only get their enable bit + points field added now — their check-specific threshold value fields (oil pressure kPa, CLT °C, AFR band, voltage band) are deliberately **not** pre-allocated, since guessing the wrong type/units now would just mean another page-5 layout bump later; they'll be added together with each check's evaluation logic.

### TPS Stuck High/Low — detection logic

Requires an Electronic Throttle Body (compares actual TPS against driver pedal intent, `SensorType::Tps1` vs `SensorType::AcceleratorPedal`):

- **Stuck high**: TPS ≥ `tpsStuckHighThreshold` while pedal ≤ `tpsStuckLowThreshold` (throttle staying open with no driver request).
- **Stuck low**: TPS ≤ `tpsStuckLowThreshold` while pedal ≥ `tpsStuckHighThreshold` (throttle not responding to driver input).
- Each direction (high/low) uses one `Timer`, run both ways: the timer resets whenever the gate condition flips, and the flag only follows the gate once `tpsStuckCelTimeoutSec` has passed since the last flip. So tripping *and* untripping both require the same continuous timeout — a momentary blip at the edge doesn't chatter the flag either direction.
- Untripping is additionally gated by `tpsStuckCelAutoClear`: enabled (default) self-heals once the gate has been continuously clear for the timeout; disabled means a tripped code latches until power cycle, matching the existing Misfire Detection latch behavior.
- This intentionally never fires at idle (pedal and TPS both low) or at WOT (both high) — the two thresholds gate against *each other*, not just an absolute TPS rail.
- Live data (`check_engine_light_state.txt` → `check_engine_light_state_s`): `isTpsStuckHigh`, `isTpsStuckLow`, `isCelPreWarning`, `celPointsTotal`.

### Versioning

Adding these page-5 fields bumped `PAGE5_DATA_VERSION` (`custom_page.cpp`) and `FLASH_DATA_VERSION` (`rusefi_config.txt`) — see [[project_flash_data_version_bump]].
