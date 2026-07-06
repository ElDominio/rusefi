/**
 * @file    idle_thread.cpp
 * @brief   Idle Air Control valve thread.
 *
 * This thread looks at current RPM and decides if it should increase or decrease IAC duty cycle.
 * This file has the hardware & scheduling logic, desired idle level lives separately.
 *
 *
 * @date May 23, 2013
 * @author Andrey Belomutskiy, (c) 2012-2022
 */

#include "pch.h"
#include "custom_page.h"
#include "engine_state_machine.h"

#if EFI_IDLE_CONTROL
#include "idle_thread.h"
#include "idle_hardware.h"

#include "dc_motors.h"

#if EFI_TUNER_STUDIO
#include "stepper.h"
#endif

using enum idle_mode_e;

/**
 * Computes the desired idle RPM target as a function of coolant temperature (CLT),
 * with optional bump for A/C, plus the hysteresis entry/exit thresholds used to
 * decide whether the engine is idling or coasting.
 *
 * Returned TargetInfo contains:
 *   - ClosedLoopTarget: RPM the closed-loop PID tries to hold
 *   - IdleEntryRpm:     RPM below which we re-enter idle
 *   - IdleExitRpm:      RPM above which we declare coasting (higher than entry for hysteresis)
 */
IIdleController::TargetInfo IdleController::getTargetRpm(float clt) {
	// Base target from the CLT->RPM curve (cold engines idle higher)
	targetRpmByClt = interpolate2d(clt, config->cltIdleRpmBins, config->cltIdleRpm);

  // FIXME: this is running as "RPM target" not "RPM bump" [ie adding to the CLT rpm target]
	// idle air Bump for AC
	// Why do we bump based on button not based on actual A/C relay state?
	// Because AC output has a delay to allow idle bump to happen first, so that the airflow increase gets a head start on the load increase
	// alternator duty cycle has a similar logic
	targetRpmAc = engine->module<AcController>().unmock().acButtonState ? engineConfiguration->acIdleRpmTarget : 0;

	float target = (targetRpmByClt < targetRpmAc) ? targetRpmAc : targetRpmByClt;
	float rpmUpperLimit = engineConfiguration->idlePidRpmUpperLimit;
 	float entryRpm = target + rpmUpperLimit;

  // Higher exit than entry to add some hysteresis to avoid bouncing around upper threshold
 	float exitRpm = target + 1.5 * rpmUpperLimit;

 	idleTarget = target;
	idleEntryRpm = entryRpm;
	idleExitRpm = exitRpm;

#if EFI_GHOST_CAM
	if (engine->module<EngineStateMachine>().unmock().engineSmIsGhostCam) {
		float ghostRpm = static_cast<float>(getCustomPage()->ghostCamIdleRpm);
		idleTarget = ghostRpm;
		idleEntryRpm = ghostRpm + rpmUpperLimit;
		idleExitRpm = ghostRpm + 1.5f * rpmUpperLimit;
		return { ghostRpm, ghostRpm + rpmUpperLimit, ghostRpm + 1.5f * rpmUpperLimit };
	}
#endif // EFI_GHOST_CAM

 	return { target, entryRpm, exitRpm };
}

/**
 * Classifies the current engine operating phase used by the idle controller:
 *   Cranking         - engine not yet running
 *   CrankToIdleTaper - just started, still tapering off cranking IAC position
 *   Idling           - closed-loop idle territory
 *   Coasting         - off-throttle, RPM above idle (engine braking)
 *   Running          - driver is on the throttle or vehicle is moving
 *
 * Hysteresis is applied via IdleEntryRpm / IdleExitRpm so we don't oscillate
 * between Idling and Coasting near the threshold.
 */
