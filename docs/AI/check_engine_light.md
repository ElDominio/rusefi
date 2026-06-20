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

- `firmware/controllers/modules/check_engine_light/check_engine_light.cpp` currently has an empty `checkEngineDefaults()` — a no-op placeholder, not part of the active control path.
