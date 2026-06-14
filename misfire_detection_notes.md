# Misfire Detection — Design Notes

## How the detector works

Each cylinder's expansion stroke is timed over a configurable angle window (e.g. 20→120° ATDC). On a healthy firing, combustion accelerates the crank through that window quickly — short segment time. On a misfire, no combustion, the crank decelerates — longer segment time.

The segment time is compared against a shared EMA baseline (`m_emaSeg`). If `segDurationUs > m_emaSeg * misfireThresholdRatio`, the firing is **flagged**.

The baseline only updates on **clean** firings — flagged firings freeze it so real misfires cannot drift the threshold upward.

---

## Parameters

### `misfireWindowStart` / `misfireWindowEnd`
Define the angle window (degrees ATDC) over which each cylinder's expansion stroke is timed. This is **what you measure**.

- Wider window → more absolute time, cleaner signal, but more overlap risk on high-cylinder-count engines.
- Tighter window → less absolute time, more sensitive to trigger noise, but avoids contamination from overlapping power strokes.

### `misfireEmaAlpha` (default 0.05)
Controls how fast the baseline tracks legitimate RPM/load drift at idle.

- **Lower (e.g. 0.02)** — very stable baseline, roughly weights the last ~50 clean firings. Better noise rejection in steady state, but slow to follow genuine RPM shifts — a 50 RPM drop may cause false flags until the baseline catches up.
- **Higher (e.g. 0.10)** — faster adaptation, roughly weights the last ~10 firings. Fewer false flags from RPM drift, but a string of sub-threshold slow firings can gradually drift the baseline up and mask a real misfire that follows.

### `misfireThresholdRatio` (e.g. 1.15)
The multiplier on the baseline that defines what counts as a misfire. `1.15` means: *"flag this firing if it took more than 15% longer than the baseline expects."*

- Too low → trigger noise and normal RPM variation cause false flags.
- Too high → only severe misfires are caught; partial or subtle misfires slip through.

### `misfireConsecutiveCount` (N) / `misfireWindowFirings` (M)
The N-of-last-M rate gate. A flagged firing only produces a counted misfire if at least N flagged firings fall within the last M firings (engine-wide, any cylinder).

- This rejects one-off noise while catching both a single dead cylinder and a whole-engine breakup.
- The flags do not need to be consecutive — they just both need to be within the last M firings at the moment the second one is evaluated.

---

## N-of-M counting behavior

### Current behavior
`registerMisfire()` is called once per flagged firing that satisfies the N-of-M condition. The first flag never gets independent credit — it only enables the condition. When the second flag arrives and `flaggedInWindow(M) >= N`, one misfire event is counted. Two real misfires → one count.

### Proposed improvement — retroactive credit
Track a `m_nmPassing` bool, updated on **every** firing (not just flagged ones) so it correctly drops back to `false` when flags age out of the window.

- **Transition `false → true`**: call `registerMisfire()` `need` times — credit all N flags at once.
- **Already passing**: call `registerMisfire()` once per subsequent flag — each new misfire counts individually.
- **Window goes cold**: `m_nmPassing` resets to `false`; the next episode starts fresh with another retroactive burst.

This means:
- Two real misfires in a burst → **two counts**, not one.
- A consistently misfiring cylinder → keeps accumulating `+1` per misfire after initial confirmation. No double-counting.
- The `+N` retroactive burst fires once **per episode** — only again if the window fully clears between episodes.

---

## V8 considerations

On a V8 with 90° firing intervals, power strokes overlap — one cylinder is still pushing while the next fires. This contaminates the expansion stroke segment with torque from adjacent cylinders, degrading signal quality.

**Recommended adjustments for V8:**

- **Tighter window** — end the measurement window well before 90° ATDC to avoid contamination from the next firing. A 20→60° window captures peak combustion acceleration while staying clear of overlap.
- **Wider `misfireThresholdRatio`** — the shorter window produces smaller absolute time differences and more baseline variance; a 15% threshold that works on a 4-cylinder may need to be wider (e.g. 1.20–1.25) to avoid noise-driven false flags.
- Expect that **subtle single-cylinder misfires are harder to detect** — the overlapping torque from healthy cylinders partially masks the deceleration of a misfiring one.

V8 window placement is best determined empirically by watching live `misfireLastSegUs` vs `misfireEmaUs` data on a known-good engine and on a cylinder with an intentional misfire, then setting the ratio to bisect the two populations cleanly.
