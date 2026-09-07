# Mitsubishi 6G72 Fast Crank+Cam Sync (investigation / proposal)

Status: **investigation complete, no firmware code written yet.** This document
records the findings that motivate the work and the proposed design, so the
implementation can start from settled ground instead of re-deriving it.

## Problem

The Mitsubishi 6G72 (3000GT/GTO/VR-4 V6) trigger pair is:

- Crank: `trigger_type_e::TT_3_TOOTH_CRANK`, built by `configure3ToothCrank()`
  in `firmware/controllers/trigger/decoders/trigger_universal.cpp` (~line 183)
  - `commonSymmetrical(s, 3, 0.5, 1.4)`, `SyncEdge::RiseOnly` -> 3 physically
    identical teeth, 120 degrees apart, only rising edges are used for sync.
- Cam: `trigger_type_e::TT_VVT_MITSU_6G72`, built by `initializeVvt6G72()` in
  `firmware/controllers/trigger/decoders/trigger_mitsubishi.cpp` (~line 43-70)
  - `FOUR_STROKE_CAM_SENSOR`, `SyncEdge::Both`, 8-edge waveform,
    `gapTrackingLength = 5` (5 configured gap ratios).

Because all 3 crank teeth look identical, the crank decoder alone cannot
resolve which of the 6 physically-indistinguishable positions per 720-degree
engine cycle it is looking at (`needsDisambiguation()` returns true for
`FOUR_STROKE_THREE_TIMES_CRANK_SENSOR`, `trigger_structure.cpp:140-156`). Cam
sync is mandatory before injection/ignition are allowed
(`unit_tests/tests/test_limp.cpp:461-482`,
`noFiringUntilCamSyncOnSymmetricalCrank`).

Today, disambiguation happens through the *generic* path: the cam signal is
decoded as its own gap-pattern-matching `TriggerWaveform` (same machinery used
for the crank), and only once the cam decoder recognizes its own unique
5-gap-ratio sequence does `TriggerCentral::handleVvtCamSignal()` /
`adjustCrankPhase()` / `syncEnginePhaseAndReport()`
(`firmware/controllers/trigger/trigger_central.cpp`) resolve full crank phase.
That unique cam sequence occurs once per camshaft revolution, so **worst-case
sync takes up to ~720 degrees of crank rotation (2 crank revolutions)**, with
no extra artificial delay layered on top - this is already the earliest point
the current architecture can call sync.

## Idea

Speeduino's decoder for the same physical trigger (`speeduinodecoders.cpp`,
`triggerPri_4G63`/`triggerSec_4G63`, 6-cylinder branch around line 1655) does
not decode the cam as a waveform at all. It samples the cam pin's raw digital
level directly at each crank edge (both rising and falling, i.e. crank
"tooth count" of 6 edges/rev instead of 3), and a specific
(crank-edge-position, cam-level) combination is enough to resolve full sync,
sometimes within 1-2 edges.

rusEFI does not currently have this technique for the 6G72 (nor for the 4G63,
which also just uses a shorter gap pattern through the same generic decoder).
It does have an architecturally similar idea already, just mirrored: for a
subset of "simple pulse" VVT modes (`VVT_SINGLE_TOOTH`, `VVT_TOYOTA_3_TOOTH`,
`VVT_HONDA_K_INTAKE`, `VVT_MAP_V_TWIN`;
`vvtWithRealDecoder()`, `trigger_central.cpp:130-137`), the cam handler skips
the generic cam gap-decoder and instead reads the **already-known crank
phase** at the moment of a cam edge. What's proposed here is the mirror image:
read the cam's level at a crank edge.

The plumbing already exists for this:
- Cam pins are already registered `PAL_EVENT_MODE_BOTH_EDGES` regardless of
  `SyncEdge` config (`firmware/hw_layer/digital_input/trigger/trigger_input_exti.cpp:60-61`),
  so no new hardware wiring is required.
