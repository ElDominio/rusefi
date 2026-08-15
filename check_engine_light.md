# Check Engine Light (MIL)

How rusEFI drives the check engine light / malfunction indicator lamp (MIL).

## The pin

- Config field: `malfunctionIndicatorPin` / `malfunctionIndicatorPinMode` (`firmware/integration/rusefi_config.txt`), set per-board in each board's `board_configuration.cpp` (e.g. `Gpio::Unassigned` on alphax-s550-pnp).
- Pin object: `enginePins.checkEnginePin` (`RegisteredOutputPin`, declared `firmware/controllers/system/efi_gpio.h`). `setValue()` on an unassigned pin is a safe no-op, so boards without the pin wired simply don't drive anything — there is no separate enable gate.

There are two independent paths that toggle this pin.

## Path 1 — normal diagnostics (OBD codes, misfire, voltage faults, etc.)

1. Any subsystem calls `addError(ObdCode)` (`firmware/controllers/gauges/malfunction_central.cpp`), which inserts the code into a fixed-size, deduplicated `error_codes_set_s` (bounded by `MAX_ERROR_CODES_COUNT`).
2. `MILController` (`firmware/controllers/modules/malfunction_indicator/malfunction_indicator.{h,cpp}`, `MODULE_MIL`, defaults to `yes` for every board — see `firmware/controllers/modules/modules.mk`), a non-blocking `EngineModule::onSlowCallback()` state machine (Idle/Pulse/Gap phases, no `chThdSleepMilliseconds` blocking):
   - Reads the active error set via `getErrorCodes()`/`hasErrorCodes()` and blinks out each code's digits (short pulse per digit, long pulse between digit groups), cycling through every currently-active code in turn.
   - Key-On-Engine-Off (KOEO) bulb check: when there are no active codes and the engine is stopped, lights the CEL solid if `getCustomPage()->celOnKoeo` is enabled (defaults `true` — see custom_page.cpp).
   - Yields to the bench-test override (`getOutputOnTheBenchTest()`) and resets its digit-blink state cleanly when a bench test releases the pin.

This is the standard, recoverable-fault path — there is no dedicated "set MIL" function; everything funnels through `addError()`.

Example consumer: Misfire Detection. `MisfireController::registerMisfire()` (`firmware/controllers/algo/misfire_detection.cpp`) increments a per-cylinder count; once it crosses `getCustomPage()->misfireCountThreshold` it latches a persistent `misfireLatched` flag (survives engine stop) and calls `addError(ObdCode::OBD_Random_Misfire)` (P0300) — lighting the CEL purely via the shared `addError()` mechanism above. It also contributes a point to Check Engine Triggering's `celPointsTotal` (below).

## Path 2 — fatal/critical errors

`firmwareError()` / `criticalError()` (`firmware/controllers/core/error_handling.h`/`.cpp`) → `criticalShutdown()` (macro: `TURN_FATAL_LED(); turnAllPinsOff();`) → `turnAllPinsOff()` (`firmware/controllers/system/efi_gpio.cpp`), which explicitly forces `enginePins.checkEnginePin.setValue(true)` — the one pin deliberately left ON while every other output is shut off.

## Summary

| Trigger | Mechanism | CEL behavior |
|---|---|---|
| Recoverable diagnostic fault | `addError(ObdCode::...)` | Picked up by `MILController`, blinks out the OBD code digits |
| Misfire latch | `addError(ObdCode::OBD_Random_Misfire)` from `MisfireController` | Same as above (P0300); also contributes a point to `celPointsTotal` |
| Voltage out of configured range (battery/MAP/IAT/TPS) | `CheckEngineLight::updateRange()` -> `addError()` | Same as above; also contributes a point to `celPointsTotal` |
| Check Engine Triggering points reach `celBlinkPointsThreshold` | `isCelBlinkingActive()` | `MILController` flashes the pin plainly (200ms on/off) instead of the normal digit-blink-out |
| Fatal/unrecoverable error | `firmwareError()` / `criticalError()` | `turnAllPinsOff()` forces CEL pin solid ON |

