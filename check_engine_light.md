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

- Each check has its own enable bit and is worth a flat **1 point** — this page is only for checks that don't already have their own severity/latch elsewhere. Anything that can set a DTC outside this page (Misfire Detection, etc.) keeps its own severity in its own subsystem (e.g. `obdCodeSeverity()` for Limp Mode) and does not feed `celPointsTotal`.
- `CheckEngineTriggering::onSlowCallback()` sums the points of all *currently tripped, enabled* checks each cycle into `celPointsTotal` (live data).
- `celDebounceTimeSec` is a single page-5 field shared by every check on this page: each check's raw trip condition must hold continuously for this long before `DebouncedGate::update()` reports tripped, and be continuously clear for the same duration before it reports untripped (`check_engine_light.h`). New checks should reuse `DebouncedGate` + `celDebounceTimeSec` rather than rolling their own timer.
- Two global page-5 fields gate the lamp from that running total, mirroring the `limpSeverityThreshold` precedent. `celPointsThreshold` is the **lower** of the two and `celBlinkPointsThreshold` must be set **higher** — escalation, not a pre-warning:
  - `celPointsThreshold` (reached first) — total points at/above this calls `addError()` (and `removeError()` once no longer tripped) for every currently-tripped check's code, handing off to the existing Path 1 `MILController` digit-blink-out behavior. This is the standard, less-urgent fault path: CEL lit, DTC stored.
  - `celBlinkPointsThreshold` (reached second, on top of an already-active DTC) — sets the `isCelBlinking` live-data bit. `MILController` (`malfunction_indicator.cpp`) polls `isCelBlinkingActive()` each tick and, when true, flashes the pin plainly (200ms on/off) *instead of* the normal digit-blink-out for that cycle. This matches OBD-II convention, where a **flashing MIL signals a more critical, actively-damaging condition than a steady one** — blink is more severe than solid, not a softer heads-up before it.
  - Either threshold set to 0 disables that stage.
- Other `addError()` callers (misfire, etc.) are unaffected — they keep lighting the CEL immediately as today.
- TunerStudio dialog: `Setup → Limits and protection → Check Engine Triggering` (`tunerstudio.template.ini`), with the live-data panel attached on the side. Misfire Detection's dialog is the model this follows.

### Checks (fixed named fields on page 5, per [[AlphaX page-5 feature convention]])

A TPS Stuck High/Low check was previously implemented here as a TPS-vs-pedal plausibility
comparison, but it duplicated the existing ETB jam detection (`EtbController::checkJam()` in
`electronic_throttle.cpp`, gated by `etbJamDetectThreshold`/`etbJamTimeout`, which already feeds
`LimpManager::reportEtbJammed()` — see [[project_limp_mode]]). It was removed and replaced by the
TPS Circuit checks below.

| Check | Status | Fields (page 5) | OBD code(s) |
|---|---|---|---|
| TPS Circuit Low/High | **Implemented** | `tpsCircuitCelEnable` | `OBD_TPS1_Primary_Low` (P0122), `OBD_TPS1_Primary_High` (P0123) — the same enum `sensor_checker.cpp` already uses for its `warning()`-only log; this check independently re-reads `Sensor::get(Tps1)` and additionally debounces+`addError()`s it |
| TPS Circuit Intermittent | **Implemented** | `tpsIntermittentCelEnable`, `tpsIntermittentFlipCount` | `OBD_TPS1_Intermittent` (P0124, newly added) |
| Oil pressure low | Reserved (enable only, not yet evaluated) | `oilPressureLowCelEnable` | `P0524` Engine Oil Pressure Too Low (still commented in `obd_error_codes.h`) |
| Coolant temp high | Reserved (enable only, not yet evaluated) | `cltHighCelEnable` | `P0217` Engine Overtemp Condition (still commented) |
| AFR / lambda lean or rich | Reserved (enable only, not yet evaluated) | `afrCelEnable` | `P0171`/`P0172` System Too Lean/Rich, Bank 1 (still commented) |
| Battery/system voltage low or high | Reserved (enable only, not yet evaluated) | `voltageCelEnable` | `OBD_System_Voltage_Low` (active, 562) / `P0563` System Voltage High (still commented) |

Plus three global fields: `celPointsThreshold`, `celBlinkPointsThreshold`, `celDebounceTimeSec` (see above). `P0121` (Range/Performance) is deliberately not part of this batch — that code is already in use as `OBD_TPS_Configuration` for ETB auto-calibrate failure (`firmwareError()`, the fatal Path 2, not a recoverable DTC), so it doesn't fit this points-gated mechanism. A genuinely new check/code would be needed if P0121 coverage is wanted later.

The four still-reserved checks only get their enable bit added now (no points field — every CET check is a flat 1 point per the rule above); their check-specific threshold value fields (oil pressure kPa, CLT °C, AFR band, voltage band) are deliberately **not** pre-allocated, since guessing the wrong type/units now would just mean another page-5 layout bump later; they'll be added together with each check's evaluation logic.

### TPS Circuit Low/High/Intermittent — detection logic

Requires `Sensor::hasSensor(SensorType::Tps1)`. Each tick, `CheckEngineTriggering::onSlowCallback()` reads `Sensor::get(Tps1)` itself (independent of `sensor_checker.cpp`, which keeps logging its own `warning()` unchanged):

- **TPS Circuit Low/High**: tripped when the raw result is invalid with `UnexpectedCode::Low`/`High`, debounced through a `DebouncedGate` against `celDebounceTimeSec` (continuous for that long to trip, continuously clear for that long to untrip).
- **TPS Circuit Intermittent**: every ok↔fault transition of the same TPS1 result pushes a timestamp into a 10-entry `Timer` ring buffer (`m_tps1FlipTimers`). Each tick, the number of entries still within the last `celDebounceTimeSec` is counted; if it's at/above `tpsIntermittentFlipCount`, that raw condition is fed through its own `DebouncedGate` (same shared `celDebounceTimeSec`) before tripping `isTpsIntermittent`.

Live data (`check_engine_light_state.txt` → `check_engine_light_state_s`): `isTpsCircuitLow`, `isTpsCircuitHigh`, `isTpsIntermittent`, `isCelBlinking`, `celPointsTotal`.

### Versioning

Adding these page-5 fields bumped `PAGE5_DATA_VERSION` (`custom_page.cpp`) and `FLASH_DATA_VERSION` (`rusefi_config.txt`) — see [[project_flash_data_version_bump]].