- The cam's live level is already latched on every real cam edge into
  `engine->outputChannels.vvtChannel1..4`
  (`trigger_central.cpp:353-361`), readable from anywhere including the crank
  handler.
- `triggerState` (crank decoder) and `vvtState[...]` (cam decoder) are both
  members of the same `TriggerCentral` object
  (`firmware/controllers/trigger/trigger_central.h:178`), so no cross-module
  refactor is needed to let one see the other's state.

`SyncEdge::RiseOnly` (current crank config) makes falling edges never reach
the decoder at all (`isUsefulSignal()` drops them before
`decodeTriggerEvent()`); switching to `SyncEdge::Rise` gives 6 evenly-spaced
(60 degree) sample points per crank revolution instead of 3 (both edges reach
the decoder for angle/RPM tracking, only rising edges are resync-eligible) -
this is what gives the cam-level check enough resolution to disambiguate
quickly. No crank/primary decoder in rusEFI currently uses `SyncEdge::Both`
(only cam/VVT waveforms do); there's a cautionary note in
`firmware/controllers/trigger/decoders/trigger_misc.cpp:130-144`
(`configure60degSingleTooth`) where `SyncEdge::Both` was tried on a
single-tooth crank and abandoned as unreliable in practice. The `TT_6G72_CRANK`
prototype independently confirmed this: `SyncEdge::Both` on a perfectly
symmetric wheel makes every edge resync-eligible, and since every edge
trivially passes the gap-ratio window, the decoder never advances past index 0
(see Next Steps for the trace-based diagnosis). `SyncEdge::Rise` avoids this
because only rising edges are resync-eligible, matching the cadence the
original `RiseOnly` design already relied on.

## Real-data validation

Real logged 6G72 crank+cam captures already exist in the repo:
`unit_tests/tests/trigger/resources/3000gt_cranking_rusefi.csv`,
`3000gt_cranking_rusefi_2.csv`, `3000gt_crank_cam_cranking.csv`,
`3000gt_crank_cam_cranking_2.csv`, `3000gt_crank_cam_cranking_idle.csv`
(`yellow` column = crank, `red` column = cam; format consumed by
`CsvReader`/`generateLog()` in
`unit_tests/tests/trigger/test_real_6g72_3000gt.cpp`). Two other files in the
same directory, `3000gt.csv` and
`3000gt_cranking_cam_first_crank_second_only_cam.csv`, are single-channel
(crank only) and were not usable for this analysis.

Method: extract every crank edge (rise and fall) with the cam level sampled
at that instant, group into sextets of 6 consecutive edges
(rise,fall,rise,fall,rise,fall = one crank revolution), and check whether the
6-level fingerprint is unique per revolution and stable across engine speed.

Findings, confirmed independently in all 5 usable captures, spanning cold
cranking (~140 RPM) through the catch/idle transition:

- Three recurring "families" of 6-edge fingerprint were observed, each
  disambiguating from its 360-degree twin at a different, predictable edge:

  | family (first 5 edges) | disambiguates at | edges needed |
  |---|---|---|
  | `0,1,0,0,1` -> `0` or `1` | edge 6 (2nd fall of 3rd tooth) | 6 (full 360 deg) |
  | `1,0,0,1,0` vs `1,1,0,1,0` | edge 2 (1st fall) | 2 (~120 deg) |
  | `0,0,1,0,0` vs `0,0,1,1,0` | edge 4 (2nd fall) | 4 (~240 deg) |

  Same families, same disambiguation points, in every file - this is real
  trigger-wheel geometry, not coincidence or a single-sample fluke.
- **Worst case across all data: full sync resolves within one crank
  revolution (360 degrees), about half of the current ~720-degree
  requirement.** Best case (depending on which tooth cranking happens to
  start on) is as little as 2 edges (~120 degrees).
- The other 5 bits of every 6-edge window were rock-stable across every file
  and every RPM observed - only the single designated disambiguating bit
  ever varied, and it varied *correctly* (alternating cleanly across
  consecutive revolutions) in the overwhelming majority of samples.

