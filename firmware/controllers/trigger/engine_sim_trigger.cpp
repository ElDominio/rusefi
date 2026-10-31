#include "pch.h"
#include "engine_sim_trigger.h"
#include "trigger_emulator_algo.h"
#if defined(EFI_SIMULATOR_POSIX)
#include "ecu_sim_diagnostics.h"
#include "sim_clock.h"
#endif
#include <cmath>
#include <cstring>

// Debug snapshot for the in-process host: the ecu_dll pump reads these from
// outside the critical sections (never print inside the lock - a printf there
// wrecks the firmware's timing).
volatile int ecuDbgCommits = 0, ecuDbgPolls = 0, ecuDbgEdges = 0, ecuDbgActive = 0;
volatile float ecuDbgSpeed = 0, ecuDbgAngle = 0;

namespace {
constexpr double Pi = 3.14159265358979323846;
constexpr int LeaseMs = 250;
// A real crank/cam edge within this window blocks EngineSim activation entirely.
constexpr uint32_t RealToothWindowMs = 100;
float enabled = 0, speed = NAN, angle = NAN;
// Cam phases (crank deg from rest, advance positive), indexed like handleVvtCamSignal: bank * 2 + (0 intake,
// 1 exhaust). Each cam's tooth comes that much earlier, so the ECU's cam decoder reads it as its vvtPosition.
// Cam 1 (bank 1 intake) always has a tooth (it syncs the engine), NaN = 0; the others only when finite.
constexpr int SimCams = 4;
float camPhase[SimCams] = {NAN, NAN, NAN, NAN};
// Start/stop button the plant's operator presses (0/1); NaN = not mocked, the wired switch reads as usual.
float startButton = NAN;
bool dirty = false, active = false, scheduled = false;
double samplePhase = 0, previousPhase = 0, trackedSpeedDeg = 0;
efitick_t sampleTime = 0, previousTime = 0;
scheduling_s timer;
#if defined(EFI_SIMULATOR_POSIX)
EcuSimDiagnostics diagnostics;
efitick_t lastCrankStamp = 0, lastRiseStamp = 0, lastCommitStamp = 0;
bool wasSynchronized = false;
#endif

// The synthetic wheel must never take over a live engine: while directSelfStimulation is
// set, hwHandleShaftSignal/hwHandleVvtCamSignal DROP real crank/cam inputs (the sensor
// noise + self-stim guards in trigger_central.cpp), so activating against hardware that
// is actually turning would cut a real engine's trigger mid-stroke and resume real edges
// into a decoder shaped by fake teeth when the lease expires.
//
// The gate is EDGE-based on purpose: the synthetic wheel's own teeth drive the RPM state
// machine to SpinningUp/Running within one tooth, so isRunning() would kill EngineSim
// itself. Real pins keep producing edges even while they are being dropped, and those
// stamp noteRealHwTooth(); the emulator and the host simulator feed handleShaftSignal()
// directly and never stamp, so neither the simulator's acceptance flow nor a prod bench
// running the trigger emulator can deadlock this (an already-running emulator just has
// to be turned off first - one synthetic wheel source at a time).
bool synchronousPoll = false;
double wheelOffsetDeg = 0;

bool engineHasRealTriggerSource() {
#if defined(EFI_SIMULATOR_POSIX)
    // Host-clock mode has one mechanical source and no physical trigger pins.
    // There is no source takeover or hardware quiet window in this mode.
    if (simClockEnabled()) return false;
#endif
	// rusEFI port: this file only builds into the POSIX simulator, which has no physical
	// trigger pins, so nothing real can be turning underneath the synthetic wheel.
	(void)RealToothWindowMs;
	return false;
}
int inputIndex(const char* name) {
    if (!strcmp(name, "EngineSimEnable")) return 1;
    if (!strcmp(name, "EngineSimCrankSpeed")) return 2;
    if (!strcmp(name, "EngineSimCrankAngle")) return 3;
    if (!strcmp(name, "EngineSimCamPhase")) return 4;
    if (!strcmp(name, "EngineSimCamPhaseB1E")) return 5;
    if (!strcmp(name, "EngineSimCamPhaseB2I")) return 6;
    if (!strcmp(name, "EngineSimCamPhaseB2E")) return 7;
    if (!strcmp(name, "EngineSimStartButton")) return 8;
    return 0;
}
// rusEFI port: the synthetic wheel follows the TUNE's trigger, not a fixed 36-1.
// Plant angle 0 = TDC #1 (compression). The crank wheel is rusEFI's own configured
// triggerShape, placed so the decoder's TDC #1 (tdcPosition() past the sync point)
// lands on plant 0: shape angle = wheel phase + tdcPosition() + sync angle. Cams are
// the configured vvtShape[], placed so the decoded vvtPosition equals the plant's cam
// phase: vvtPosition = vvtOffset - (TDC-relative angle at the cam sync tooth), so the
// sync tooth goes at plant angle vvtOffset - camPhase. Any trigger/VVT setup whose
// offsets are right for the real engine is right here too.
constexpr double EdgeEps = 1e-6;
constexpr double CamToothWidth = 10;   // single-tooth cams (no vvtShape): rise, fall 10 deg later

TriggerWaveform& crankShape() {
    return engine->triggerCentral.triggerShape;
}
bool crankUsable() {
    auto& shape = crankShape();
    return shape.getSize() > 0 && !shape.shapeDefinitionError && shape.getCycleDuration() > 0;
}
double crankShift() {
    auto& shape = crankShape();
    return tdcPosition() + shape.getAngle(shape.triggerShapeSynchPointIndex);
}
// First shape state with switch angle strictly after `within` (sorted ascending), or size.
int firstStateAfter(const TriggerWaveform& shape, double within) {
    int lo = 0, hi = static_cast<int>(shape.getSize());
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (shape.getSwitchAngle(mid) > within + EdgeEps) hi = mid; else lo = mid + 1;
    }
    return lo;
}
// Next edge strictly after `phase` of a shape whose state k sits at wheel phase
// shapeAngle(k) - shift (mod the shape's cycle). Returns INFINITY when unusable.
double nextShapeEdge(const TriggerWaveform& shape, double phase, double shift, int& state) {
    const double cycle = shape.getCycleDuration();
    const int size = static_cast<int>(shape.getSize());
    if (size == 0 || cycle <= 0) return INFINITY;
    const double sigma = phase + shift;
    const double base = std::floor((sigma + EdgeEps) / cycle) * cycle;
    int k = firstStateAfter(shape, sigma - base);
    double edge;
    if (k < size) {
        edge = base + shape.getSwitchAngle(k);
    } else {
        k = 0;
        edge = base + cycle + shape.getSwitchAngle(0);
    }
    state = k;
    return phase + (edge - sigma);
}
double nextCrankEdge(double phase, int& state) {
    return crankUsable() ? nextShapeEdge(crankShape(), phase, crankShift(), state) : INFINITY;
}