IIdleController::Phase IdleController::determinePhase(float rpm, IIdleController::TargetInfo targetRpm, SensorResult tps, float vss, float crankingTaperFraction) {
#if EFI_SHAFT_POSITION_INPUT
	// Not spinning at running RPM yet -> cranking
	if (!engine->rpmCalculator.isRunning()) {
		return Phase::Cranking;
	}
	badTps = !tps;

	if (badTps) {
		// If the TPS has failed, assume the engine is running
		return Phase::Running;
	}

	// if throttle pressed, we're out of the idle corner
	if (tps.Value > engineConfiguration->idlePidDeactivationTpsThreshold) {
		return Phase::Running;
	}

	// If rpm too high (but throttle not pressed), we're coasting
	// ALSO, if still in the cranking taper, disable coasting
  if (rpm > targetRpm.IdleExitRpm) {
 		looksLikeCoasting = true;
 	} else if (rpm < targetRpm.IdleEntryRpm) {
 		looksLikeCoasting = false;
 	}

 	looksLikeCrankToIdle = crankingTaperFraction < 1;
	if (looksLikeCoasting && !looksLikeCrankToIdle) {
		return Phase::Coasting;
	}

	// If the vehicle is moving too quickly, disable CL idle
	auto maxVss = engineConfiguration->maxIdleVss;
	looksLikeRunning = maxVss != 0 && vss > maxVss;
	if (looksLikeRunning) {
		return Phase::Running;
	}

	// If still in the cranking taper, disable closed loop idle
	if (looksLikeCrankToIdle) {
		return Phase::CrankToIdleTaper;
	}
#endif // EFI_SHAFT_POSITION_INPUT

	// If we are entering idle, and the PID settings are aggressive, it's good to make a soft entry upon entering closed loop
	if (m_crankTaperEndTime == 0.0f) {
		m_crankTaperEndTime = engine->fuelComputer.running.timeSinceCrankingInSecs;
		m_idleTimingSoftEntryEndTime = m_crankTaperEndTime + engineConfiguration->idleTimingSoftEntryTime;
	}

	// No other conditions met, we are idling!
	return Phase::Idling;
}

/**
 * Returns a 0..1 (and beyond, or negative while holding) fraction representing how far we
 * have progressed through the post-cranking taper. Negative/0 while still within the hold
 * duration (locked at the cranking value), 1 = taper complete. Used to blend cranking IAC
 * position into running open-loop position.
 */
float IdleController::getCrankingTaperFraction(float clt) const {
  	float holdDuration = interpolate2d(clt, config->afterCrankingIACtaperHoldDurationBins, config->afterCrankingIACtaperHoldDuration);
  	float taperDuration = interpolate2d(clt, config->afterCrankingIACtaperDurationBins, config->afterCrankingIACtaperDuration);
	float cyclesPastHold = (float)engine->rpmCalculator.getRevolutionCounterSinceStart() - holdDuration;
	return cyclesPastHold / taperDuration;
}

float IdleController::getOffIdleAdder(Phase phase, float rpm) {
#if EFI_OFF_IDLE_RPM_ADDER
	float dRpmPerSec = std::abs(rpm - m_lastRpmForStability) / (FAST_CALLBACK_PERIOD_MS / 1000.0f);
	m_lastRpmForStability = rpm;

	if (getCustomPage()->offIdleRpmAdder <= 0) {
		m_offIdlePhase = OffIdleAdderPhase::Inactive;
		return m_offIdleAdderRpm = 0;
	}

	// Cranking always aborts the adder, even if a prior Running/Coasting cycle left it
	// Armed/Stabilizing/Waiting/Decaying (e.g. the engine stalled and is being re-cranked).
	// The adder must only ever engage via Coasting (or Running), never carry over into cranking.
	if (phase == Phase::Cranking) {
		m_offIdlePhase = OffIdleAdderPhase::Inactive;
		return m_offIdleAdderRpm = 0;
	}

	const float maxAdder  = getCustomPage()->offIdleRpmAdder;
	const float stability = getCustomPage()->offIdleRpmStabilityThreshold;
	const float waitTime  = getCustomPage()->offIdleWaitTime;
	const float decayTime = getCustomPage()->offIdleRpmAdderDecayTime;

	bool isOffIdle = (phase == Phase::Running || phase == Phase::Coasting);

	if (isOffIdle) {
		m_offIdlePhase = OffIdleAdderPhase::Armed;
		return m_offIdleAdderRpm = maxAdder;
	}

	switch (m_offIdlePhase) {
		case OffIdleAdderPhase::Inactive:
			return m_offIdleAdderRpm = 0;

		case OffIdleAdderPhase::Armed:
			m_offIdlePhase = OffIdleAdderPhase::Stabilizing;
			return m_offIdleAdderRpm = maxAdder;

		case OffIdleAdderPhase::Stabilizing:
			if (dRpmPerSec < stability) {
				m_offIdlePhase = OffIdleAdderPhase::Waiting;
				m_offIdleWaitTimer.reset();
			}
			return m_offIdleAdderRpm = maxAdder;

		case OffIdleAdderPhase::Waiting:
			if (m_offIdleWaitTimer.hasElapsedSec(waitTime)) {
				m_offIdlePhase = OffIdleAdderPhase::Decaying;
				m_offIdleDecayTimer.reset();
			}
			return m_offIdleAdderRpm = maxAdder;

		case OffIdleAdderPhase::Decaying: {
			float elapsed = m_offIdleDecayTimer.getElapsedSeconds();
			if (decayTime <= 0 || elapsed >= decayTime) {
				m_offIdlePhase = OffIdleAdderPhase::Inactive;
				return m_offIdleAdderRpm = 0;
			}
			return m_offIdleAdderRpm = interpolateClamped(0, maxAdder, decayTime, 0.0f, elapsed);
		}
	}
	return m_offIdleAdderRpm = 0;
#else // !EFI_OFF_IDLE_RPM_ADDER
	(void)phase; (void)rpm;
	return m_offIdleAdderRpm = 0;
#endif // EFI_OFF_IDLE_RPM_ADDER
}