## CheckEngineLight (`MODULE_CHECK_ENGINE_LIGHT`)

A single module (`firmware/controllers/modules/check_engine_light/check_engine_light.{h,cpp}`, class `CheckEngineLight`, a 20Hz `EngineModule::onSlowCallback()`, defaults `yes` for every board) that combines two kinds of checks:

1. **Direct voltage-range checks** — battery, MAP, IAT, TPS (raw `Tps1Primary` ADC voltage). Each has its own `RangeState` (active/pending code + debounce timer via `updateRange()`) and raises/clears its DTC (`OBD_System_Voltage_Low/Malfunction`, `OBD_Map_Low/High`, `OBD_Iat_Low/High`, `OBD_TPS1_Primary_Low/High`) **immediately and unconditionally** once out of the configured min/max (`cel_battery_min_v`/`_max_v` etc., page-1 fields, `setDefaultConfiguration()`). This behavior predates and is independent of the points system below.
2. **Points-gated "Check Engine Triggering" checks** — TS-configurable checks on page 6 (`tpsCircuitCelEnable`, `tpsIntermittentCelEnable`, `celDebounceTimeSec`, etc.), each debounced via the shared `DebouncedGate` primitive against `celDebounceTimeSec`. Currently implemented: TPS Circuit Low/High (re-reads `Sensor::get(Tps1)` for `UnexpectedCode::Low/High`) and TPS Circuit Intermittent (counts ok/fault flips in a ring buffer). Reserved-but-not-yet-evaluated: oil pressure low, coolant temp high, AFR/lambda lean-or-rich (`oilPressureLowCelEnable`/`cltHighCelEnable`/`afrCelEnable`, enable bit only).

### The points system

**Every** currently-active check on `CheckEngineLight` — both kinds above, plus Misfire Detection's latch — contributes a flat **1 point** to `celPointsTotal` (live data), summed each `onSlowCallback()` tick. There is a single points system, not two: the voltage-range checks' own DTC-raising is unaffected by points (see above), but their *activity* still counts toward the total.

Two page-6 thresholds gate what the combined total does, `celPointsThreshold` lower than `celBlinkPointsThreshold`:

- `celPointsThreshold` (reached first) — raises the DTC(s) for the *points-gated* checks only (`OBD_TPS1_Range_Performance` for TPS Circuit Low/High, `OBD_TPS1_Intermittent` for TPS Circuit Intermittent). Voltage-range and misfire DTCs are unaffected — they already raised independently. This is the standard, less-urgent fault path.
- `celBlinkPointsThreshold` (reached second) — sets the `isCelBlinking` live-data bit. `MILController` polls `isCelBlinkingActive()` each tick and, when true, flashes the pin plainly instead of the normal digit-blink-out — a flashing MIL signals a more critical, actively-damaging condition than a steady one, per OBD-II convention.
- Either threshold set to 0 disables that stage.

`OBD_TPS1_Range_Performance` is deliberately a different code than `OBD_TPS1_Primary_Low`/`High` (used by the voltage-range check above for the raw `Tps1Primary` ADC channel) — the two checks would otherwise fight over the same DTC, since one's "no fault" branch would `removeError()` a code the other legitimately raised.

### Limp Mode

Limp Mode (`firmware/controllers/algo/engine_state_machine.cpp`) latches on an ETB jam (`LimpManager::reportEtbJammed`) **or** when `isCelBlinkingActive()` is true — i.e. the exact same points system above, gated by `#ifdef MODULE_CHECK_ENGINE_LIGHT`. There is no separate DTC-severity table or threshold for Limp Mode; it simply reuses the CEL's own flashing-escalation signal.

### Malfunction Indicator KOEO bulb check (`MODULE_MIL`)

Independent of the points system: when `getCustomPage()->celOnKoeo` is enabled (default), `MILController` lights the CEL solid while the engine is stopped and there are no active codes, mimicking the classic OBD-II bulb-check.