// Cam index = bank * CAMS_PER_BANK + cam (handleVvtCamSignal's index), like camPhase[].
bool hasCam(int index) {
    if (index >= SimCams || index >= CAM_INPUTS_COUNT) return false;
    if (engineConfiguration->vvtMode[index % CAMS_PER_BANK] == VVT_INACTIVE) return false;
    // Bank 1 intake always (it is the sync cam on most setups); others only when wired.
    return index == 0 || isBrainPinValid(engineConfiguration->camInputs[index]);
}
// Wheel phase of the cam's sync tooth.
double camSyncPhase(int index) {
    const double phase = std::isfinite(camPhase[index]) ? camPhase[index] : 0;
    return engineConfiguration->vvtOffsets[index] - phase + wheelOffsetDeg;
}
// Next edge of cam `index` strictly after `phase`; rise reports the edge direction.
double nextCamEdge(int index, double phase, bool& rise) {
    const auto& shape = engine->triggerCentral.vvtShape[index % CAMS_PER_BANK];
    const double sync = camSyncPhase(index);
    if (shape.getSize() == 0 || shape.shapeDefinitionError) {
        // Single-tooth style cam: rise at the sync phase, fall CamToothWidth later.
        const double riseAt = (std::floor((phase - sync + EdgeEps) / 720) + 1) * 720 + sync;
        const double fallAt = (std::floor((phase - sync - CamToothWidth + EdgeEps) / 720) + 1) * 720 + sync + CamToothWidth;
        rise = riseAt <= fallAt;
        return std::min(riseAt, fallAt);
    }
    int state = 0;
    const double shift = shape.getAngle(shape.triggerShapeSynchPointIndex) - sync;
    const double edge = nextShapeEdge(shape, phase, shift, state);
    const int size = static_cast<int>(shape.getSize());
    const auto level = shape.wave.getChannelState(0, state);
    if (level == shape.wave.getChannelState(0, (state + size - 1) % size)) {
        // A state change on another channel only: not an edge of this cam. Skip past it.
        return nextCamEdge(index, edge, rise);
    }
    rise = TriggerValue::RISE == level;
    return edge;
}
double nextAnyCamEdge(double phase) {
    double best = INFINITY;
    for (int c = 0; c < SimCams; ++c) {
        bool rise;
        if (hasCam(c)) best = std::min(best, nextCamEdge(c, phase, rise));
    }
    return best;
}
// Fire every crank channel transition of shape state `state` (like the trigger emulator).
void fireCrankState(int state, efitick_t stamp) {
    auto& wave = crankShape().wave;
    const int size = static_cast<int>(crankShape().getSize());
    const int previous = (state + size - 1) % size;
    for (int channel = 0; channel < 2; channel++) {
        if (wave.getChannelState(channel, state) == wave.getChannelState(channel, previous)) continue;
        bool isRise = TriggerValue::RISE == wave.getChannelState(channel, state);
        isRise ^= (channel == 0 && engineConfiguration->invertPrimaryTriggerSignal);
        isRise ^= (channel == 1 && engineConfiguration->invertSecondaryTriggerSignal);
#if defined(EFI_SIMULATOR_POSIX)
        if (channel == 0 && isRise) {
            if (lastRiseStamp) diagnostics.lastRiseIntervalUs = NT2USF(stamp - lastRiseStamp);
            lastRiseStamp = stamp;
        }
#endif
        handleShaftSignal(channel, isRise, stamp);
    }
}
void poll();
void pollTo(efitick_t now, double endPhase, bool advanceClock) {
    chibios_rt::CriticalSectionLocker lock;
    if (!active || now - sampleTime > MS2NT(LeaseMs) || engineHasRealTriggerSource()) {
        active = false; scheduled = false; ecuDbgActive = 0;
        engine->triggerCentral.directSelfStimulation = false;
        return; // Native no-tooth watchdog expires RPM and sync.
    }
    const double advance = endPhase - previousPhase;
    if (speed > 0 && advance > 0 && advance < 1440 && now > previousTime) {
        engine->triggerCentral.directSelfStimulation = true;
        double phase = previousPhase;
        int work = 0;
        for (; work < 600; ++work) {
            int crankState = 0;
            const double crank = nextCrankEdge(phase, crankState);
            const double camEdge = nextAnyCamEdge(phase);
            const double edge = std::min(crank, camEdge);
            if (!(edge <= endPhase)) break;   // also stops on INFINITY (no usable shape)
            const auto stamp = previousTime + static_cast<efitick_t>((now - previousTime) * (edge - previousPhase) / advance);
#if defined(EFI_SIMULATOR_POSIX)
            // Execute earlier output and controller deadlines before the next
            // mechanical tooth. Firmware callbacks observe the same clock.
            if (advanceClock && simClockAdvanceTo(stamp) != 0) return;
#endif
            if (edge == crank) {
#if defined(EFI_SIMULATOR_POSIX)
                ++diagnostics.crankEdges;
                if (lastCrankStamp && stamp <= lastCrankStamp) ++diagnostics.nonMonotonicEdges;
                lastCrankStamp = stamp;
#endif
                fireCrankState(crankState, stamp);
            }
            if (edge == camEdge) {   // every cam with an edge here (cams at the same phase share it)
#if defined(EFI_SIMULATOR_POSIX)
                ++diagnostics.camEdges;
#endif
                for (int c = 0; c < SimCams; ++c) {
                    if (!hasCam(c)) continue;
                    bool rise;
                    if (nextCamEdge(c, phase, rise) == edge) {
                        handleVvtCamSignal(rise ? TriggerValue::RISE : TriggerValue::FALL, stamp, c);
                    }
                }
            }
            phase = edge;
            ecuDbgEdges = ecuDbgEdges + 1;
        }
#if defined(EFI_SIMULATOR_POSIX)
        if (work == 600) ++diagnostics.batchLimitHits;
#endif
    } else if (advance < 0 || speed <= 0) engine->triggerCentral.directSelfStimulation = false;
#if defined(EFI_SIMULATOR_POSIX)
    if (speed > 0 && ((advanceClock ? advance < 0 : advance <= 0) || advance >= 1440)) ++diagnostics.skippedAdvances;
    ++diagnostics.polls;
    const bool synchronized = engine->triggerCentral.triggerState.getShaftSynchronized();
    if (wasSynchronized && !synchronized) ++diagnostics.syncLosses;
    wasSynchronized = synchronized;
#endif
    ecuDbgPolls = ecuDbgPolls + 1;
    previousPhase = endPhase; previousTime = now;
#if defined(EFI_SIMULATOR_POSIX)
    if (advanceClock) simClockAdvanceTo(now);
#endif
    int nextState = 0;
    const double nextCrank = nextCrankEdge(endPhase, nextState);
    const double distance = std::min(nextCrank, nextAnyCamEdge(endPhase)) - endPhase;
    const int delayUs = speed > 0 && std::isfinite(distance) ? std::max(50, std::min(1000,
        static_cast<int>(distance * Pi / 180 / speed * 1e6))) : 1000;
    if (!synchronousPoll) {
        engine->scheduler.schedule("EngineSim", &timer, now + US2NT(delayUs), action_s::make<poll>());
    }
}
void poll() {
    chibios_rt::CriticalSectionLocker lock;
    const auto now = getTimeNowNt();
    pollTo(now, samplePhase + trackedSpeedDeg * NT2USF(now - sampleTime) / 1e6, false);
}
}

