# Conditional Feature-Compilation Flags

This is the complete list of conditional-compilation feature flags available in the
firmware. `EFI_*` flags are defined in the per-MCU-family `config/.../efifeatures.h`
headers (base: `config/stm32f4ems/efifeatures.h`, with `stm32f7ems` / `stm32h7ems`
overriding selected values). `MODULE_*` flags are defined by each pluggable module's
`controllers/modules/<name>/<name>.mk`.

A board overrides any flag from its `board.mk` with e.g. `DDEFS += -DEFI_EXHAUST_CUTOUT=FALSE`.

> Build-mode flags `EFI_PROD_CODE` / `EFI_SIMULATOR` / `EFI_UNIT_TEST` are mutually
> exclusive and set by the build, not by `efifeatures.h`.

## How to enable / disable a flag

Flags are booleans set to `TRUE` or `FALSE`. The effective value comes from the first
place that defines it, in this order of precedence:

1. **Per-board override (preferred)** — add a `-D` to the board's `board.mk`. This wins
   because it is defined on the compiler command line before any header runs:
   ```make
   # config/boards/<your-board>/board.mk
   DDEFS += -DEFI_DOWNSHIFT_BLIPPER=FALSE   # disable on this board
   DDEFS += -DEFI_ADVANCED_FUEL_PUMP=TRUE   # enable an opt-in feature on this board
   ```
   After editing `board.mk`, rebuild that board (e.g. `cd config/boards/<board> && ./compile_*.sh`).

2. **Family / base default** — change the value in the MCU-family header. The base is
   `config/stm32f4ems/efifeatures.h`; `stm32f7ems` and `stm32h7ems` include it and may
   override selected flags. These use the guard pattern below, so a board `-D` still wins:
   ```c
   #ifndef EFI_EXHAUST_CUTOUT
   #define EFI_EXHAUST_CUTOUT TRUE
   #endif
   ```

3. **Unit tests / simulator** — set the value in `unit_tests/efifeatures.h` or
   `simulator/simulator/efifeatures.h` (these are standalone, they do not include the
   family header).

In code, a flag gates compilation with the preprocessor:
```c
#if EFI_DOWNSHIFT_BLIPPER
    // feature implementation
#else
    // stub / no-op
#endif
```
Disabling a flag removes that feature's code from the build (LTO strips the rest).
The feature's TS page-5 config fields are still generated regardless — they are just
unused on a board where the feature is off.

> Tip: to verify a board still builds with a feature off, set the `-D...=FALSE` and compile.

## AlphaX custom subsystems (config lives in TS page 6)

> **Convention for new features on this branch.** Every new AlphaX feature added on this
> branch MUST:
> 1. Put its tuner-adjustable config on **TS page 6** (`config_page_6.txt`), not the main
>    config page — page 6 is the dedicated home for AlphaX custom features.
> 2. Ship with its own `EFI_<FEATURE>` conditional-compilation flag, declared in the MCU
>    family headers using the `#ifndef` guard pattern so it can be enabled/disabled
>    per board from `board.mk` (`-DEFI_<FEATURE>=TRUE|FALSE`).
> 3. Gate all of the feature's code behind `#if EFI_<FEATURE>` with a stub/no-op `#else`,
>    so a board can compile the feature out entirely (LTO strips the rest).
>
> Follow the F4-off / F7+H7-on default pattern below unless there's a reason not to, and
> add the new flag to both the table here and the "All flags" list.

These gate the AlphaX custom features. All of them are **FALSE** in the f4ems base
(F4 has limited flash) and **TRUE** on f7/h7. A board may opt in on F4 with
`-DEFI_<FEATURE>=TRUE` in its `board.mk`.