/**
 * Open-loop IAC position while cranking - purely a function of coolant temperature.
 */
float IdleController::getCrankingOpenLoop(float clt) const {
	return interpolate2d(clt, config->cltCrankingCorrBins, config->cltCrankingCorr);
}

/**
 * Computes the running (non-cranking) open-loop IAC position.
 * Starts from the CLT/RPM correction table and adds bumps for:
 *   - A/C compressor request
 *   - Cooling fans on
 *   - Lua scripting adder
 *   - Antilag system
 *   - TPS 'dashpot' decay when releasing throttle (helps avoid stalls)
 *   - Extra airflow as RPM climbs (airByRpmTaper)
 */
percent_t IdleController::getRunningOpenLoop(IIdleController::Phase phase, float rpm, float clt, SensorResult tps) {
	// Base value from a 2D CLT vs RPM correction surface
	float running = interpolate3d(
		config->cltIdleCorrTable,
		config->rpmIdleCorrBins, m_lastTargetRpm,
		config->cltIdleCorrBins, clt
	);

	// Now we bump it by the AC/fan amount if necessary
    if (engine->module<AcController>().unmock().acButtonState && (phase == Phase::Idling || phase == Phase::CrankToIdleTaper)) {
    	running += engineConfiguration->acIdleExtraOffset;
    }

	running += enginePins.fanRelay.getLogicValue() ? engineConfiguration->fan1ExtraIdle : 0;
	running += enginePins.fanRelay2.getLogicValue() ? engineConfiguration->fan2ExtraIdle : 0;

	for (int i = 0; i < IDLE_UP_SWITCH_COUNT; i++) {
		if (isBrainPinValid(engineConfiguration->idleUpSwitchPins[i])) {
			if (efiReadPin(engineConfiguration->idleUpSwitchPins[i], engineConfiguration->idleUpSwitchMode[i])) {
				running += engineConfiguration->idleUpAdder[i];
			}
		}
	}

	running += luaAdd;

#if EFI_ANTILAG_SYSTEM
if (engine->antilagController.isAntilagCondition) {
	running += engineConfiguration->ALSIdleAdd;
}
#endif /* EFI_ANTILAG_SYSTEM */

	// 'dashpot' (hold+decay) logic for coasting->idle
	float tpsForTaper = tps.value_or(0);
	efitimeus_t nowUs = getTimeNowUs();
	if (phase == Phase::Running) {
		lastTimeRunningUs = nowUs;
	}
	// imitate a slow pedal release for TPS taper (to avoid engine stalls)
	if (tpsForTaper <= engineConfiguration->idlePidDeactivationTpsThreshold) {
		// make sure the time is not zero
		float timeSinceRunningPhaseSecs = (nowUs - lastTimeRunningUs + 1) / US_PER_SECOND_F;
		// we shift the time to implement the hold correction (time can be negative)
		float timeSinceRunningAfterHoldSecs = timeSinceRunningPhaseSecs - engineConfiguration->iacByTpsHoldTime;
		// implement the decay correction (from tpsForTaper to 0)
		tpsForTaper = interpolateClamped(0, engineConfiguration->idlePidDeactivationTpsThreshold, engineConfiguration->iacByTpsDecayTime, tpsForTaper, timeSinceRunningAfterHoldSecs);
	}

	// Now bump it by the specified amount when the throttle is opened (if configured)
	// nb: invalid tps will make no change, no explicit check required
	iacByTpsTaper = interpolateClamped(
		0, 0,
		engineConfiguration->idlePidDeactivationTpsThreshold, engineConfiguration->iacByTpsTaper,
		tpsForTaper);

	running += iacByTpsTaper;


	float airTaperRpmUpperLimit = engineConfiguration->idlePidRpmUpperLimit;
	iacByRpmTaper = interpolateClamped(
		engineConfiguration->idlePidRpmUpperLimit, 0,
		airTaperRpmUpperLimit, engineConfiguration->airByRpmTaper,
		rpm);

	running += iacByRpmTaper;

  // are we clamping open loop part separately? should not we clamp once we have total value?
	return clampPercentValue(running);
}

