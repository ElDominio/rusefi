# Instantaneous Crank Angle Estimation and Misfire Detection

Design notes on how rusEFI estimates engine angle between trigger teeth, why that
estimate cannot be used to measure misfire, and what "better" estimators would and
would not buy us. Written as background for the tooth-timing misfire detector
(`firmware/controllers/algo/misfire_detection.cpp`).

## What rusEFI does today

`TriggerCentral::getCurrentEnginePhase()` (`firmware/controllers/trigger/trigger_central.cpp`)
returns a continuous angle estimate at any instant:

```cpp
return toothPhase + elapsed / oneDegreeUs;
```

- `toothPhase` — engine phase captured at the **last** trigger edge.
- `elapsed` — time since that edge.
- `oneDegreeUs` — microseconds per degree, derived from the current RPM.

This is a **zeroth-order hold on speed**: it assumes angular velocity is constant at
whatever it was at the last tooth and integrates forward. It is cheap (one division),
robust at cranking / low RPM / sync loss, and predictable. Between teeth there is **no
new sensor data** — the crank/cam sensor only produces a real timing sample when an
edge passes. The "estimated angle" is a straight-line model, not a measurement.

## Why the estimate cannot measure misfire

A misfiring cylinder makes the crank **decelerate** through its expansion stroke instead
of accelerating. The detector measures that by timing how long the crank takes to rotate
through a fixed angular window. But `getCurrentEnginePhase()` *assumes constant velocity*
to interpolate. Asking it for the timestamp at an angle inside a tooth gap returns

```
time ≈ (windowEnd − windowStart) × oneDegreeUs
```

— angular width times a constant. It would **smooth away the very deceleration we are
trying to detect**. It is circular: using a constant-speed model to measure a departure
from constant speed.

So the resolution limit is physical, not an implementation shortcut:

- Real angular-velocity samples exist **only at tooth edges**. Tooth-to-tooth period is
  the genuine, highest-fidelity measurement of instantaneous crank speed available.
- Interpolating to an arbitrary angle between teeth does not add information — it
  fabricates a linear profile. On a 60-2 wheel (≈6° teeth) the window ends land
  essentially on real samples and it looks continuous; on a 3-tooth crank (120° teeth)
  anything between teeth is invented.

This is why the misfire detector measures **raw tooth-to-tooth timestamps** rather than
consulting the angle estimate. It reads the true velocity samples instead of trusting the
constant-velocity model. The only failure mode is the degenerate case where the window
collapses inside a single tooth interval and `segUs` becomes 0 — guarded against by
refusing to run when the window is narrower than the average tooth spacing (see
`onEnginePhase`, the "window too narrow for trigger" `configError`).

## The ladder of better estimators

Ordered from cheapest to most sophisticated. Each rung improves the estimate of *where
the crank is right now*, which helps **scheduling** (spark/injection firing closer to the
intended angle during transients). None of them add information between teeth.

1. **Constant-acceleration (first-order) extrapolation.** Use the last *two* tooth periods
   to estimate angular acceleration and extrapolate with a quadratic-in-time term. The
   crank genuinely accelerates through every power stroke and decelerates through every
   compression, so the constant-speed model always lags during those swings.
   `InstantRpmCalculator` already computes `m_instantRpmRatio` between teeth, so some
   ingredients exist. A couple of extra multiplies; the obvious next rung.

2. **Polynomial / spline fit over the last N tooth samples.** Fit a smooth curve through
   recent `(angle, time)` edges and read position/velocity off it. More accurate *inside*
   the measured span, but extrapolation past the last tooth can ring; more compute.

3. **Kalman filter / state observer.** Model the crankshaft as a dynamic system
   (state = angle, velocity, optionally acceleration or indicated torque), predict between
   teeth from the model, correct at each edge. The "proper" estimator; what the research
   literature uses for this problem. Degrades gracefully with noisy edges, can fuse
   crank + cam. Costs: tuning of process/measurement noise, an inertia model, more compute,
   and care at low RPM / cranking where higher-order models get twitchy.

### Two connections that matter for misfire specifically

- **The Kalman innovation *is* a misfire signal.** With a smooth crank-dynamics observer,
  the measurement residual (actual tooth period minus model prediction) spikes precisely
  when a cylinder fails to make torque. The estimator and the detector become the same
  object — watch the residuals instead of computing a separate EMA/wobble baseline. A real
  architectural alternative, not just a cosmetic upgrade.

- **Beware over-smoothing.** A constant-acceleration or Kalman model that is too aggressive
  *absorbs* the combustion event into the state estimate — the same circularity as the
  constant-speed estimate. The model must be loose enough that combustion shows up as
  residuals, not state.

## The levers that beat estimator sophistication

On a coarse wheel the estimator order is a rounding error compared to two physical levers:

- **Tooth count.** Going 3-tooth → 36-1 / 60-2 does more for *any* method than the fanciest
  filter on 3 teeth. Resolution is physical; estimator math has steep diminishing returns.
  Second-difference acceleration estimates also get *noisy* on coarse wheels — tooth
  machining tolerance turns directly into fake angular acceleration.

- **Per-tooth error learning (segment adaptation).** The production-car trick: every
  physical tooth has a small machining/spacing error that pollutes crank-speed measurement.
  OEM misfire systems *learn* a per-tooth correction during fuel-cut decel (known-zero
  torque) and subtract it, so the measured fluctuation is combustion, not wheel geometry.
  Improves misfire detection **without touching the estimator** — arguably more valuable
  than a higher-order extrapolation.

## Bottom line

- The constant-speed estimate is a deliberate, robust choice, not an oversight.
- It cannot be used to measure misfire — it assumes away the signal.
- The misfire detector is correct to time raw tooth-to-tooth periods; the only fix needed
  was guarding the sub-tooth-window degenerate case.
- For *scheduling* accuracy, constant-acceleration extrapolation is the cheap win.
- For *misfire* sensitivity, tooth count and per-tooth adaptation matter far more than
  estimator order; a Kalman-residual approach is the "do it properly" path if per-cylinder
  detection on a high-count wheel is ever wanted.
