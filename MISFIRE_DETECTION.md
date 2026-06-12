# Misfire Detection Research & Proposed Architecture

This document outlines the feasibility and proposed implementation for a tooth-timing-based misfire detection system in rusEFI.

## 1. Feasibility Analysis
The current rusEFI firmware already maintains high-resolution tooth timing data that can be used for misfire detection.

### Key Data Points
*   **`trgtriggerSyncGapRatio`**: This is the ratio between the current tooth duration and the previous tooth duration (`toothDurations[0] / toothDurations[1]`). 
*   **Availability**: It is updated every single trigger tooth in `TriggerDecoderBase::decodeTriggerEvent`.
*   **Resolution**: 
    *   **60-2 Wheel**: 6° resolution.
    *   **36-1 Wheel**: 10° resolution.
*   **Advantages**: Much higher resolution than `instantRpm` (which averages over ~90°), allowing for the detection of sudden crankshaft deceleration ("jerk") caused by a lack of combustion torque.

## 2. Proposed Detection Strategy: Expansion Stroke Analysis
Misfire detection should focus on the **Expansion (Power) Stroke** rather than compression.

### Why Expansion?
*   **Healthy Cylinder**: Combustion "pushes" the piston, causing a sharp acceleration of the crankshaft (Gap Ratio < 1.0).
*   **Misfiring Cylinder**: The absence of combustion forces the flywheel to "drag" the engine through the cycle, resulting in a measurable deceleration (Gap Ratio > 1.0).
*   **Windowing**: Checking during compression is unreliable as compression always slows the engine down. Expansion is the only stroke where acceleration is expected.

## 3. Automated Windowing Architecture
The system can automatically calculate detection windows based on existing engine configuration.

### Required Inputs
*   `cylindersCount`
*   `firingOrder`
*   `triggerAngle` (Crank angle where the engine is at 0°)

### Automatic Calculation
For each cylinder, a monitoring window is defined relative to its TDC:
*   **Start**: `TDC + 20°` (Combustion pressure begins to peak)
*   **End**: `TDC + 120°` (Torque contribution starts to diminish)

### Implementation Example (4-Cylinder, 1-3-4-2)
| Cylinder | TDC Phase | Detection Window |
| :--- | :--- | :--- |
| Cyl 1 | 0° | 20° - 120° |
| Cyl 3 | 180° | 200° - 300° |
| Cyl 4 | 360° | 380° - 480° |
| Cyl 2 | 540° | 560° - 660° |

## 4. Proposed Configuration Fields
To allow for user calibration, the following fields should be added to `rusefi_config.txt`:

*   `misfireThresholdRatio`: The `gapRatio` limit above which a misfire is suspected (e.g., 1.15).
*   `misfireMaxRpm`: RPM ceiling for detection (e.g., 4500 RPM). Inertia makes high-RPM detection unreliable.
*   `misfireCountThreshold`: Consecutive misfires before logging an error or triggering a MIL.
*   `misfireWindowStart / End`: Degree offsets from TDC.

## 5. Implementation Path
1.  **Engine Module**: Create a `MisfireController` as an `EngineModule`.
2.  **Callback**: Hook into `mainTriggerCallback` to receive per-tooth updates.
3.  **Logic**:
    ```cpp
    void MisfireController::onEnginePhase(float rpm, efitick_t now, float currentPhase, float nextPhase) {
        if (rpm > config->misfireMaxRpm) return;
        
        for (int i = 0; i < cylinders; i++) {
            if (isPhaseInRange(misfireWindows[i].start, currentPhase, nextPhase)) {
                if (triggerSyncGapRatio > config->misfireThresholdRatio) {
                    incrementMisfireCounter(i);
                }
            }
        }
    }
    ```
4.  **Learning (Future)**: Implement a calibration phase during fuel-cut/overrun to "map" the trigger wheel's natural mechanical errors and subtract them from the live ratio.

## 6. As Implemented (idle-only, self-referenced)

The shipped `MisfireController` narrows the scope above to make it robust without the §4
learning step, by running **only while the Engine State Machine reports `Idle`** (lowest
inertia ⇒ highest combustion-torque signal). Sub-feature of the SM, gated by
`EFI_ENGINE_STATE_MACHINE`.

* **Windowing** — uses `engine->cylinders[i].getAngleOffset()` (firing order / odd-fire
  aware) + `isPhaseInRange`, mirroring `MapAveragingModule`. Window is `[TDC+start, TDC+end]`
  in the expansion stroke (defaults 20°–120°).
* **Metric — self-referenced EMA**, not raw `triggerSyncGapRatio`: each cylinder's
  *segment time* (latched from `edgeTimestamp` between the window-start and window-end teeth)
  is compared to that cylinder's own slow rolling average. A fixed trigger-wheel offset and a
  chronically-weak cylinder both fold into the baseline, so only an **acute** deceleration
  flags — this sidesteps the §4 wheel-learning problem for v1.
* **Counting — consecutive-arm** — a streak must reach `misfireConsecutiveCount` flagged
  firings on one cylinder to *arm*; once armed, every flagged firing increments that
  cylinder's counter; a clean firing disarms. Cumulative since key-on.
* **Trip** — when the total across cylinders reaches `misfireCountThreshold` (default 50),
  the MIL is **latched until power cycle** via an OBD code (`P0300`/`P0301`–`P0312`).
* **Config (TS page 5)** — `misfireDetectionEnabled`, `misfireCountThreshold`,
  `misfireConsecutiveCount`, `misfireThresholdRatio`, `misfireWindowStart/End`. TS dialog
  under Setup → Limits and protection, grayed unless `useEngineStateMachine`.
* **LiveData** — `misfire_detection_state_s`: active/latched bits, last cylinder, total +
  per-cylinder counts.
* **Files** — `controllers/algo/misfire_detection.{h,cpp,_state.txt}`; tests in
  `unit_tests/tests/test_misfire_detection.cpp`.

Known v1 trade-off (accepted): a *sporadic* misfire that keeps disarming won't count. The
upgrade path is "N of last M firings" instead of strict-consecutive.