/**
 * Top-level open-loop IAC computation. Picks between:
 *   - cranking position (engine still cranking)
 *   - dedicated coasting table (if enabled and we are coasting)
 *   - running open-loop, blended from cranking via the taper fraction
 */
percent_t IdleController::getOpenLoop(Phase phase, float rpm, float clt, SensorResult tps, float crankingTaperFraction) {
	isCranking = phase == Phase::Cranking;
	isIdleCoasting = phase == Phase::Coasting || (phase == Phase::Running && engineConfiguration->modeledFlowIdle);

	// During cranking with air amount table enabled — but not when RPM flare mode uses cranking air as an adder.
	if (isCranking && engineConfiguration->crankingAirAmountEnabled && !engineConfiguration->crankingIdleRpmFlareEnabled) {
		return getCrankingOpenLoop(clt);
	}

	// If coasting (and enabled), use the coasting position table instead of normal open loop
	isIacTableForCoasting = engineConfiguration->useIacTableForCoasting && isIdleCoasting;
	if (isIacTableForCoasting) {
		percent_t coastingPosition = interpolate2d(rpm, config->iacCoastingRpmBins, config->iacCoasting);

		// Add A/C offset if the A/C is on during coasting
		if (engine->module<AcController>().unmock().acButtonState) {
			coastingPosition += engineConfiguration->acIdleExtraOffset;
		}

		// We return here, bypassing the final interpolation, so we should clamp the value
		// to ensure it's a valid percentage.
		return clampPercentValue(coastingPosition);
	}

	percent_t running = getRunningOpenLoop(phase, rpm, clt, tps);

	// No air amount table: taper is handled via m_lastTargetRpm driving the 3D table; just return running.
	if (!engineConfiguration->crankingAirAmountEnabled) {
		return running;
	}

	// When RPM flare is also enabled, cranking air becomes a fading additive offset on top of the running
	// open loop, so the extra air decays in sync with the RPM target adder from the flare.
	if (engineConfiguration->crankingIdleRpmFlareEnabled) {
		float crankingAdder = interpolateClamped(0, getCrankingOpenLoop(clt), 1, 0.0f, crankingTaperFraction);
		return clampPercentValue(running + crankingAdder);
	}

	// Standard mode: blend from cranking duty to running over the crank taper.
	// This clamps once you fall off the end, so no explicit check for >1 required.
	return interpolateClamped(0, getCrankingOpenLoop(clt), 1, running, crankingTaperFraction);
}

float IdleController::getIdleTimingAdjustment(float rpm) {
	return getIdleTimingAdjustment(rpm, m_lastTargetRpm, m_lastPhase);
}

/**
 * Idle ignition-timing PID adjustment.
 *
 * Small timing tweaks react much faster than airflow changes through the IAC,
 * so we use a separate PID on ignition advance to hold RPM steady at idle.
 * Returns degrees of timing correction to add to the base advance.
 */
