# Engine Protection System (LimpManager)

The `LimpManager` is the central authority for engine safety. It monitors engine conditions and determines when to inhibit injection, ignition, or electronic throttle (ETB).

## Core Concepts

### ClearReason
Every cut is associated with a `ClearReason` (defined in `limp_manager.h`). This reason is used for:
- Logging the cause of a cut.
- Informing the user via TunerStudio (displayed in the "Fuel/Ignition Cut Code" gauge).
- Internal logic (e.g., preventing spark cuts in certain conditions to avoid backfires).

### Transient vs. Permanent Cuts
- **Permanent Cuts**: Managed via `m_allowInjection`, etc. These usually trigger a fatal error or a state that requires a power cycle or specific recovery.
- **Transient Cuts**: Managed via `m_transientAllowInjection`, etc. These are evaluated every `onFastCallback` (e.g., RPM limit, Boost cut).

## Protection warnings

`BoostCut`, `InjectorDutyCycle`, `LambdaProtection`, and the latched ETB jam
(`EtbProblem`) report P0234, C9013, P0171, and P0638 respectively. A single
`LimpManager` timer limits this group to one warning per 30 seconds, including
when the active reason changes. The first warning can be reported immediately.
The limiter only gates diagnostics; cut decisions and recovery are unchanged.
ETB reporting runs from `updateState` so a jam remains eligible for reporting
after another warning consumed the interval, even if `reportEtbJammed` is not
called again.

## Oil pressure and boost cut details

- The running minimum-oil-pressure check (`minimumOilPressureBins`/`Values`) is deferred
  until the 5 s after-start window (`minOilPressureAfterStart`) is over, so cold oil can
  build pressure before the running check starts counting.
- An active boost fuel cut (`BoostCut`) is kept until a *valid* MAP reading falls below the
  resume pressure; an invalid MAP sample neither starts nor releases the cut.
- The boost cut limit includes the Lua limiter adder (`EFI_LUA_LIMITER`) before the
  comparison.

## Check Engine Light, limp and malfunction codes

- Codes live in `WarningCodeState` (`controllers/gauges/malfunction_central.{h,cpp}`):
  `MAX_STORED_ERROR_CODES_COUNT` (32) codes are stored, the first `MAX_ERROR_CODES_COUNT`
  (10) are reported. With SD logging (`EFI_FILE_LOGGING`) `PastCodeHistory`
  (`past_codes.*`, stored by `hw_layer/past_codes_storage.*`) keeps expired codes.
- `warning()` stores a code, prints a console line and takes a freeze frame (DtcManager). It
  is used by `LimpManager` cuts and by `MisfireController` for
  `OBD_Random_Misfire`. `addError()` stores the code only; the `CheckEngineLight` voltage range
  checks and TPS Range/Intermittent codes use it.
- `CheckEngineLight` sums points (TPS circuit low/high, TPS intermittent, active range DTCs,
  latched misfire). `celPointsThreshold` lights the lamp, `celBlinkPointsThreshold` makes
  `isCelBlinkingActive()` true.
- `MILController` blinks the stored codes with a 5 s steady prelude, 2 s between codes and
  finishes the code being blinked when it disappears. While `isCelBlinkingActive()` it flashes
  plainly at 200 ms instead ([tag:alphax_cel_blink]). The key-on lamp check with no codes
  requires `celOnKoeo`.
- Limp mode (`OperatingModes`) latches on an ETB jam or on `isCelBlinkingActive()`.
  There is no cumulative-severity trigger.

## How-To: Adding an Engine Protection Feature

To add a new protection feature (e.g., "Fuel Pressure Protection"), follow this 4-step pattern:

1.  **Define the Reason**: Add a new entry to the `ClearReason` enum in `limp_manager.h`.
    ```cpp
    enum class ClearReason : uint8_t {
        ...
        FuelPressure,
    };
    ```
2.  **Add Configuration**: Add thresholds and timeouts to `rusefi_config.txt`.
    ```text
    float fuelPressureThreshold;
    float fuelPressureTimeout;
    ```
3.  **Implement Detection**: Add the monitoring logic to `LimpManager::updateState` in `limp_manager.cpp`.
    ```cpp
    if (fuelPressure < config->fuelPressureThreshold) {
        m_fuelPressureTimer.reset(nowNt);
    }
    if (m_fuelPressureTimer.hasElapsedSec(config->fuelPressureTimeout)) {
        allowFuel.clear(ClearReason::FuelPressure);
    }
    ```
4.  **Expose to User**: Update `tunerstudio.template.ini` (usually handled by code generation, but verify the Gauge list) so the user can see why the engine stopped.

## Key Files
- `firmware/controllers/limp_manager.cpp`: Main protection logic.
- `firmware/controllers/shutdown_controller.cpp`: Logic for graceful vs. immediate engine stops.