int engineSimInputIndex(const char* name) {
    return inputIndex(name);
}
int validateEngineSimInput(const char* name, float value) {
    const int index = inputIndex(name);
    return index ? validateEngineSimInputIndex(index, value) : 0;
}
int validateEngineSimInputIndex(int index, float value) {
    if (std::isnan(value)) return 1; // Named release, same contract as sensors.
    if (!std::isfinite(value)) return -1;
    return (index == 1 || index == 8 ? value == 0 || value == 1 :
        index == 2 ? value >= 0 && value <= 3500 : // headroom above 30k RPM for plant transients
        index >= 4 ? value >= -90 && value <= 90 : value >= 0 && value < 720) ? 1 : -1;
}
void stageEngineSimInput(const char* name, float value) {
    stageEngineSimInputIndex(inputIndex(name), value);
}
void stageEngineSimInputIndex(int index, float value) {
    chibios_rt::CriticalSectionLocker lock;
    switch (index) {
    case 1: enabled = value; break;
    case 2: speed = value; break;
    case 3: angle = value; break;
    case 4: case 5: case 6: case 7: camPhase[index - 4] = value; break;
    case 8: startButton = value; return;   // a switch input: no trigger commit needed
    }
    dirty = true;
}
void commitEngineSimInputs() {
    bool runPollNow = false;
    {
    chibios_rt::CriticalSectionLocker lock;
    if (!dirty) return;
    dirty = false;
    if (enabled != 1 || !std::isfinite(speed) || !std::isfinite(angle)) {
        active = false;
        return;
    }
    if (engineHasRealTriggerSource()) {
        // Refuse the takeover and hand the trigger back to real hardware immediately.
        active = false;
        engine->triggerCentral.directSelfStimulation = false;
        return;
    }
    const auto now = getTimeNowNt();
#if defined(EFI_SIMULATOR_POSIX)
    if (diagnostics.commits) {
        diagnostics.commitIntervalUs = NT2USF(now - lastCommitStamp);
        diagnostics.maxCommitIntervalUs = std::max(diagnostics.maxCommitIntervalUs, diagnostics.commitIntervalUs);
    }
    ++diagnostics.commits;
    lastCommitStamp = now;
#endif
    if (!active) {
        disableTriggerStimulator();
        engine->rpmCalculator.Register();
        Sensor::resetMockValue(SensorType::Rpm);
        samplePhase = previousPhase = angle + wheelOffsetDeg;
        previousTime = now;
        trackedSpeedDeg = speed * 180 / Pi;
    }
#if defined(EFI_SIMULATOR_POSIX)
    else if (simClockEnabled()) {
        // The host supplies the actual next plant angle. No wall-time phase
        // prediction or slew correction belongs in this mode.
        samplePhase = previousPhase;
        trackedSpeedDeg = speed * 180 / Pi;
    }
#endif
    else {
        // Preserve generated position across M packets. Network arrival jitter
        // cannot rewind the wheel or replay a tooth. Slew small errors out
        // with a bounded speed correction instead of jumping to each sample.
        const double predicted = samplePhase + trackedSpeedDeg * NT2USF(now - sampleTime) / 1e6;
        const double requestedSpeed = speed * 180 / Pi;
        const double error = std::remainder(angle + wheelOffsetDeg - predicted, 720.0);
        if (std::abs(error) > 20.0) {
            // The staged angle is authoritative in-process: an error beyond a
            // quarter of the sync gap window means the model drifted, so snap
            // onto the staged angle rather than slewing (slew only smooths the
            // sub-tooth jitter of the staged stream). Snaps are rare
            // re-anchors (boot, transients), not periodic events - small
            // drift stays on the slew path below.
            // A mid-stream wheel jump reads as missed/duplicated teeth to the
            // decoder: its cycle window is partial and its RPM model is
            // indexed by the old origin. Re-anchor exactly like the no-gap
            // cam-datum path (clean cycle state and sync counters, fresh
            // instant-RPM and spark model) so the next teeth re-sync cleanly.
            // rusEFI port: no EpicEFI acquireNoGapCamDatum(); a full decoder reset re-syncs
            // on the next gap (one revolution without sync on a rare snap).
            engine->triggerCentral.triggerState.resetState();
            trackedSpeedDeg = requestedSpeed;
            samplePhase = angle + wheelOffsetDeg;
        } else {
            const double limit = requestedSpeed * 0.1;
            trackedSpeedDeg = requestedSpeed + std::max(-limit, std::min(limit, error / 0.1));
            samplePhase = predicted;
        }
    }
    sampleTime = now; active = true;
    ecuDbgCommits = ecuDbgCommits + 1; ecuDbgActive = 1; ecuDbgSpeed = speed; ecuDbgAngle = angle;
    if (synchronousPoll) {
        runPollNow = true;
#if defined(EFI_SIMULATOR_POSIX)
        if (simClockEnabled()) runPollNow = false;
#endif
    } else if (!scheduled) {
        scheduled = true;
        engine->scheduler.schedule("EngineSim", &timer, now + US2NT(50), action_s::make<poll>());
    }
    }
    // Synchronous mode: the host commits at its selected wall-time cadence. The
    // wheel advances here on the caller's thread; scheduler re-arm is skipped.
    if (runPollNow) poll();
}
void releaseEngineSimInputs() {
    chibios_rt::CriticalSectionLocker lock;
    enabled = 0; speed = angle = NAN; active = false; dirty = false;
    for (float& phase : camPhase) phase = NAN;
    startButton = NAN;
}
int engineSimStartButton() {
    return std::isnan(startButton) ? -1 : startButton != 0;
}
bool engineSimTriggerActive() {
    chibios_rt::CriticalSectionLocker lock;
    return active;
}