float IdleController::getIdleTimingAdjustment(float rpm, float targetRpm, Phase phase) {
	// if not enabled, do nothing
	if (!engineConfiguration->useIdleTimingPidControl) {
		return 0;
	}

	// If not idling, do nothing
	if (phase != Phase::Idling) {
		m_timingPid.reset();
		return 0;
	}

	if (engineConfiguration->idleTimingSoftEntryTime > 0.0f) {
		// Use interpolation for correction taper
		m_timingPid.setErrorAmplification(interpolateClamped(m_crankTaperEndTime, 0.0f, m_idleTimingSoftEntryEndTime, 1.0f, engine->fuelComputer.running.timeSinceCrankingInSecs));
	}

#if EFI_GHOST_CAM
	if (m_lastGhostCamActive) {
		// Ghost Cam replaces whatever closed-loop idle timing scheme is normally active (PID or
		// modeled-flow) with its own PID -- m_timingPid was already swapped to the page-6
		// ghostCamTimingPid_* gains in getIdlePosition() when ghost cam activated.
		return m_timingPid.getOutput(targetRpm, rpm, FAST_CALLBACK_PERIOD_MS / 1000.0f);
	}
#endif // EFI_GHOST_CAM

	if (engineConfiguration->modeledFlowIdle) {
		return m_modeledFlowIdleTiming;
	} else {
		// We're now in the idle mode, and RPM is inside the Timing-PID regulator work zone!
		return m_timingPid.getOutput(targetRpm, rpm, FAST_CALLBACK_PERIOD_MS / 1000.0f);
	}
}

static void finishIdleTestIfNeeded() {
	if (engine->timeToStopIdleTest != 0 && getTimeNowUs() > engine->timeToStopIdleTest)
		engine->timeToStopIdleTest = 0;
}

/**
 * @return idle valve position percentage for automatic closed loop mode
 */
float IdleController::getClosedLoop(IIdleController::Phase phase, float tpsPos, float rpm, float targetRpm) {
	auto idlePid = getIdlePid();

	if (shouldResetPid && !wasResetPid) {
		needReset = idlePid->getIntegration() <= 0 || shouldResetPid;
		// this is not-so valid since we have open loop first for this?
		// we reset only if I-term is negative, because the positive I-term is good - it keeps RPM from dropping too low
		if (needReset) {
			idlePid->reset();
		}
		shouldResetPid = false;
		wasResetPid = true;
	}

	// todo: move this to pid_s one day
	industrialWithOverrideIdlePid.antiwindupFreq = engineConfiguration->idle_antiwindupFreq;
	industrialWithOverrideIdlePid.derivativeFilterLoss = engineConfiguration->idle_derivativeFilterLoss;

	efitimeus_t nowUs = getTimeNowUs();

	isIdleClosedLoop = phase == IIdleController::Phase::Idling;

	if (!isIdleClosedLoop) {
		// Don't store old I and D terms if PID doesn't work anymore.
		// Otherwise they will affect the idle position much later, when the throttle is closed.¿
		shouldResetPid = true;
		idleState = TPS_THRESHOLD;

		// We aren't idling, so don't apply any correction.  A positive correction could inhibit a return to idle.
		m_lastAutomaticPosition = 0;
		return 0;
	}

	bool acToggleJustTouched = engine->module<AcController>().unmock().timeSinceStateChange.getElapsedSeconds() < 0.5f /*second*/;
	// check if within the dead zone
	isInDeadZone = !acToggleJustTouched && std::abs(rpm - targetRpm) <= engineConfiguration->idlePidRpmDeadZone;
	if (isInDeadZone) {
		idleState = RPM_DEAD_ZONE;
		// current RPM is close enough, no need to change anything
		return m_lastAutomaticPosition;
	}

	// When rpm < targetRpm, there's a risk of dropping RPM too low - and the engine dies out.
	// So PID reaction should be increased by adding extra percent to PID-error:
	percent_t errorAmpCoef = 1.0f;
	if (rpm < targetRpm) {
		errorAmpCoef += (float)engineConfiguration->pidExtraForLowRpm / PERCENT_MULT;
	}

	// if PID was previously reset, we store the time when it turned on back (see errorAmpCoef correction below)
	if (wasResetPid) {
		restoreAfterPidResetTimeUs = nowUs;
		wasResetPid = false;
	}
	// increase the errorAmpCoef slowly to restore the process correctly after the PID reset
	// todo: move restoreAfterPidResetTimeUs to idle?
	efitimeus_t timeSincePidResetUs = nowUs - restoreAfterPidResetTimeUs;
	// todo: add 'pidAfterResetDampingPeriodMs' setting
	errorAmpCoef = interpolateClamped(0, 0, MS2US(/*engineConfiguration->pidAfterResetDampingPeriodMs*/1000), errorAmpCoef, timeSincePidResetUs);
	// If errorAmpCoef > 1.0, then PID thinks that RPM is lower than it is, and controls IAC more aggressively
	idlePid->setErrorAmplification(errorAmpCoef);

	percent_t newValue = idlePid->getOutput(targetRpm, rpm, FAST_CALLBACK_PERIOD_MS / 1000.0f);
	idleState = PID_VALUE;

	// the state of PID has been changed, so we might reset it now, but only when needed (see idlePidDeactivationTpsThreshold)
	mightResetPid = true;

	// Apply PID Multiplier if used
	if (engineConfiguration->useIacPidMultTable) {
		float engineLoad = getFuelingLoad();
		float multCoef = interpolate3d(
			config->iacPidMultTable,
			config->iacPidMultLoadBins, engineLoad,
			config->iacPidMultRpmBins, rpm
		);
		// PID can be completely disabled of multCoef==0, or it just works as usual if multCoef==1
		newValue = interpolateClamped(0, 0, 1, newValue, multCoef);
	}

	// Apply PID Deactivation Threshold as a smooth taper for TPS transients.
	// if tps==0 then PID just works as usual, or we completely disable it if tps>=threshold
	// TODO: should we just remove this? It reduces the gain if your zero throttle stop isn't perfect,
	// which could give unstable results.
	newValue = interpolateClamped(0, newValue, engineConfiguration->idlePidDeactivationTpsThreshold, 0, tpsPos);

	m_lastAutomaticPosition = newValue;
	return newValue;
}

