# External CAN-Bus ETB Controller — Feasibility Notes

Concept: an external CAN node built on a **CH32V203F6P6** (RISC-V MCU) driving a
**TLE9201SGAUMA1** H-bridge, reading the 4 analog ETB inputs (dual TPS + current
sense, etc.) and closing the throttle position loop locally. The main rusEFI ECU
configures and supervises the loop over CAN so it behaves, from TunerStudio's
point of view, like an onboard ETB.

## Hardware — solid

- **TLE9201SG is already a proven part on this platform.** rusEFI has a native
  driver for it in-tree (`tle9201.o`), used as an ETB H-bridge on boards such as
  alphax-s197-v2. The actuator stage is not a risk.
- **CH32V203 generally has the right peripherals** — ADC, PWM timers, and bxCAN —
  to cover PWM/DIR to the TLE9201, current-sense readback, and the CAN link.
- **Open item:** confirm the specific **F6P6** SKU/package actually has CAN and
  ≥4 ADC channels. WCH ships many CH32V203 pin-count/peripheral variants, and the
  smallest packages sometimes drop CAN or trim ADC channel count. Verify before
  committing to this exact part.

## Architecture options considered

### 1. Remote closes the loop independently (rejected)
CH32 owns target tracking, plausibility checks, and fail-safe state entirely on
its own. Rejected because it duplicates rusEFI's existing ETB fault-handling
logic in a second codebase/toolchain (WCH SDK vs. ChibiOS) with no shared code.

### 2. rusEFI's PID runs centrally, CAN carries raw duty/sensor values (rejected)
rusEFI's existing `EtbController` runs unmodified; the CH32 is a dumb CAN↔H-bridge
bridge (ADC→CAN sensor frame, CAN duty frame→PWM/DIR). This is architecturally
the cleanest in terms of code reuse — `Sensor::get(SensorType::Tps1)` already
supports a CAN-backed implementation via `CanSensor<TStorage, TScale>`
(`firmware/controllers/sensors/can_sensor.h`), and `DcMotor`
(`firmware/controllers/system/dc_motor.h`) is a tiny interface (`set(float duty)`,
`enable()`, `disable()`) that a `CanDcMotor` could implement in well under 100
lines using the existing `CanTxMessage` helper.

Rejected because the **entire stabilizing PID loop would live inside the CAN
round-trip** (ADC sample → CAN → PID compute → CAN → PWM). ETB control wants
~1kHz update with bounded jitter; any bus contention or latency directly
degrades loop bandwidth and risks oscillation.

### 3. Hybrid: local loop closure, centralized tuning authority (current direction)

- **CH32 owns the tight, latency-critical tick**: reads its local TPS1/2 (and
  other analog inputs), runs a ported `Pid`/`PidIndustrial` control step, drives
  the TLE9201 PWM/DIR directly. This stays fast and local — no CAN in the
  stability-critical path.
- **rusEFI owns gain selection, autotune, fault/plausibility analysis, and TS
  display** — fed by TPS1/2 (and duty/current/fault flags) mirrored back over
  CAN, consumed via the existing `CanSensor` mechanism so the rest of rusEFI
  (gauges, logging, fault logic) doesn't need to know the sensor is remote.
  rusEFI pushes new `pid_s` gains down to the CH32 whenever autotune (or the
  user) decides to adjust them.

**Why this is feasible, not just plausible:**

- `firmware/util/math/efi_pid.h` — the `Pid` class is essentially pure math: no
  ChibiOS/HAL dependencies, only generated structs (`pid_state_s`, `pid_s`), and
  the one `EFI_TUNER_STUDIO`-guarded method is trivially stubbed. This is the
  piece to port to the CH32 — small and self-contained, not a deep architectural
  lift.
- **rusEFI already ships this exact pattern.**
  `firmware/controllers/can/wideband_firmware/` runs its own closed heater-control
  loop on a separate small MCU and reports back over CAN; the main ECU consumes
  it as a normal sensor. The ETB hybrid is the same shape applied to throttle
  control.
- `electronic_throttle.cpp:727` already has `m_isAutotune` — ETB autotune exists
  today. In the hybrid, that decision logic (what gains to try next, based on
  observed step response) can keep running unmodified inside rusEFI, consuming
  TPS1/2 telemetry over CAN, rather than being duplicated on the CH32.

**Open question — porting scope:** `EtbController`'s per-tick body
(`electronic_throttle.cpp` ~L254-300+) is intertwined with rusEFI-specific
`Sensor::get`/`engineConfiguration` types, so it can't be dropped onto the CH32
as-is. The plan is to extract just the `Pid::getOutput` call plus the
duty-clamp/scale math (`ETB_PERCENT_TO_DUTY` etc.) into a small standalone loop
on the CH32, hand-wired to its own ADC/PWM — a contained, well-bounded port, not
a literal file clone.

## CAN protocol needs

- **Gains push** (rusEFI → CH32): `pid_s` (P/I/D/offset/etc.) — infrequent, only
  on config change or autotune step.
- **Telemetry pull** (CH32 → rusEFI): TPS1/2, actual duty, motor current, fault
  flags — needs to be frequent enough that rusEFI's autotune transient analysis
  isn't aliased, but is not load-bearing for stability since the stabilizing
  loop already runs locally on the CH32.
- **Fail-safe watchdog on the CH32 side**: if gain/target updates from rusEFI go
  stale, the CH32 must independently default to motor-off. This is the one piece
  of fail-safe logic that genuinely needs to be written fresh on the remote
  side — everything else (TPS-invalid handling, etc.) falls out of rusEFI's
  existing sensor-timeout fault path once TPS1/2 are wired through `CanSensor`.

## Bottom line

Feasible, and the hybrid design is the right balance: it avoids putting the
stabilizing control loop inside a CAN round-trip (the main risk of a purely
centralized design) while keeping tuning, autotune, and TunerStudio
configuration centralized in rusEFI (avoiding duplicated fault/plausibility
logic, the main risk of a purely remote design).

## Next steps to consider

1. Confirm CH32V203F6P6's exact pinout/peripherals (CAN presence, ADC channel
   count) against the 4-analog-input + PWM/DIR + CAN budget.
2. Define the CAN message set: gains-push frame, telemetry frame, heartbeat/
   staleness handling.
3. Extract the CH32-side control tick (Pid + duty scaling) as a standalone unit
   decoupled from rusEFI's `Sensor`/`engineConfiguration` types.
4. Decide telemetry rate needed for autotune's transient analysis to remain
   meaningful over CAN.