Caveat found during validation - the fragile bit:

- The disambiguating bit is fragile *because* it sits close to a real cam
  signal transition (that's why it disambiguates). Near RPM transients -
  observed at the cranking-to-catch inflection in
  `3000gt_crank_cam_cranking.csv` (~t=2.75-3.0s) and
  `3000gt_crank_cam_cranking_2.csv` (~t=4.35-4.7s) - that bit occasionally
  read the "wrong" way for a revolution or two, producing a fingerprint
  outside the canonical set for that stretch. This never happened to any of
  the other 5 bits in any file.
- `3000gt_crank_cam_cranking_idle.csv` also shows plain contact/starter
  noise at the very start of cranking (sub-millisecond double-edges around
  t=1.974s) - not real teeth, and any implementation needs to run this
  through the existing trigger noise filter before trusting it.

## Design implication

This is a promising fast-sync candidate, but not a "trust the first edge
blindly" algorithm. A real implementation should:

1. Reuse the existing crank noise filter before considering a candidate edge.
2. Treat the fast-path result as provisional and let the existing full
   gap-decoder corroborate it within the next revolution or two, rather than
   fully committing on a single sample - especially during the volatile
   cranking-RPM-ramp phase where the disambiguating bit is least reliable.
3. Not touch the existing generic gap-decoder engine
   (`TriggerWaveform`/`TriggerDecoderBase`) at all - this is a parallel,
   opt-in path added at the `TriggerCentral::handleShaftSignal()` /
   `handleVvtCamSignal()` seam, the same architectural seam
   `vvtWithRealDecoder()` already uses for its bypass modes.

## Proposed trigger types (naming decision)

To keep the existing, field-proven decoders available as a fallback (in case
the fast-sync path misbehaves on some units, or on engines whose cam-crank
mechanical alignment doesn't reproduce the same disambiguating-bit
guarantee), the fast-sync decoders will be **new, separate trigger types**,
not a modification of the existing ones:

- Crank: new `TT_6G72_CRANK` ("6G72 Crank" in the TS UI) - same physical
  3-tooth wheel as `TT_3_TOOTH_CRANK`, but built with `SyncEdge::Rise` so both
  edges of each tooth reach the decoder for angle/RPM tracking (6 edges/rev),
  while only rising edges are eligible to re-trigger the sync-point check
  (`SyncEdge::Both` was tried first and found broken - see Next Steps).
- Cam: new `VVT_MITSUBISHI_6G72_BETA` / `TT_VVT_MITSU_6G72_BETA` ("6G72 Cam
  Beta") - implements the crank-edge-samples-cam-level disambiguation
  described above, falling back to (or corroborated by) the existing
  gap-decoder logic per the design implication above.

The existing `TT_3_TOOTH_CRANK` and `TT_VVT_MITSU_6G72` stay exactly as they
are and remain the default/selectable option - nothing about them changes.
Users pick the new pair explicitly (per-board default or TS trigger type
selection) to opt into faster sync, with the old pair always available as a
known-good fallback.

## Next steps

- [x] Prototype `TT_6G72_CRANK` and confirm it doesn't regress angle
      interpolation/RPM calc on its own (independent of cam changes).
      Implemented as enum value 99 (`TT_UNUSED` bumped to 100) in
      `firmware/controllers/algo/engine_types.h`, `configure6G72Crank()` in
      `firmware/controllers/trigger/decoders/trigger_universal.{h,cpp}`,
      wired into the shape-builder switch in `trigger_structure.cpp`, and
      given a TS label in `firmware/integration/rusefi_config.txt`.

      **Correction from the original plan**: `SyncEdge::Both` (not `SyncEdge::Rise`)
      turned out to be the wrong choice. Traced via
      `Triggers/AllTriggersFixture.TestTrigger` (`unit_tests/tests/trigger/test_all_triggers.cpp`):
      this wheel is perfectly symmetric, so every single edge trivially
      satisfies the 0.5-1.4 gap-ratio sync window. `SyncEdge::Both` makes every
      edge eligible to re-trigger the sync-point check, so the decoder's
      `current_index` gets reset to 0 on every edge instead of ever advancing -
      it never actually cycles through positions. `SyncEdge::Rise` is correct:
      both edges still reach the decoder for angle/RPM tracking (per
      `trigger_decoder.cpp`'s `shouldConsiderEdge()`), but only rising edges
      are eligible for the resync check, matching the cadence the original
      `SyncEdge::RiseOnly` design already relied on. Confirmed via
      `unit_tests/triggers.txt` (12 events, 60 degrees apart, alternating
      rise/fall, `cycleDuration=120`, `syncEdge=Rise`).

      Validated against real captures (`unit_tests/tests/trigger/test_real_6g72_3000gt.cpp`,
      `sync_3000gt_cranking_rusefi_6g72CrankType`,
      `sync_3000gt_crank_cam_cranking_idle_6g72CrankType`, paired with the
      existing unmodified `TT_VVT_MITSU_6G72` cam decoder): identical
      first-RPM value and identical line index to the original
      `TT_3_TOOTH_CRANK` on the same files - no regression. Full unit test
      suite (1496 tests) passes.
- [x] Investigate the crank-edge/cam-level disambiguation mechanism itself
      (not yet wired into production). Findings below. Implementation is
      deliberately paused pending a decision on the safety-gating question
      raised here - see "Open question" at the end of this section.

      **How `syncEnginePhaseAndReport` actually works** (this simplified the
      whole design): `PrimaryTriggerDecoder::syncEnginePhase(divider, remainder, ...)`
      (`firmware/controllers/trigger/trigger_decoder.cpp:268-293`) doesn't need
      to know anything about crank tooth indices - it just repeatedly calls
      `incrementShaftSynchronizationCounter()` until
      `getSynchronizationCounter() % divider == remainder`, shifting engine
      phase by `engineCycle/divider` each increment, then sets
      `m_hasSynchronizedPhase = true`. Every existing VVT mode's
      `adjustCrankPhase()` (`trigger_central.cpp:181-248`) already just picks a
      hardcoded `remainder` (0 for `VVT_MITSUBISHI_6G72`) and calls
      `tc->syncEnginePhaseAndReport(crankDivider, remainder)`. So the fast path
      does not need to reimplement crank index tracking or engine-phase math -
      it only needs to determine the correct `remainder` (0-5, since
      `crankDivider` = `SYMMETRICAL_THREE_TIMES_CRANK_SENSOR_DIVIDER` = 6) faster
      than the existing cam gap-decoder, then call the exact same
      `syncEnginePhaseAndReport()` entry point every other VVT mode already
      uses. `getSynchronizationCounter()` increments once per RISE edge only
      (confirmed: `TT_6G72_CRANK`'s `SyncEdge::Rise` means only RISE edges are
      "important" for `shouldConsiderEdge()`/sync bookkeeping), so
      `synchronizationCounter % 6` at any moment is already computed inline as
      `crankInternalIndex` in `TriggerCentral::handleShaftSignal()`
      (`trigger_central.cpp:938`) - the fast path's hook point.

      **Empirically deriving the (remainder, cam-level) table, properly this
      time.** The earlier hand-analysis of raw CSV edges (see "Real-data
      validation" above) was done before `TT_6G72_CRANK` existed, using an
      assumed/manual sextet alignment. To ground it in the real firmware
      numbering instead of a guess, this session temporarily instrumented
      `trigger_central.cpp` (printf on every crank FALL edge: `synchronizationCounter`
      + live `outputChannels.vvtChannel1`; printf right before each
      `adjustCrankPhase()` call: `synchronizationCounter` pre-adjustment) and
      ran all 5 real captures through `TT_6G72_CRANK` crank +
      unmodified `TT_VVT_MITSU_6G72` cam (ground truth: the existing slow
      decoder's own successful lock). The instrumentation and its throwaway
      `DISABLED_dumpBetaDebug_*` unit tests were removed afterward -
      findings recorded here are what's durable.

      Since `synchronizationCounter` increments by exactly 1 per RISE edge with
      no other resets, a sample's raw counter value can be converted to the
      *true* remainder using the constant offset established at the run's own
      first successful cam-lock: `trueRemainder = (rawCounterAtFallEdge + K) % 6`,
      where `K = (0 - rawCounterAtFirstLock % 6 + 6) % 6`. This gives, per file,
      a clean table of `remainder -> dominant cam level`:

      | file | K | remainder->cam1 (0..5) |
      |---|---|---|
      | `3000gt_crank_cam_cranking.csv` | 0 | 0,1,1,0,0,1 |
      | `3000gt_crank_cam_cranking_2.csv` | 0 | 0,1,1,0,0,1 |
      | `3000gt_crank_cam_cranking_idle.csv` | 2 | 0,1,0,1,1,0 |
      | `3000gt_cranking_rusefi.csv` | 5 | 1,1,0,0,1,0 |
      | `3000gt_cranking_rusefi_2.csv` | 5 | 1,1,0,0,1,0 |

      At first glance these look like 2 disagreeing groups. They are not: the
      `cranking_rusefi[.2]` pair matches the other 3 files *exactly* when
      rotated by 3 (half of `crankDivider`=6, i.e. exactly 360 degrees) - e.g.
      `cranking_rusefi`'s remainder-0 equals the other files' remainder-3, its
      remainder-1 equals their remainder-4, and so on, for all 6 positions, no
      exceptions. **This means the existing, already-shipping `TT_VVT_MITSU_6G72`
      slow decoder itself locked onto the wrong (but structurally identical)
      cam pulse - exactly 360 degrees / one full crank revolution out of phase -
      on its very first sync attempt in 2 of the 5 real cranking captures, and
      never self-corrected for the rest of either file** (`adjustCrankPhase()`
      fired 4-5 more times per file at later cam revolutions, and every one
      found `synchronizationCounter % 6` already at its - wrong - locked value,
      i.e. zero further correction applied). This is a pre-existing property of
      the current production decoder, not something introduced by this work -
      it is unrelated to `TT_6G72_CRANK`/`SyncEdge::Rise` and would affect any
      board running `TT_3_TOOTH_CRANK` + `VVT_MITSUBISHI_6G72` today. It is
      being recorded here because it surfaced directly from this
      investigation's instrumentation and materially affects how much the fast
      path's own "ground truth" (an existing cam lock) can be trusted - see
      "Known residual risk" further down. It is **not** being fixed in this
      branch; that's separate, unrelated production-decoder work.

      Using the 3 mutually-consistent files as ground truth, the canonical
      table is **remainder 0..5 -> cam level `0,1,0,1,1,0`**.

      **How many consecutive samples are needed to resolve all 6 positions.**
      A single FALL-edge cam-level sample only gives 1 bit (3 remainders map to
      each cam value), not enough on its own. Checking all 6 rotations of the
      canonical pattern `[0,1,0,1,1,0]` against each other by increasing prefix
      length: rotations starting `0,...` need 3 samples to fully separate
      (`010` vs `011` vs `001` all differ by length 3); rotations starting
      `1,...` also need at most 3. **Worst case: 3 consecutive FALL-edge
      samples (roughly 240-360 degrees) uniquely identify the remainder** -
      consistent with (and slightly better than) the earlier hand-analysis's
      "worst case one crank revolution" finding, now derived from the actual
      firmware numbering instead of a manual CSV alignment guess.

      **Resolved - two-tier phase confidence, reusing an existing mechanism.**
      Wasted-spark ignition fires the same physical coil at both TDCs 360
      degrees apart, so a phase guess that's wrong by exactly 360 degrees (half
      of `crankDivider`=6) is harmless for wasted spark specifically - it is
      *not* harmless for sequential mode, nor for a guess that's wrong by some
      other amount (120/240 degrees), which the 3-sample match already guards
      against by requiring a clean, unambiguous match to exactly one of the 6
      canonical rotations. rusEFI already has almost exactly the staged-firing
      mechanism this implies, just not extended to symmetric cranks:
      `getCurrentIgnitionMode()` (`firmware/controllers/math/engine_math.cpp:77-89`)
      already auto-downgrades individual-coil ignition to wasted-spark whenever
      `!hasSynchronizedPhase()`, regardless of configured ignition mode or
      physical coil count; the only thing blocking *any* firing at all for
      symmetric cranks was one blanket check in
      `firmware/controllers/limp_manager.cpp`'s `noFiringUntilVvtSync()` (its own
      comment already distinguishes "non-symmetrical cranks can use faster
      spin-up mode" from "symmetrical crank modes require cam sync before
      firing").

      Implemented as a new, weaker confidence tier, separate from the existing
      `hasSynchronizedPhase()`:
      - `TriggerDecoderBase::syncEnginePhase()` (`trigger_decoder.h`/`.cpp`) takes
        a new `isProvisional` parameter (default false, zero behavior change for
        every existing caller): true sets the new `m_hasProvisionalPhase` instead
        of `m_hasSynchronizedPhase`. `hasProvisionalPhase()` returns
        `m_hasSynchronizedPhase || m_hasProvisionalPhase` - confirmed implies
        provisional. `resetHasFullSync()` clears both on a real trigger error/loss
        of sync.
      - `TriggerCentral::syncEnginePhaseAndReport()` gained the same
        pass-through parameter.
      - `limp_manager.cpp`'s `noFiringUntilVvtSync()`: when the symmetric-crank
        block would otherwise fire, checks `hasProvisionalPhase()` first and
        allows firing if set. `getCurrentIgnitionMode()` is untouched - it still
        keys off the strict `hasSynchronizedPhase()`, so a provisional-only guess
        never promotes to sequential mode.
      - The fast path itself only ever calls `syncEnginePhaseAndReport(..., isProvisional=true)`.
        The existing slow gap-decoder (unmodified) is the only thing that can
        set the strong, sequential-unlocking `hasSynchronizedPhase()`.

      **New trigger types actually implemented:**
      - `trigger_type_e::TT_6G72_CRANK` = 99 (`TT_UNUSED` bumped to 100) -
        `configure6G72Crank()` in `trigger_universal.{h,cpp}`, wired in
        `trigger_structure.cpp`, TS label in `rusefi_config.txt`. (Built and
        validated earlier in this session - see the corrected entry above.)
      - `vvt_mode_e::VVT_MITSUBISHI_6G72_BETA` = 35 (`rusefi_enums.h`) - maps to
        the *same*, unmodified `trigger_type_e::TT_VVT_MITSU_6G72` cam waveform
        (`engine.cpp`), and to the same `remainder=0` in `adjustCrankPhase()`'s
        switch (`trigger_central.cpp`) as `VVT_MITSUBISHI_6G72` - the slow
        decoder runs completely unchanged for this mode too. TS label "Mitsu
        6G72 Beta" added to `vvt_mode_e_enum` in `rusefi_config.txt` (padded
        with 3 `"INVALID"` placeholders for the pre-existing, already-short
        `VVT_CUSTOM_3/4/5` gap in that array).
      - The fast-path logic itself: `TriggerCentral::tryMitsu6g72BetaFastSync()`
        (`trigger_central.cpp`), called from `handleShaftSignal()` on every
        crank FALL edge. Maintains a 3-sample rolling window
        (`mitsu6g72BetaFallSamples[3]`, a `TriggerCentral` member so it resets
        naturally per engine instance) of the cam level at consecutive fall
        edges, matches it against the 6 canonical rotations of
        `{0,1,0,1,1,0}` (`matchMitsu6g72BetaPattern()`), and on a clean, single
        match calls `syncEnginePhaseAndReport(crankDivider, matchedRemainder,
        isProvisional=true)`. Gated on `trigger.type == TT_6G72_CRANK`,
        `vvtMode[engineSyncCam] == VVT_MITSUBISHI_6G72_BETA`, and
        `!hasProvisionalPhase()` (don't re-guess once any phase info exists).
        Does not touch `TriggerWaveform`/`TriggerDecoderBase`'s gap-matching
        engine at all, per the design implication above.

      **Measured results** (`unit_tests/tests/trigger/test_real_6g72_3000gt.cpp`,
      `real6g72.beta_*`, replaying the real captures with `TT_6G72_CRANK` +
      `VVT_MITSUBISHI_6G72_BETA`, comparing the CSV line index at which
      `hasProvisionalPhase()` vs. `hasSynchronizedPhase()` first becomes true):

      | file | provisional at | confirmed at | speedup |
      |---|---|---|---|
      | `3000gt_cranking_rusefi.csv` | 24 | 38 | ~1.6x |
      | `3000gt_cranking_rusefi_2.csv` | 24 | 38 | ~1.6x |
      | `3000gt_crank_cam_cranking.csv` | 24 | 75 | ~3.1x |
      | `3000gt_crank_cam_cranking_2.csv` | 28 | 39 | ~1.4x |
      | `3000gt_crank_cam_cranking_idle.csv` | 72 | 107 | ~1.5x |

      The fast path reached provisional phase before the slow decoder
      confirmed it on all 5 real captures, with no false-positive matches
      observed. Full unit test suite: 1501/1501 pass (1496 baseline + 5 new
      `beta_*` tests).

      **Known residual risk (unchanged from above, worth restating):** for the
      2 files where the slow decoder's own first lock was itself 360 degrees
      wrong (`cranking_rusefi[.2]`), the slow decoder's later (wrong) call to
      `syncEnginePhaseAndReport()` still overwrites whatever the fast path
      guessed, since the slow decoder remains authoritative by design. This
      means final state on those 2 files ends up phase-wrong post-confirmation
      - exactly as it already would with zero changes from this work (the bug
      is in the existing, unmodified slow decoder, not introduced here). It
      only matters for sequential mode; wasted-spark firing is 360-degree
      error tolerant per the reasoning above and is unaffected either way.
- [x] Replay all 5 real captures through the new decoder pair and confirm sync
      time improves without introducing false/early sync during the noisy
      cranking-start and RPM-transient windows identified above. Done via
      `real6g72.beta_*` in `test_real_6g72_3000gt.cpp` - see "Measured results"
      above (1.4x-3.1x faster on all 5 files, zero false-positive matches
      observed).
- [ ] Add unit test coverage exercising the fallback: new decoders disabled
      or misbehaving should not affect `TT_3_TOOTH_CRANK` / `TT_VVT_MITSU_6G72`
      users at all. Partially covered indirectly (every existing test still
      passes unmodified, and `isProvisional` defaults false for every
      pre-existing caller of `syncEnginePhase`/`syncEnginePhaseAndReport`), but
      no test explicitly exercises "fast path matches wrong/ambiguous data and
      is safely rejected" - worth adding before this leaves Beta.
- [ ] Real hardware validation on an actual 6G72 vehicle (cranking on a bench
      or a car) - everything above is real *logged* data replayed through unit
      tests, not a live ECU. Recommended before relying on this for actual
      starts, per the residual 360-degree-lock risk noted above.
- [ ] Consider whether the pre-existing "slow decoder can lock 360 degrees
      wrong with no self-correction" bug (found via this investigation's
      instrumentation, present in current production `TT_VVT_MITSU_6G72`,
      unrelated to this branch) deserves its own follow-up issue/fix.