| Flag | Subsystem | Default |
|---|---|---|
| `EFI_DOWNSHIFT_BLIPPER` | Downshift blipper (also needs `EFI_ELECTRONIC_THROTTLE_BODY`) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_UPSHIFT_RPM_HOLD` | Upshift RPM hold (also needs `EFI_ELECTRONIC_THROTTLE_BODY`) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_EXHAUST_CUTOUT` | Exhaust cutout | FALSE (f4) / TRUE (f7,h7) |
| `EFI_ENGINE_STATE_MACHINE` | Engine state machine + pops & bangs | FALSE (f4) / TRUE (f7,h7) |
| `EFI_MISFIRE_DETECTION` | Idle misfire detection (ESM sub-feature; needs `EFI_ENGINE_STATE_MACHINE` at runtime) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_CLUTCH_DELAY_VALVE` | Clutch delay valve (CDV) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_LAUNCH_POWER_RAMP` | Launch power ramp (timing pull + ramp-in after a WOT launch release) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_ROLLING_LAUNCH` | Rolling Launch Control (button-held captured-RPM hold via the hard spark limiter to spool the turbo while moving; flat timing/fuel while held, timing ramp-out on release; needs `EFI_LAUNCH_CONTROL`) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_BURST_KNOCK` | Burst knock (transient ignition timing pull on a TPS-rate stab, decays over time) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_WOT_ENRICHMENT` | WOT time enrichment (AFR adder applied to the target after prolonged WOT; needs `EFI_ENGINE_STATE_MACHINE` at runtime for WOT state) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_SPORT_PEDAL` | Sport Pedal (ETB pedal-to-throttle ratio shaping via a pedal-indexed multiplier curve; switch/Lua-gauge activated; needs `EFI_ELECTRONIC_THROTTLE_BODY`) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_AC_PRESSURE_FAN` | AC Pressure Fan Control (per-fan pressure on/off thresholds with hysteresis replace the simple "enable with A/C" toggle when a high-side pressure sensor is installed) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_OFF_IDLE_RPM_ADDER` | Off-idle RPM adder | FALSE (f4) / TRUE (f7,h7) |
| `EFI_LUA_LIMITER` | Lua limiter | FALSE (f4) / TRUE (f7,h7) |
| `EFI_ADVANCED_FUEL_PUMP` | Advanced / PWM secondary fuel pump | FALSE (f4) / TRUE (f7,h7) |
| `EFI_VVT_COMPENSATION` | VVT timing/fuel compensation | FALSE (f4) / TRUE (f7,h7) |
| `EFI_VVT_ADVANCED_MODE` | VVT Advanced Mode (distance-from-target + oil-pressure duty curves replace the fixed PID Hold Duty offset, with an optional near-target PID pause) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_CHECK_ENGINE_TRIGGERING` | Check Engine Triggering (TS-configurable threshold checks with a points-gated CEL; TPS Stuck High/Low implemented, others reserved) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_CHT_CLT_ESTIMATOR` | CLT Estimator (estimates coolant temp from CHT via a competing-rate radiator model, with a lagged thermostat valve simulating the crossing/dip/recover hunting behavior) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_OIL_LIFE_MONITOR` | Weighted Engine Oil Life Monitor (temperature-weighted revolution counter, oil-life % gauge; accumulates in RAM only and flushes to flash exactly once on ignition-off; **requires `EFI_MAIN_RELAY_CONTROL`** — a build error if the flag is on without it, and consequently not enabled in the simulator, which sets `EFI_MAIN_RELAY_CONTROL FALSE`) | FALSE (f4) / TRUE (f7,h7) |
| `EFI_CRANKING_NO_SPARK` | Cranking No-Spark (suppresses ECU-scheduled coil dwell/charge entirely while `isCranking()`, for engines with a distributor/module that fires spark on its own during crank; normal ECU spark control resumes as soon as cranking ends, reusing the existing Cranking RPM threshold — no separate threshold field. Fuel/everything else unaffected. Cut via `LimpManager`/`ClearReason::CrankingNoSpark`, same mechanism as `kickStartCranking`) | FALSE (f4) / TRUE (f7,h7) |

**Exception to the table above:** `EFI_WHEEL_SPEED_SENSORS` (Main Speed Sensor: a Source
selector — Output Shaft Speed / Front Axle / Rear Axle — that is the **sole** source of
`SensorType::VehicleSpeed` on every board; there is no other mechanism, the legacy pin/CAN VSS
code has been deleted entirely. Each source is independently Physical-pin or CAN/Lua, with a
shared percentage-based glitch filter and a configurable Wheel Slip Ratio Source1/Source2
selector) is **TRUE everywhere by default, including F4** — not the usual family-conditional
FALSE-f4/TRUE-f7h7 pattern, and not opt-in either. It's a normal default-TRUE, opt-out flag, same
pattern as `EFI_TOOTH_LOGGER`: any board that needs to disable it (eg. it no longer fits in flash)
overrides with `DDEFS += -DEFI_WHEEL_SPEED_SENSORS=FALSE` in its own `board.mk`, same as
`hellen/small-can-board` does for `EFI_TOOTH_LOGGER`.

## All flags

```
EFI_AC_PRESSURE_FAN              ← AlphaX (AC pressure-based fan on/off hysteresis)
EFI_ACTIVE_CONFIGURATION_IN_FLASH
EFI_ADVANCED_FUEL_PUMP            ← AlphaX (PWM/secondary fuel pump)
EFI_ALTERNATOR_CONTROL
EFI_ANALOG_SENSORS
EFI_ANTILAG_SYSTEM
EFI_BACKUP_SRAM
EFI_BLUETOOTH_SETUP
EFI_BOOST_CONTROL
EFI_BOR_LEVEL
EFI_BOSCH_YAW
EFI_BURST_KNOCK                  ← AlphaX (burst knock transient timing pull)
EFI_WOT_ENRICHMENT               ← AlphaX (WOT time AFR enrichment)
EFI_CAN_GPIO
EFI_CAN_SERIAL
EFI_CAN_SUPPORT
EFI_CDM_INTEGRATION
EFI_CHECK_ENGINE_TRIGGERING      ← AlphaX (Check Engine Triggering)
EFI_CHT_CLT_ESTIMATOR             ← AlphaX (CLT estimator from CHT)
EFI_CLI_SUPPORT
EFI_CLOCK_LOCKS
EFI_CLUTCH_DELAY_VALVE           ← AlphaX (CDV)
EFI_CONSOLE_AF
EFI_CONSOLE_RX_BRAIN_PIN
EFI_CONSOLE_RX_BRAIN_PIN_MODE
EFI_CONSOLE_TX_BRAIN_PIN
EFI_CONSOLE_TX_BRAIN_PIN_MODE
EFI_CONSOLE_USB_DEVICE
EFI_CRANKING_NO_SPARK            ← AlphaX (cranking no-spark, external ignition module)
EFI_CUSTOM_PANIC_METHOD
EFI_DAC
EFI_DETAILED_LOGGING
EFI_DFU_JUMP
EFI_DOWNSHIFT_BLIPPER            ← AlphaX (downshift blipper)
EFI_DYNO_VIEW
EFI_ELECTRONIC_THROTTLE_BODY
EFI_EMBED_INI_MSD
EFI_EMULATE_POSITION_SENSORS
EFI_ENABLE_ASSERTS
EFI_ENGINE_CONTROL
EFI_ENGINE_EMULATOR
EFI_ENGINE_SNIFFER
EFI_ENGINE_STATE_MACHINE         ← AlphaX (ESM + pops & bangs)
EFI_ETHERNET
EFI_EXHAUST_CUTOUT               ← AlphaX (exhaust cutout)
EFI_FILE_LOGGING
EFI_GPIO_HARDWARE
EFI_HD_ACR
EFI_HELLA_OIL
EFI_HISTOGRAMS
EFI_HPFP
EFI_IDLE_CONTROL
EFI_IDLE_PID_CIC
EFI_INTERNAL_ADC
EFI_INTERNAL_FAST_ADC_GPT
EFI_INTERNAL_FAST_ADC_PWM
EFI_INTERNAL_SLOW_ADC_BACKGROUND
EFI_LAUNCH_CONTROL
EFI_LAUNCH_POWER_RAMP            ← AlphaX (launch power ramp)
EFI_ROLLING_LAUNCH               ← AlphaX (rolling launch control)
EFI_LOGIC_ANALYZER
EFI_LTFT_CONTROL
EFI_LUA
EFI_LUA_LIMITER                  ← AlphaX (lua limiter)
EFI_LUA_LOOKUP
EFI_MAIN_RELAY_CONTROL
EFI_MALFUNCTION_INDICATOR
EFI_MAP_AVERAGING
EFI_MAX_31855
EFI_MC33816
EFI_MCP_3208
EFI_MISFIRE_DETECTION            ← AlphaX (idle misfire detection)
EFI_OFF_IDLE_RPM_ADDER           ← AlphaX (off-idle RPM adder)
EFI_OIL_LIFE_MONITOR              ← AlphaX (weighted oil life monitor, needs EFI_MAIN_RELAY_CONTROL)
EFI_ONBOARD_MEMS
EFI_PERF_METRICS
EFI_POTENTIOMETER
EFI_RTC
EFI_SENT_SUPPORT
EFI_SHAFT_POSITION_INPUT
EFI_SIGNAL_EXECUTOR_ONE_TIMER
EFI_SIGNAL_EXECUTOR_SLEEP
EFI_SPI1_AF
EFI_SPI2_AF
EFI_SPI3_AF
EFI_SPI4_AF
EFI_SPI5_AF
EFI_SPI6_AF
EFI_SPORT_PEDAL                  ← AlphaX (ETB sport pedal ratio shaping)
EFI_STORAGE_INT_FLASH
EFI_STORAGE_MFS
EFI_STORAGE_SD
EFI_SUPPORT_FATFS
EFI_TCU
EFI_TEXT_LOGGING
EFI_TOOTH_LOGGER
EFI_TS_SCATTER
EFI_TUNER_STUDIO
EFI_TUNER_STUDIO_VERBOSE
EFI_UART_ECHO_TEST_MODE
EFI_UART_GPS
EFI_UPSHIFT_RPM_HOLD             ← AlphaX (upshift RPM hold)
EFI_USB_SERIAL
EFI_USE_COMPRESSED_INI_MSD
EFI_USE_FAST_ADC
EFI_USE_OPENBLT
EFI_USE_UART_DMA
EFI_VEHICLE_SPEED
EFI_VVT_COMPENSATION             ← AlphaX (VVT compensation)
EFI_VVT_ADVANCED_MODE            ← AlphaX (VVT advanced-mode duty curves)
EFI_VVT_PID
EFI_WHEEL_SPEED_SENSORS         ← AlphaX (Main Speed Sensor / OSS / axle speed / wheel slip source; default TRUE everywhere, opt-out, see exception note above)
EFI_WIDEBAND_FIRMWARE_UPDATE
EFI_WIFI
EFI_WS2812
ROTATIONAL_IDLE_CONTROLLER
MODULE_CDV_CONTROLLER
MODULE_CONFIGURATION_WIZARD
MODULE_EXAMPLE_LIVEDATA
MODULE_FAN_CONTROL
MODULE_FUEL_PUMP
MODULE_GEAR_DETECTOR
MODULE_LOGGING
MODULE_MAP_AVERAGING
MODULE_ODOMETER
MODULE_TACHOMETER
MODULE_VVL_CONTROLLER
```

> Note: `MODULE_*` flags currently do not gate per-board on their own — every module's
> `.mk` is included unconditionally in `controllers/modules/modules.mk`. That is why the
> CDV feature toggle uses the dedicated `EFI_CLUTCH_DELAY_VALVE` flag rather than relying
> on `MODULE_CDV_CONTROLLER`.