/**
 * Main entry point invoked on the fast callback. Orchestrates the whole idle pipeline:
 *   1. Read sensors (CLT, TPS, VSS)
 *   2. Compute target RPM and cranking taper
 *   3. Determine the current Phase (cranking/idling/coasting/running)
 *   4. Compute open-loop IAC position
 *   5. If automatic mode + valid TPS, add closed-loop PID correction
 *   6. For modeled-flow idle, convert airmass -> valve position and split high-frequency
 *      content to the timing PID
 * Returns the final IAC valve position percentage (0..100).
 */
float IdleController::getIdlePosition(float rpm) {
#if EFI_SHAFT_POSITION_INPUT

		// Simplify hardware CI: we borrow the idle valve controller as a PWM source for various stimulation tasks
		// The logic in this function is solidly unit tested, so it's not necessary to re-test the particulars on real hardware.
		#ifdef HARDWARE_CI
			return config->cltIdleCorrTable[0][0];
		#endif

		bool useModeledFlow = engineConfiguration->modeledFlowIdle;

	/*
	 * Here we have idle logic thread - actual stepper movement is implemented in a separate
	 * working thread see stepper.cpp
	 */
		getIdlePid()->iTermMin = engineConfiguration->idlerpmpid_iTermMin;
		getIdlePid()->iTermMax = engineConfiguration->idlerpmpid_iTermMax;

		// On failed sensor, use 0 deg C - should give a safe highish idle
		float clt = Sensor::getOrZero(SensorType::Clt);
		auto tps = Sensor::get(SensorType::DriverThrottleIntent);

		// Compute the target we're shooting for
		auto targetRpm = getTargetRpm(clt);

		// Determine cranking taper (modeled flow does no taper of open loop)
		float crankingTaper = useModeledFlow ? 1 : getCrankingTaperFraction(clt);

		// Determine what operation phase we're in - idling or not
		float vehicleSpeed = Sensor::getOrZero(SensorType::VehicleSpeed);
		auto phase = determinePhase(rpm, targetRpm, tps, vehicleSpeed, crankingTaper);

		// update TS flag
		isIdling = (phase == Phase::Idling) || (phase == Phase::CrankToIdleTaper);

    if (phase != m_lastPhase && phase == Phase::Idling) {
        // Just entered idle, reset timer
 		    m_timeInIdlePhase.reset();
 	  }

		m_lastPhase = phase;

#if EFI_GHOST_CAM
		{
			bool ghostCamActive = engine->module<EngineStateMachine>().unmock().engineSmIsGhostCam;
			if (m_lastGhostCamActive && !ghostCamActive) {
				// Ghost cam deactivated: reset CLID integrator and restore normal timing PID config.
				getIdlePid()->reset();
				m_timingPid.initPidClass(&engineConfiguration->idleTimingPid);
			} else if (!m_lastGhostCamActive && ghostCamActive) {
				// Ghost cam activated: build pid_s from individual page-6 fields and switch.
				auto* p = getCustomPage();
				m_ghostCamTimingPidConfig.pFactor  = p->ghostCamTimingPid_pFactor;
				m_ghostCamTimingPidConfig.iFactor  = p->ghostCamTimingPid_iFactor;
				m_ghostCamTimingPidConfig.dFactor  = p->ghostCamTimingPid_dFactor;
				m_ghostCamTimingPidConfig.minValue = p->ghostCamTimingPid_minValue;
				m_ghostCamTimingPidConfig.maxValue = p->ghostCamTimingPid_maxValue;
				m_ghostCamTimingPidConfig.offset   = 0;
				m_ghostCamTimingPidConfig.periodMs = 0;
				m_timingPid.initPidClass(&m_ghostCamTimingPidConfig);
			}
			m_lastGhostCamActive = ghostCamActive;
		}
#endif // EFI_GHOST_CAM

		// CLT-based RPM adder during cranking, tapered to zero as crankingTaper → 1.
		// Use targetRpm.ClosedLoopTarget (fresh from getTargetRpm() this call) as the base so
		// the adder is not accumulated across iterations via m_lastTargetRpm.
		if (engineConfiguration->crankingIdleRpmFlareEnabled &&
		    (phase == Phase::Cranking || phase == Phase::CrankToIdleTaper)) {
			float rpmAdder = interpolate2d(clt, config->cltCrankingCorrBins, config->cltCrankingRpmAdder);
			float baseIdle = targetRpm.ClosedLoopTarget;
			float crankingRpmTarget = baseIdle + rpmAdder;
			m_lastTargetRpm = interpolateClamped(0, crankingRpmTarget, 1, baseIdle, crankingTaper);
			targetRpm.ClosedLoopTarget = m_lastTargetRpm;
		}

		// Apply off-idle RPM adder to the closed-loop target
		float offIdleAdder = getOffIdleAdder(phase, rpm);
		targetRpm.ClosedLoopTarget += offIdleAdder;
		m_lastTargetRpm = targetRpm.ClosedLoopTarget;
		// Only update the displayed idleTarget when the engine is running. During key-off
		// (Phase::Cranking) the flare block may have set targetRpm.ClosedLoopTarget to a
		// garbage value from an uninitialised cltCrankingRpmAdder table, so we leave
		// idleTarget at the base value written by getTargetRpm() instead.
		if (phase != Phase::Cranking) {
			idleTarget = static_cast<uint16_t>(targetRpm.ClosedLoopTarget);
		}
		offIdleAdderCurrentRpm = static_cast<int16_t>(offIdleAdder);
		offIdleAdderStateIndex = static_cast<uint8_t>(m_offIdlePhase);

		finishIdleTestIfNeeded();

			// Always apply open loop correction
		percent_t iacPosition = getOpenLoop(phase, rpm, clt, tps, crankingTaper);
			baseIdlePosition = iacPosition;

#if EFI_GHOST_CAM
		if (m_lastGhostCamActive && phase == Phase::Idling) {
			iacPosition = getCustomPage()->ghostCamIdleBaseDuty;
			baseIdlePosition = iacPosition;
		}
#endif // EFI_GHOST_CAM

			// Force closed loop operation for modeled flow
			auto idleMode = useModeledFlow ? IM_AUTO : engineConfiguration->idleMode;

			// If TPS is working and automatic mode enabled, add any closed loop correction
			if (tps.Valid && idleMode == IM_AUTO) {
				if (useModeledFlow && phase != Phase::Idling) {
					auto idlePid = getIdlePid();
					idlePid->reset();
				}
				auto closedLoop = getClosedLoop(phase, tps.Value, rpm, targetRpm.ClosedLoopTarget);
				idleClosedLoop = closedLoop;
				iacPosition += closedLoop;
			} else {
			  isIdleClosedLoop = false;
			}

			if (engine->module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs) {
				iacPosition += getCustomPage()->popsAndBangsAirAdd;
			}

			// Quick Warmup: feed-forward IAC offset to compensate for torque lost to timing retard + rich target.
			if (engine->module<EngineStateMachine>().unmock().engineSmIsQuickWarmup) {
				iacPosition += getCustomPage()->quickWarmupEtbOffset;
			}

			iacPosition = clampPercentValue(iacPosition);

// todo: while is below disabled for unit tests?
#if EFI_TUNER_STUDIO && (EFI_PROD_CODE || EFI_SIMULATOR)

			// see also tsOutputChannels->idlePosition
			getIdlePid()->postState(engine->outputChannels.idleStatus);


		extern StepperMotor iacMotor;
		engine->outputChannels.idleStepperTargetPosition = iacMotor.getTargetPosition();
#endif /* EFI_TUNER_STUDIO */
		if (useModeledFlow && phase != Phase::Cranking) {
			float totalAirmass = 0.01 * iacPosition * engineConfiguration->idleMaximumAirmass;
			idleTargetAirmass = totalAirmass;

			bool shouldAdjustTiming = engineConfiguration->useIdleTimingPidControl && phase == Phase::Idling;

			// extract hiqh frequency content to be handled by timing
			float timingAirmass = shouldAdjustTiming ? m_timingHpf.filter(totalAirmass) : 0;

			// Convert from airmass delta -> timing
			m_modeledFlowIdleTiming = interpolate2d(timingAirmass, engineConfiguration->airmassToTimingBins, engineConfiguration->airmassToTimingValues);

			// Handle the residual low frequency content with airflow
			float idleAirmass = totalAirmass - timingAirmass;
			float airflowKgPerH = 3.6 * 0.001 * idleAirmass * rpm / 60 * engineConfiguration->cylindersCount / 2;
			idleTargetFlow = airflowKgPerH;

			// Convert from desired flow -> idle valve position
			float idlePos = interpolate2d(
				airflowKgPerH,
				engineConfiguration->idleFlowEstimateFlow,
				engineConfiguration->idleFlowEstimatePosition
			);

			iacPosition = idlePos;
		}

		currentIdlePosition = iacPosition;

	bool acActive = engine->module<AcController>().unmock().acButtonState;
	bool fan1Active = enginePins.fanRelay.getLogicValue();
	bool fan2Active = enginePins.fanRelay2.getLogicValue();
	updateLtit(rpm, clt, acActive, fan1Active, fan2Active, getIdlePid()->getIntegration());

		return iacPosition;
#else
		return 0;
#endif // EFI_SHAFT_POSITION_INPUT

}