void engineSimSetSynchronousPoll(bool on) {
    synchronousPoll = on;
}

void engineSimSetWheelOffset(double deg) {
    wheelOffsetDeg = deg;
}

float engineSimGetWheelPhaseDrift() {
    chibios_rt::CriticalSectionLocker lock;
    // How far the synthetic wheel's integrated phase has drifted from the staged
    // (plant) crank angle: the slew limiter deliberately tracks slowly, so the
    // drift grows during plant accelerations. In-process event consumers use it
    // to translate decoder-phase event angles back onto the plant's real cycle.
    // Only the SLEW lag is corrected here: the wheel-offset part is baked into
    // the decoder's sync (both sides share it) and must cancel in the plant's
    // angle matching instead.
    return static_cast<float>(samplePhase - angle - wheelOffsetDeg);
}

#if defined(EFI_SIMULATOR_POSIX)
int engineSimAdvancePlantTo(efitick_t target, float plantAngle, float plantSpeed) {
    chibios_rt::CriticalSectionLocker lock;
    if (!simClockEnabled() || target < getTimeNowNt()
        || validateEngineSimInput("EngineSimCrankAngle", plantAngle) != 1
        || validateEngineSimInput("EngineSimCrankSpeed", plantSpeed) != 1) return -1;
    stageEngineSimInput("EngineSimEnable", 1);
    stageEngineSimInput("EngineSimCrankSpeed", plantSpeed);
    stageEngineSimInput("EngineSimCrankAngle", plantAngle);
    commitEngineSimInputs();
    // No takeover delay in host-clock mode: failure to activate is a rejected
    // plant step, rather than silently advancing an ECU without crank input.
    if (!active) return -2;
    const double end = previousPhase + std::remainder(plantAngle + wheelOffsetDeg - previousPhase, 720.0);
    // A zero-speed sample has no edges, but timers still advance (watchdogs,
    // periodic controllers, prime injection, PWM). Advance cannot rewind.
    pollTo(target, end, true);
    samplePhase = previousPhase;
    sampleTime = target;
    const auto stats = simClockStats();
    return stats.budgetHits ? -3 : 0;
}

