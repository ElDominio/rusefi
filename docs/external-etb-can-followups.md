# External CAN ETB — Follow-ups from 2026-09-03 Bench Bring-Up

Companion to `CAN_ETB_FEASIBILITY.md` (this repo) and `external-etb/CH32V203_ETB_CONTROLLER.md` /
`external-etb/rusefi/RUSEFI_SIDE_TODO.md` (the board-side repo). Those docs cover the original
architecture and build order; this one is a punch list from the first real bench session, on branch
`external-etb-can-controller`.

## Already fixed this session (2026-09-03)

- `firmware/tunerstudio/tunerstudio.template.ini` — Auto Calibrate ETB 1/2 and pedal "Grab Idle/Up"
  / "Grab WOT/Down" buttons were gated on `isTps1Primary`/`isTps2Primary`/`isPpsPrimary`, which only
  check *local* ADC channel config. Added `|| enableExternalCanEtb` so these work with local channels
  left at None.
- `firmware/console/status_loop.cpp`:
  - `rawPpsPrimary`/`rawPpsSecondary` were displaying the CAN board's raw 12-bit ADC count directly
    as if it were volts (e.g. raw count 24 shown as "24V"). Now converted using the board's 5V VDDA
    reference (`CH32V203_ETB_CONTROLLER.md` #1.1) when `enableExternalCanEtb` is set.
  - `rawTps1Primary`/`rawTps2Primary` read `SensorType::Tps1Primary`/`Tps2Primary`, which are never
    registered under CAN ETB (stale/zero). Now sourced from `getExternalEtbRawTps()` (the same raw
    ADC the autocal sweep uses), converted to volts. Mapped as `TPS1 -> rawTps1Primary`,
    `TPSB -> rawTps1Secondary` (both channels belong to the ONE throttle body this board drives, not
    two separate throttle bodies) - `rawTps2Primary/Secondary` intentionally zeroed, no CAN source.
  - `VIgn` gauge read `SensorType::IgnKeyVoltage` directly (flat 0 on boards like paralela with no
    separate ignition-relay sense circuit) instead of falling back to `BatteryVoltage` the way the
    real decision logic (`isIgnVoltage()`, `ignition_controller.cpp`) already does. Now mirrors that
    same fallback. Cosmetic only - `hasIgnitionVoltage` itself was already correct.
- `firmware/init/sensor/init_etb_can.cpp` — removed `externalEtbTps2Sensor` (was feeding the board's
  `TPSB` redundant/secondary channel into `SensorType::Tps2`, which means "throttle body 2" elsewhere
  in rusEFI - misrepresented a redundant channel as a second physical throttle). `Tps2` is now
  genuinely disconnected (reads 0/invalid) until a real second CAN ETB board with its own addressable
  telemetry exists (see "Wire protocol" section below - today's protocol can't support that anyway).
- `firmware/controllers/can/can_etb_remote.cpp` — considered, then reverted, forcing an identity
  (0..4095) calibration fallback when uncalibrated. Decided `TPS`/`TPS2` percent correctly reading 0%
  until real calibration exists is the intended safe behavior (matches the board's own
  `raw_to_percent()` guard) - use the Raw TPS gauges (calibration-independent) to verify wiring
  instead. No net change from original, just confirmed correct.
- `firmware/controllers/actuators/electronic_throttle_impl.h` — root cause of "Auto Calibrate opens
  weakly / fails the swing check even though Bench Test works": the board's CAN staleness failsafe
  disables its own h-bridge if `ETB_TARGET` goes stale for >200ms (`external-etb/firmware/src/
  failsafe.h`), but `doAutocalExternalCan()`'s Open/Close phases dwell 1000ms while sending the duty
  command only once at phase start - throttle loses drive for the last ~800ms of each phase before
  the endpoint is captured. Fixed by re-sending the duty command every tick (500Hz) during both the
  autocal dwell and the bench-test 300ms window, matching the periodic-resend convention the rest of
  this protocol already uses (`sendExternalEtbGains()`/`sendExternalEtbTarget()`). Deliberately did
  NOT loosen the board's 200ms timeout - that's a safety margin for a live throttle actuator, not
  something to trade away for one caller's convenience.

**Not yet bench-verified after these fixes** - rebuild, reflash, reload the regenerated `.ini` in
TunerStudio, and re-test: TPS/pedal raw gauges, Auto Calibrate actually reaching true open/closed
endpoints, `TPS2` reading 0/disconnected.

## Already fixed this session (2026-09-05) - TPS1/TPSB now a real "virtual channel", not a CAN-only sensor

Resolved `external-etb/rusefi/RUSEFI_SIDE_TODO.md` §6's long-open "should `ETB_STATUS`'s pre-scaled
percent be authoritative, or should rusEFI re-derive it?" question, in favor of the latter:

- Removed `externalEtbTps1Sensor` (`init_etb_can.cpp`) - the `CanSensor<int16_t, PACK_MULT_PERCENT>`
  that registered `SensorType::Tps1` directly from `ETB_STATUS`'s board-computed percent.
- `init_tps.cpp`'s `LinearSensorUnit`/`RedundantPair`/`TpsConfig` gained a `isVirtual` flag: when set,
  `configure()`/`init()` build the same `LinearFunc` calibration curve and register the same sensor a
  physical ADC pin would, just skipping the `AdcSubscription` hookup (no real channel to subscribe to).
  `initTps()` now passes `isVirtual = enableExternalCanEtb` for `analogTps1`'s primary/secondary
  config (TPS1/TPSB only - `tps2` untouched, no per-throttle CAN addressing exists anyway).
  `postExternalCanEtbRawTps()` (new, `tps.h`) feeds this from outside, exactly like
  `AdcSubscription::postRawValue()` would.
  `EtbCanRawListener` (`init_etb_can.cpp`) now converts `ETB_RAW`'s raw TPS1/TPSB counts to volts
  (the board's fixed 5V/4095-count ADC scale, `can_etb.h`'s `CAN_ETB_BOARD_ADC_FULL_SCALE_VOLTS`) and
  calls it on every frame, alongside its existing job feeding the auto-calibrate sweep.
- Consequence: `SensorType::Tps1`/`Tps1Secondary` under CAN ETB mode now go through the *exact* same
  code path (`tpsMin`/`tpsMax`/`tps1SecondaryMin`/`tps1SecondaryMax` calibration,
  `RedundantPair`/`RedundantSensor` mismatch detection) a physically-wired TPS1/TPSB would - this also
  resolves the fault/plausibility "where does it hook in" open question from the same RUSEFI_SIDE_TODO
  section, for free, since it's no longer a bespoke CAN-only path.
- `canEtbTps1RawMin/RawMax`/`canEtbTpsBRawMin/RawMax` (persisted rusEFI-only config, never part of the
  wire protocol) removed entirely - `tpsMin`/`tpsMax`/`tps1SecondaryMin`/`tps1SecondaryMax` (the same
  volts-typed fields a local ADC pin's calibration uses) are now the single source of truth, converted
  to the board's raw ADC counts only at the `ETB_CAL_TPS` TX boundary
  (`can_etb_remote.cpp`'s `sendExternalEtbCalibration()`). `doAutocalExternalCan()`
  (`electronic_throttle_impl.h`) now writes its sweep result back as volts into these same fields
  instead of raw counts into the old ones.
- TunerStudio: `tpsNum1Inputs` dialog's "Primary sensor input" (`tps1_1AdcChannel`) hidden under
  `enableExternalCanEtb` (selecting a local ADC channel is meaningless - TPS1 arrives over CAN);
  "Secondary sensor input" (`tps1_2AdcChannel`) was already hidden as a side effect (gated on
  `isTps1Primary`, which stays false since `tps1_1AdcChannel` is left unconfigured by design).
  "Primary closed/open"/"Secondary closed/open" (the four calibration voltage fields) gated
  `|| enableExternalCanEtb` instead - stay visible and manually editable, exactly as for a local pin.
- **No board firmware change required** - `ETB_RAW` and `ETB_CAL_TPS` are byte-for-byte unchanged on
  the wire; this was entirely a rusEFI-side reinterpretation of telemetry it already received and a
  recomputation of values it already sent. The board's own `raw_to_percent()`/local PID target
  tracking (which still needs `ETB_CAL_TPS`'s raw endpoints) is unaffected.
- Validation: unit tests (temporarily forcing `EFI_EXTERNAL_CAN_ETB=TRUE` in `unit_tests/efifeatures.h`,
  normally off there, then reverting) - full suite (1493/1493) passes, including the pre-existing local
  dual-ADC-pin TPS1/TPSB test (`etb.intermittentTps`) confirming the refactored
  `LinearSensorUnit::configure()`/`init()` still work correctly for the non-virtual case. Not
  hardware-tested this session (no bench/car access).

## Open: dead ETB actuator-page controls under CAN ETB mode

All of these are only ever read inside the local closed-loop pipeline
(`EtbController::getOpenLoop()`/`getClosedLoop()`/`setOutput()`), which is entirely skipped for a
CAN-mode throttle (`EtbController::update()` early-returns before calling
`ClosedLoopController::update()`). Confirmed by grep - no other reader exists.

| Control | Dialog | Currently |
|---|---|---|
| ~~PID min/max (`etb_minValue`/`etb_maxValue`)~~ | ETB PID settings | **Done (2026-09-03)** - transmitted via new `ETB_LIMITS` frame (`can_etb.h`), board applies to its output clamp (`pid.c`) |
| ~~iTermMin/iTermMax (`etb_iTermMin`/`etb_iTermMax`)~~ | ETB PID settings | **Done (2026-09-03)** - transmitted via `ETB_LIMITS`, board now clamps iTerm to this instead of the full ±100 output range - this was the one with an actual default mismatch (rusEFI ±30 vs board's old ±100) |
| ~~ETB Bias Table (`etbBiasBins`/`etbBiasValues`)~~ | (feedforward curve) | **Done (2026-09-03)** - transmitted via new `ETB_BIAS_1..4` frames, board interpolates and adds it on top of PID output (`feedforward.c`/`main.c`), mirroring `EtbController::getOpenLoop()`'s `openLoop + closedLoop` split |
| Jam Detection (`etbJamDetectThreshold`, `etbJamTimeout`, disable-jam button) | ETB Jam detection settings | `checkJam()` never called for CAN mode - still open, out of scope for the PID-feel fix above |

Decision needed for the remaining row: hide/grey it out in TS for `enableExternalCanEtb` (cheap,
honest), or extend the wire protocol so it's actually transmitted and used on the board too (same
shape as the three rows just closed above).

Separately (found 2026-09-03, not from the original bench session): the board's PID was
differentiating the raw, unfiltered single-tick ADC sample for its `dTerm` input instead of the
already-present `adc_read_filtered()` EMA - fixed in the same pass as the three rows above (see
`external-etb`'s `main.c`/`adc.h`), since it's a second, independent contributor to "PID feels bad
in the car" alongside the missing feedforward.

Separately noted, not CAN-specific: `engineConfiguration->etb.offset` **is** transmitted
(`ETB_GAINS_2`) and used by the board's PID (`pid.c`'s `output = pTerm + iTerm + dTerm +
gains->offset`), but there is no TS field anywhere to actually set it - a pre-existing gap, inverse
of the dead-controls problem above.

## Open: dead/stale telemetry under CAN ETB mode

Same root cause (fields only written inside the skipped local pipeline):

| Field | Set inside | Fix complexity |
|---|---|---|
| `etb1validPlantPosition` | `observePlant()` | Cheap - just read `Sensor::get(m_positionSensor).Valid` from the CAN path too, data's already there |
| `etb1jamDetected`, `etb1ETB jam timer` | `checkJam()` | Cheap - `checkJam()` only needs `target` (already computed by `getSetpointEtb()`, which DOES run for CAN mode) and `observation` (already CAN-sourced `Tps1`). Just needs to be called from the CAN path too |
| ~~`ETB: Duty` (`etb1DutyCycle`)~~ | `setOutput()` | **Done (2026-09-03)** - `EtbCanDutyListener` (`init_etb_can.cpp`) already decoded the board's real duty into `outputChannels.etbStatus.output`; now also mirrors it into `etb1DutyCycle` in the same listener, same values/units, so the plain "ETB: Duty" gauge isn't stuck at 0 anymore |
| ~~`etb1etbFeedForward`~~ | `getOpenLoop()` | **Done (2026-09-03)** - new `ETB_FEEDFORWARD` (`0x30F`) frame reports the feedforward term the board actually applied each tick; `EtbCanFeedForwardListener` (`init_etb_can.cpp`) decodes it into `etbFeedForward` via a new `IEtbController::setFeedForward()` setter. Deliberately NOT a local `interpolate2d()` recompute on the rusEFI side - the board is the authority on what it actually used (its own curve copy can briefly lag rusEFI's between periodic `ETB_BIAS_1..4` resends), so a local guess could silently disagree with the real applied value |
| `etb1Integral error` (`m_targetErrorAccumulator`) | `getClosedLoop()` | N/A - this is specifically the *local* PID's accumulator; the board's real iTerm is already alive under a different name, `etbStatus_iTerm` |
| `etb1EBT: last PID dT` | `getClosedLoop()` | N/A - no local PID tick exists in CAN mode |

Good news confirmed: `getSetpointEtb()` is shared by both paths (`sendExternalEtbTarget()` calls
`controller->getSetpoint()` every tick), so `ETB: target for current pedal`, `board adjustment`,
`target with idle`, `luaAdjustment`, `trim`, `target with adjustments`, `final target`, `traction
control`, `rev limit active`, `sport pedal active` are all already correctly live for CAN mode - no
fix needed there.

Proposed order: `validPlantPosition` -> `checkJam()` wiring -> `ETB: Duty` mirror. All three are
rusEFI-only, no board-side change, low risk.

## Open: wire protocol gaps (cross-repo, bigger)

- No per-throttle addressing: base ID `0x790000` is shared by every CAN ETB board on the bus
  (`can_etb.h`'s header comment). Fine for today's single-throttle-body setup; a real second physical
  board (true dual-throttle-body CAN ETB) would collide on the same IDs with no way to disambiguate.
  Flagging for whenever dual-throttle CAN ETB is actually attempted - not an issue today.
- ~~`EtbCanStatus` (rusEFI's `can_etb.h`) has no `Autotune` value ... nothing on the rusEFI side reads
  `ETB_STATUS_OFFSET_STATUS`/`ETB_PID_STATUS_OFFSET_STATUS` at all~~ **Done (2026-09-05)** - added
  `Autotune = 4` to `EtbCanStatus` (matching the board's `etb_status_t`) and a new `canEtbStatus`
  output channel (`output_channels.txt`, under the `canWriteOk`/`canWriteNotOk` CAN gauges), decoded
  from `ETB_STATUS_OFFSET_STATUS` in the existing `EtbCanDutyListener` (`init_etb_can.cpp`) - same
  frame the duty telemetry already comes from, no new CAN traffic. Motivated by a bench session
  where `ETB: Duty`/`etb1etbFeedForward` both read a flat 0 with PID gains deliberately zeroed to
  isolate the feedforward/bias curve (`etbasedutyno.msq`/`.msl`) - with no visibility into the
  board's own status byte there was no way to tell "board genuinely computed zero" from "board's
  h-bridge is off" (Fault, or Disabled from `ETB_TARGET` going stale). `ETB_PID_STATUS_OFFSET_STATUS`
  (the same byte, mirrored on `ETB_PID_STATUS`) still isn't separately read - redundant with
  `ETB_STATUS`'s copy, not needed. Raw enum value only, no TS combo-box lookup wired up (same
  convention as `wideband_state_s.stateCode`). Validated: firmware build for
  `fw-custom-paralela-master` (the `EFI_EXTERNAL_CAN_ETB=TRUE` board matching this branch's bench
  tune) links clean; full unit test suite (1493/1493) passes.

## Unrelated finding, not CAN-ETB-specific

`DC: en0` / `DC: output0` / `isEnabled0` (`dc_motors.txt` -> `engine->dc_motors`) are dead in **every**
mode, local hardware included - grepped the whole `firmware/controllers` tree, nothing ever writes to
`engine->dc_motors`. Orphaned/half-wired live-data struct, pre-existing, unrelated to this feature.
Not blocking anything here; separate cleanup if anyone wants to pick it up.
