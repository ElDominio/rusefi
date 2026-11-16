# Engine state machine and shift recognition

`EngineStateMachine` in `controllers/modules/engine_state_machine` classifies
operating state at the slow-callback cadence. It is runtime-disabled by default.
Settings live on the main configuration page. The module uses the launch and
idle controllers, reports operating-state and shift flags, and optionally
selects STFT regions. Actuator behavior belongs to the features consuming those
flags. The AlphaX-only operating modes (Limp, Sport Mode, Eco Mode, Ghost Cam,
Sport Pedal, Quick Warmup, Pops and Bangs) live in the separate `OperatingModes`
module described in [Operating modes](#operating-modes) and layer on this one.

## Module layout

- `controllers/modules/engine_state_machine`: `EngineStateMachine`, its settings
  fragment (`engine_state_machine_engineConfiguration.txt`, the 20 `sm*` settings,
  `useEngineStateMachine` and `stateBasedStft` in `engineConfiguration`) and the
  `engine_state_machine_state_s` live data (`engineSm*` channels). The class is
  the upstream state machine with four patches, each marked
  `[tag:alphax_esm_overlay_hook]`: the `Limp = 12` and `Eco = 13` enumerators (the
  module never classifies them), `holdRateClassification()`, the traction-control
  term of `engineSmIsTorqueReduction`, and the `EFI_ENGINE_STATE_MACHINE` compile-out
  (the module stays in the list as a stub; defaults still apply).
- `controllers/modules/operating_modes`: `OperatingModes` and
  `operating_modes_state_s` (`engineSmIsLimp`, `engineSmIsPopsAndBangs`,
  `engineSmPnbSparkCut`, `engineSmIsEcoMode`, `engineSmIsQuickWarmup`,
  `engineSmIsGhostCam`, `engineSmIsSportPedal`, `engineSmIsSportMode`). Channel
  names are unchanged. Both modules sit in `AlphaXModuleList` in `engine.h`,
  `OperatingModes` after `EngineStateMachine`, so the state machine flags are
  current when the overlays run.
- Page 6 keeps the operating-mode settings (Sport Mode, Eco, Ghost Cam, Pops and
  Bangs, Quick Warmup, Limp, `smSlowStateTransitionEnabled`); the state machine,
  WOT enrichment and misfire settings are in `engineConfiguration`.

## Operating modes

`OperatingModes::onSlowCallback()` does nothing while the state machine is
disabled and clears its flags. Otherwise, in order: it latches Limp (ETB jam via
`LimpManager::reportEtbJammed`, or the Check Engine Light blinking), evaluates
Sport Mode (Limp out-votes it), Eco Mode (Limp and Sport Mode out-vote it), arms
the Eco settle hold, evaluates Ghost Cam (Idle only), steps the Eco and Ghost Cam
target blends, evaluates Sport Pedal, Quick Warmup (Cold and Idle), rewrites the
display state and finally runs the Pops and Bangs machine.

- Display priority in `engineSmCurrentState`: Limp, Upshifting, Downshifting,
  LaunchControl, Eco, then the primary state. The state machine ranks the middle
  three; `OperatingModes` places Limp first and Eco after them. Ghost Cam is not a
  display state.
- Eco settle hold: Eco's own throttle/VVT step is not driver input. On an Eco edge
  that was not itself caused by Accelerating/Decelerating, `OperatingModes` calls
  `EngineStateMachine::holdRateClassification()`, which suppresses the RPM-rate
  states for `smTransientHoldTimeMs`.
- Consumers of the overlay flags and blends (`getEcoModeBlend()`,
  `getGhostCamBlend()`, `getPopsAndBangsSparkSkipRatio()`) read `OperatingModes`;
  consumers of the primary flags (idle, WOT, shift, Decelerating) read the state
  machine. State-aware DFCO inhibits its cut during Pops and Bangs.

## Primary state and display overlays

The priority order is Off, Cranking, WOT, AE-driven Transient, RPM-rate
Accelerating/Decelerating, then IdleController phase. WOT uses
`DriverThrottleIntent > smWotTpsThreshold` (default 90%). Driver throttle intent
comes from the pedal on electronic-throttle setups and TPS on cable-throttle
setups. AE's acceleration or deceleration threshold requests Transient, held for
`smTransientHoldTimeMs` after the last qualifying sample.

IdleController phase maps CrankToIdleTaper to Afterstart and Idling to Idle.
Coasting maps to Overrun only with throttle below `coastingFuelCutTps`, RPM above
`coastingFuelCutRpmHigh`, and an engaged transmission. Otherwise it stays
Coasting. Running with closed throttle above a nonzero `maxIdleVss` also maps to
Coasting; the remaining case is Cruising. Without idle control, the phase
fallback is Running.

RPM rate is recomputed at `smRpmRateWindowMs`, floored to the 50 ms slow-callback
period, using actual elapsed time. The value holds between updates and is read
by classification on the next callback. Sampling runs independently of shift
recognition, including with no clutch selection and during flat-shift torque
reduction.
Zero acceleration/deceleration thresholds disable those respective states.
Acceleration can be held for `smAccelHoldMs`; a coasting phase breaks that hold.
Decelerating requires valid vehicle speed and a valid neutral/unknown
DetectedGear value of zero. Overrun checks configured clutch pins independently
of shift-direction selections. Selections also identify virtual switches, and
the second-switch option includes the complementary input. Either available
switch indicating disengagement prevents Overrun; with no inputs it defaults
to engaged.

`getCurrentState()` returns the primary state. `engineSmCurrentState` is the
display state: Upshifting overrides Downshifting, which overrides LaunchControl,
which overrides the primary state. Primary bits remain available during those
overlays. Flat-shift torque reduction provides an authoritative upshift and
torque-reduction flag. Temperature flags classify CLT below the cold threshold,
above the hot threshold, or in between; invalid CLT falls back to Operating.

## Shift recognition

Clutch Up is true with the pedal at rest; its falling edge opens a shift window.
Clutch Down is true at full press; its rising edge opens a window. Each
direction can select either switch or None. A single configured direction trusts
its switch; two configured directions require accumulated rate evidence. When
both share a switch, the accumulator sign selects the candidate direction.

The accumulator is bounded to -100..100. RPM mode builds positive evidence in
Accelerating and negative evidence while RPM falls. VSS mode integrates signed
vehicle-speed changes and requires valid VSS. In VSS mode, the numeric
acceleration/deceleration thresholds also serve as km/h/s normalization values;
primary-state classification continues to interpret them as RPM/s. Idle, launch
and flat-shift torque reduction do not build new evidence. The first sample
establishes an anchor. Invalid VSS clears evidence and requires a new anchor
when valid speed returns, avoiding artificial acceleration/deceleration.

Gain and decay use elapsed seconds. `smAccumulatorSnapTimeMs` optionally clears
opposite-sign evidence after sustained Accelerating or Decelerating/Overrun.
Evidence freezes during an open clutch window. Release closes the window and
clears its accumulator; a window also expires after more than 3000 ms.

With `smSecondClutchSwitchAvailable`, the complementary switch can open the
window early; Clutch Down release can close a Clutch Up window early. Confirmed
direction can be held by `smShiftLatchTimeMs`, with optional extension on a new
full Clutch Down press. Clutch Up at rest always cancels the shift overlay.
Losing valid VSS cancels VSS-mode windows and latches. The first usable clutch
sample after reset, sensor loss or a detection gap establishes switch history
without synthesizing a press edge. A held pedal must be released and pressed
again to open a new window.

Disable, engine stop and configuration changes clear flags, windows, latches,
accumulators, RPM samples and hold timers. The reset reports Off until the next
enabled callback classifies current inputs; `engineSmEnabled` continues to
reflect the enable setting. Off and Cranking callbacks clear history and do not
publish launch or shift overlays.

## STFT integration

Both `useEngineStateMachine` and `stateBasedStft` must be enabled to replace
RPM/load region selection. Idle, Afterstart and Coasting select the idle cell;
Overrun selects overrun; WOT selects power; all remaining primary states select
cruise. Existing per-region gains, limits and learning gates are unchanged.
The fast fuel loop observes the latest completed slow-callback state.

## DFCO integration

[State-aware DFCO](fueling_system.md#overrun-and-deceleration-fuel-cut) consumes
the latest Decelerating and up/downshift flags in the fast loop. Decel fuel cut
also permits a raw RPM-rate fallback when no vehicle-speed sensor is configured;
that fallback does not change primary-state classification. Overrun fuel-cut
eligibility is evaluated independently of the SM's physical Overrun state.

## Regression coverage

`unit_tests/tests/engine_state_machine/test_engine_state_machine.cpp` covers
history reset/rearming after disable, stop and burn; independent RPM sampling;
launch-time evidence suppression; initial and recovered VSS samples; VSS loss
during a latched shift; both physical clutch inputs and Overrun classification;
and suppression of overlays while stopped or cranking.
`HeldClutchDoesNotCreateFreshShiftAfterBurn` checks that burning settings with
the pedal held does not invent a press edge or revive an expired shift.

The suite also covers state priority, transient hold, rate cadence, clutch
edges, accumulator direction, latches, stale-window timeout and temperature
overlays.
`unit_tests/tests/test_stft.cpp` covers region mapping and threshold fallback.
`unit_tests/tests/engine_state_machine/test_operating_modes.cpp` covers the
Limp latch and priority, Sport Mode, Ghost Cam via Sport Mode, the Eco settle hold,
the traction-control Torque Reduction flag and shift suppression of the overrun
fuel cut; `test_eco_mode.cpp` and `test_pops_and_bangs.cpp` cover the Eco and
Pops and Bangs machines.