void engineSimReadDiagnostics(EcuSimDiagnostics& out) {
    chibios_rt::CriticalSectionLocker lock;
    const auto now = getTimeNowNt();
    auto& tc = engine->triggerCentral;
    out = diagnostics;
    out.flags = (active ? 1u : 0u) | (synchronousPoll ? 2u : 0u)
        | (tc.triggerState.getShaftSynchronized() ? 4u : 0u)
        | (tc.triggerState.hasSynchronizedPhase() ? 8u : 0u);
    out.configuredType = static_cast<uint32_t>(engineConfiguration->trigger.type);
    out.decoderType = static_cast<uint32_t>(tc.primaryTriggerConfiguration.TriggerType.type);
    out.expectedPrimary = tc.triggerShape.getExpectedEventCount(TriggerWheel::T_PRIMARY);
    out.shapeSize = tc.triggerShape.getSize();
    out.triggerErrors = tc.triggerState.totalTriggerErrorCounter;
    out.orderingErrors = tc.triggerState.orderingErrorCounter;
    // Absolute times need double precision: NT2USF narrows through float and
    // produces apparent clock drift of multiple microseconds after a few seconds.
    out.ecuWallUs = static_cast<double>(now) / US_TO_NT_MULTIPLIER;
    out.sampleAgeUs = static_cast<double>(now - sampleTime) / US_TO_NT_MULTIPLIER;
    out.stagedAngle = angle;
    out.wheelPhase = samplePhase;
    out.wheelRpm = trackedSpeedDeg / 6.0;
    out.decodedRpm = engine->rpmCalculator.getCachedRpm();
    out.phaseErrorDeg = std::remainder(angle + wheelOffsetDeg - samplePhase, 720.0);
    if (simClockEnabled()) out.flags |= 32;
    const auto stats = simClockStats();
    out.clockAdvances = stats.advances;
    out.timerCallbacks = stats.callbacks;
    out.lateCallbacks = stats.lateCallbacks;
    out.timerBudgetHits = stats.budgetHits;
    out.maxTimerLatenessUs = NT2USF(stats.maxLatenessNt);
}
#endif