// Periodic tick from the fast callback scheduler: recompute idle position and push it to hardware.
void IdleController::onFastCallback() {
#if EFI_SHAFT_POSITION_INPUT
	float position = getIdlePosition(engine->triggerCentral.instantRpm.getInstantRpm());
	// Send the resulting percentage to the actual IAC actuator (stepper / DC motor / PWM solenoid)
	applyIACposition(position);
	// huh: why not onIgnitionStateChanged?
	engine->m_ltit.checkIfShouldSave();
#endif // EFI_SHAFT_POSITION_INPUT
}

void IdleController::onEngineStop() {
	getIdlePid()->reset();
}

void IdleController::onConfigurationChange(engine_configuration_s const * previousConfiguration) {
#if ! EFI_UNIT_TEST
	shouldResetPid = !previousConfiguration || !getIdlePid()->isSame(&previousConfiguration->idleRpmPid);
#endif
}

/**
 * One-time controller initialization: wire up the timing-PID, the high-pass filter used
 * by modeled-flow idle, and the main RPM idle PID.
 */
void IdleController::init() {
	shouldResetPid = false;
	mightResetPid = false;
	wasResetPid = false;
	m_timingPid.initPidClass(&engineConfiguration->idleTimingPid);
	m_timingHpf.configureHighpass(20, 1);
	getIdlePid()->initPidClass(&engineConfiguration->idleRpmPid);
	engine->m_ltit.loadLtitFromConfig();
}

void IdleController::updateLtit(float rpm, float clt, bool acActive, bool fan1Active, bool fan2Active, float idleIntegral) {
	if (engineConfiguration->ltitEnabled) {
		engine->m_ltit.update(rpm, clt, acActive, fan1Active, fan2Active, idleIntegral);
	}
}

void IdleController::onIgnitionStateChanged(bool ignitionOn) {
    engine->m_ltit.onIgnitionStateChanged(ignitionOn);
}

#endif /* EFI_IDLE_CONTROL */
