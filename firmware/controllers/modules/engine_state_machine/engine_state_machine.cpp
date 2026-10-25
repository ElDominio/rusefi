#include "pch.h"
#include "custom_page.h"
#include "engine_state_machine.h"
#include "electronic_throttle.h"
#include "malfunction_central.h"
#include "dfco.h"
#include "exhaust_cutout.h"
#include "tinymt32.h" // basic 'random' for the P&B automatic cut duration

// Lower/upper bounds (ms) for the P&B spark-cut window, also used by the 'automatic' random mode.
static constexpr float PNB_CUT_MIN_MS = 20.0f;
static constexpr float PNB_CUT_MAX_MS = 500.0f;

void EngineStateMachine::setDefaultConfiguration() {
	engineConfiguration->useEngineStateMachine = false;
	engineConfiguration->stateBasedStft = false;
	engineConfiguration->smWotTpsThreshold = 90;
	engineConfiguration->smTransientHoldTimeMs = 200;
	engineConfiguration->smUpshiftClutchSwitch = sm_clutch_switch_e::None;
	engineConfiguration->smDownshiftClutchSwitch = sm_clutch_switch_e::None;
	engineConfiguration->smShiftDetectionMode = sm_shift_detection_mode_e::RpmRate;
	engineConfiguration->smSecondClutchSwitchAvailable = false;
	engineConfiguration->smRelatchOnClutchDown = false;
	engineConfiguration->smAccelRateThreshold = 0;
	engineConfiguration->smDecelRateThreshold = 0;
	engineConfiguration->smRpmRateWindowMs = 100;
	engineConfiguration->smShiftLatchTimeMs = 0;
	engineConfiguration->smShiftMinVss = 0;
	engineConfiguration->smAccumulatorUpshiftThreshold = 40;
	engineConfiguration->smAccumulatorDownshiftThreshold = 40;
	engineConfiguration->smAccelHoldMs = 0;
	engineConfiguration->smAccumulatorSnapTimeMs = 0;
	engineConfiguration->smAccumulatorGain = 20;
	engineConfiguration->smAccumulatorDecayRate = 10;
	engineConfiguration->smColdTempThreshold = 60;
	engineConfiguration->smHotTempThreshold = 100;
}

void EngineStateMachine::reset() {
	static_cast<engine_state_machine_state_s&>(*this) = {};
	engineSmEnabled = isEnabled();
	engineSmIsOff = true;
	m_currentState = EngineStateMachineState::Off;
	m_transientHoldTimer.init();
	m_accelHoldTimer.init();
	m_accelStateTimer.init();
	m_decelStateTimer.init();
	m_shiftWindowOpen = false;
	m_shiftIsUpshift = false;
	m_shiftWindowOpenMs = 0;
	m_prevClutchUp = false;
	m_prevClutchDown = false;
	m_clutchHasSample = false;
	m_shiftLatched = false;
	m_shiftLatchedIsUpshift = false;
	m_shiftLatchUntilMs = 0;
	m_shiftAccumulator = 0;
	m_accumulatorLastMs = 0;
	m_accumulatorLastRpm = 0;
	m_accumulatorLastVss = 0;
	m_accumulatorHasSample = false;
	m_lastRpmRate = 0;
	m_rpmRateAnchorMs = 0;
	m_rpmRateAnchorRpm = 0;
	m_rpmRateHasAnchor = false;
}

void EngineStateMachine::onEngineStop() {
	reset();
}

void EngineStateMachine::onConfigurationChange(engine_configuration_s const*) {
	reset();
}

bool EngineStateMachine::isEnabled() const {
	return engineConfiguration->useEngineStateMachine;
}

EngineStateMachineState EngineStateMachine::getCurrentState() const {
	return m_currentState;
}

void EngineStateMachine::reportLimpCondition() {
	m_limpModeLatched = true;
}

void EngineStateMachine::onSlowCallback() {
	if (!isEnabled()) {
		reset();
		return;
	}

	// Do not carry timers, rate samples or shift windows through a stop/restart.
	bool running = engine->rpmCalculator.isRunning();
	if (!running) {
		reset();
	}
	engineSmEnabled = true;

	efitimems_t nowMs = getTimeNowMs();
	float rpm = engine->rpmCalculator.getCachedRpm();

	auto tpsSensor = Sensor::get(SensorType::DriverThrottleIntent);
	float tps = tpsSensor ? tpsSensor.Value : 0.0f;

	auto vssSensor = Sensor::get(SensorType::VehicleSpeed);
	float vss = vssSensor.value_or(0);
	bool vssValid = vssSensor.Valid;

	// Primary state determination (engine lifecycle, throttle position)
	EngineStateMachineState newState = determineState(rpm, tps, vss, vssValid);
	m_currentState = newState;

	uint8_t stateNum = static_cast<uint8_t>(newState);
	engineSmCurrentState = stateNum;

	engineSmIsOff          = (newState == EngineStateMachineState::Off);
	engineSmIsCranking     = (newState == EngineStateMachineState::Cranking);
	engineSmIsAfterStart   = (newState == EngineStateMachineState::Afterstart);
	engineSmIsIdle         = (newState == EngineStateMachineState::Idle);
	engineSmIsCoasting     = (newState == EngineStateMachineState::Coasting);
	engineSmIsTransient    = (newState == EngineStateMachineState::Transient);
	engineSmIsWot          = (newState == EngineStateMachineState::WOT);
	engineSmIsCruising     = (newState == EngineStateMachineState::Cruising);
	engineSmIsOverrun      = (newState == EngineStateMachineState::Overrun);
	engineSmIsAccelerating = (newState == EngineStateMachineState::Accelerating);
	engineSmIsDecelerating = (newState == EngineStateMachineState::Decelerating);

	if (!running) {
		updateTempOverlay();
		return;
	}

#if EFI_LAUNCH_CONTROL
	// Overlay: Launch Control — forward directly from LaunchControlBase
	engineSmIsLaunchControl = engine->launchController.isLaunchCondition;

	// Overlay: Torque Reduction + Shift direction
	// Flat-shift torque reduction is an authoritative upshift signal: the driver stayed WOT
	// throughout the shift, so direction is certain and no clutch analysis is needed.
	bool flatShiftActive = engine->shiftTorqueReductionController.isCuttingTorque();
	// Traction control also reduces torque (ETB / timing / spark cut) but is not a gear shift,
	// so it raises the Torque Reduction overlay without driving shift direction below.
	bool tractionActive = engine->tractionController.isActive();
	engineSmIsTorqueReduction = flatShiftActive || tractionActive;
#else
	bool flatShiftActive = false;
#endif

	// RPM classification must continue without shift switches and during a flat shift.
	updateShiftAccumulator(rpm, vss, vssValid, nowMs);
	if (flatShiftActive) {
		engineSmIsUpshifting   = true;
		engineSmIsDownshifting = false;
		// Abort any open clutch-detection window/latch so it doesn't fight the flat-shift signal
		m_shiftWindowOpen = false;
		m_shiftLatched    = false;
		m_clutchHasSample = false;
	} else {
		updateShiftDetection(vss, vssValid, nowMs);
	}

	// Overlay: Limp Mode — explicit latched protective state.
	//
	// Limp is a dedicated latch (m_limpModeLatched) raised only via reportLimpCondition(),
	// currently by an ETB jam (LimpManager::reportEtbJammed). The ordinary rev limit and other
	// transient cuts intentionally do NOT enter limp mode.

	engineSmIsLimp = m_limpModeLatched;

	// Sport Mode overlay — shared "driver wants aggressive behavior" flag consumed by Ghost Cam,
	// Sport Pedal, Exhaust Cutout, Pops & Bangs, Downshift Blip, and the Limiter Adders. Priority
	// chain: Limp out-votes Sport Mode (protective), Sport Mode out-votes Eco Mode (mutually
	// exclusive). Must run after engineSmIsLimp is set (above), and before updateEcoMode() (eco
	// reads engineSmIsSportMode), updateGhostCam() (Ghost Cam's Sport Mode activation source reads
	// it), and updatePopsAndBangs() at the end of this function.
	updateSportMode();

	// Eco mode overlay — must run before the display override below so the dash reflects it
	// this cycle. Depends on m_currentState (set above), engineSmIsLimp (limp wins), and
	// engineSmIsSportMode (sport wins — the two are mutually exclusive).
	updateEcoMode(m_currentState);

	// Arm a settle holdoff on an eco engage/disengage edge: the ecoThrottleMult/VVT step that
	// follows is a real RPM transient but not driver-initiated, and determineState() must not
	// misread it as Accelerating/Decelerating next tick or eco would immediately bounce itself
	// back off (see m_ecoSettleHoldTimer). Skipped when the edge itself was caused by a
	// genuine, already-detected Accelerating/Decelerating this tick -- that's real driver input,
	// not eco noise, and masking further detection would just blind us to its continuation.
	if (engineSmIsEcoMode != m_prevEcoModeActive
			&& m_currentState != EngineStateMachineState::Accelerating
			&& m_currentState != EngineStateMachineState::Decelerating) {
		m_ecoSettleHoldTimer.reset();
	}
	m_prevEcoModeActive = engineSmIsEcoMode;

	// Ghost Cam overlay — idle-only manual lope effect via switch or Sport Mode.
	// Must run after engineSmIsIdle, engineSmIsLimp and engineSmIsSportMode are set.
	updateGhostCam();

	// Overlay-target transition blend — must run after both engineSmIsEcoMode and
	// engineSmIsGhostCam are finalized above (see getEcoModeBlend()/getGhostCamBlend()).
	{
		efitimems_t blendDtMs = (m_lastBlendTickMs == 0) ? 0 : (nowMs - m_lastBlendTickMs);
		m_lastBlendTickMs = nowMs;
		updateOverlayBlend(m_ecoModeBlend, engineSmIsEcoMode, blendDtMs);
		updateOverlayBlend(m_ghostCamBlend, engineSmIsGhostCam, blendDtMs);
	}

	// Sport Pedal overlay — ETB pedal-to-throttle ratio shaping via switch or Lua gauge.
	updateSportPedal();

	// Temperature overlay — three mutually-exclusive CLT bands published as bits.
	// Independent of all other state; falls back to Operating when no CLT sensor is valid.
	updateTempOverlay();

	// Quick Warmup overlay — active when Cold AND Idle (not cranking, not after-start).
	engineSmIsQuickWarmup = getCustomPage()->quickWarmupEnabled
	                     && engineSmIsCold
	                     && engineSmIsIdle;

	// Override the display integer for overlay priority: Limp > Upshifting > Downshifting > LaunchControl > Eco
	// Ghost Cam is intentionally NOT in this chain: it only ever runs while engineSmIsIdle is true
	// (see updateGhostCam()), so it stays an overlay on top of Idle rather than a state you "enter" -
	// the dash keeps showing Idle and engineSmIsGhostCam conveys the overlay separately.
	if (engineSmIsLimp) {
		engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::Limp);
	} else if (engineSmIsUpshifting) {
		engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::Upshifting);
	} else if (engineSmIsDownshifting) {
		engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::Downshifting);
	} else if (engineSmIsLaunchControl) {
		engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::LaunchControl);
	} else if (engineSmIsEcoMode) {
		engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::Eco);
	}

	updatePopsAndBangs(engineSmIsOverrun);
}

void EngineStateMachine::updateEcoMode(EngineStateMachineState currentState) {
	// Limp mode is protective and out-votes eco; Sport Mode is the driver's explicit request for
	// aggressive behavior and is mutually exclusive with eco, so it out-votes eco too. A disabled
	// feature is always inactive. These override even an active engage lock below -- safety and
	// explicit driver intent always win.
	if (!getCustomPage()->ecoModeEnabled || engineSmIsLimp || engineSmIsSportMode) {
		m_ecoCruiseTimer.reset();
		engineSmIsEcoMode = false;
		return;
	}

	// Inhibit also overrides the engage lock below -- the driver's explicit "no eco" request wins.
	bool inhibited = isEcoModeInhibited();

	// Post-engage lock: once eco is active, hold it forced on and skip all gate/state
	// re-evaluation for ecoModeEngageLockTime. Eco's own AFR/timing/VVT/throttle step is a real
	// change in torque delivery, and re-checking Cruising/MAP/VSS/RPM against the resulting
	// transient could misread it as "left Cruising" and immediately drop eco right back off.
	// 0 disables the lock (instant re-evaluation, same as before this existed).
	if (engineSmIsEcoMode && !inhibited
			&& !m_ecoLockTimer.hasElapsedSec(getCustomPage()->ecoModeEngageLockTime)) {
		return;
	}

	// Instant drop: Cruising and brief AE-driven Transient blips both accumulate time -- a
	// throttle flutter that immediately settles back to Cruising is AE noise, not a driver-
	// initiated tip-in/tip-out, and shouldn't cost eco its progress. Any other state (genuine
	// Accelerating/Decelerating/Overrun/etc.) pins the timer at zero, so re-entry must
	// accumulate the full ecoModeCruisingTime again before eco re-engages.
	bool cruiseElapsed = false;
	if (currentState == EngineStateMachineState::Cruising || currentState == EngineStateMachineState::Transient) {
		cruiseElapsed = m_ecoCruiseTimer.hasElapsedSec(getCustomPage()->ecoModeCruisingTime);
	} else {
		m_ecoCruiseTimer.reset();
	}

	// MAP gate: load creeping up above the limit drops eco exactly like leaving Cruising, even if
	// the state machine still reports Cruising (TPS-based) this cycle. 0 disables the gate.
	uint16_t mapLimit = getCustomPage()->ecoModeMapLimit;
	if (mapLimit > 0 && Sensor::get(SensorType::Map).value_or(0) > mapLimit) {
		m_ecoCruiseTimer.reset();
		cruiseElapsed = false;
	}

	// VSS floor: below this speed eco cannot engage regardless of state or MAP. Catches
	// low-speed parking-lot crawls that the cruise timer alone would eventually allow. 0 disables.
	uint16_t minVss = getCustomPage()->ecoModeMinVss;
	if (minVss > 0 && Sensor::getOrZero(SensorType::VehicleSpeed) < static_cast<float>(minVss)) {
		m_ecoCruiseTimer.reset();
		cruiseElapsed = false;
	}

	// RPM ceiling: eco has no business engaging (or staying engaged) above this RPM -- Cruising
	// can still be reported there (TPS-based), but it's no longer the steady-state economy cruise
	// eco targets. 0 disables the gate.
	uint16_t maxRpm = getCustomPage()->ecoModeMaxRpm;
	if (maxRpm > 0 && Sensor::getOrZero(SensorType::Rpm) > static_cast<float>(maxRpm)) {
		m_ecoCruiseTimer.reset();
		cruiseElapsed = false;
	}

	bool newEcoActive = cruiseElapsed && !inhibited;

	// Rising edge: (re-)arm the post-engage lock window.
	if (newEcoActive && !engineSmIsEcoMode) {
		m_ecoLockTimer.reset();
	}

	// Inhibit blocks the timer-based engage; there is no Force-On override of the timer/MAP/RPM gate.
	engineSmIsEcoMode = newEcoActive;
}

void EngineStateMachine::updateTempOverlay() {
	auto clt = Sensor::get(SensorType::Clt);
	if (!clt) {
		// No valid CLT — treat as Operating so no false Cold/Hot trigger.
		engineSmIsCold      = false;
		engineSmIsOperating = true;
		engineSmIsHot       = false;
		return;
	}

	float temp = clt.Value;
	engineSmIsCold      = (temp < engineConfiguration->smColdTempThreshold);
	engineSmIsHot       = (temp > engineConfiguration->smHotTempThreshold);
	engineSmIsOperating = !engineSmIsCold && !engineSmIsHot;
}

// Reads the configured manual switch source (hardware pin and/or Lua gauge). Either asserting
// counts as asserted; mirrors the pin + Lua-gauge reading pattern in isPopsAndBangsBlocked().
bool EngineStateMachine::isEcoModeSwitchAsserted() const {
	bool pinAsserted = false;

#if !EFI_UNIT_TEST
	const switch_input_pin_e pin = getCustomPage()->ecoModeSwitchPin;
	if (isBrainPinValid(pin)) {
		pinAsserted = efiReadPin(pin, getCustomPage()->ecoModeSwitchPinMode);
	}
#endif

	bool gaugeAsserted = false;
	SensorType gaugeType = SensorType::LuaGauge1;
	switch (getCustomPage()->ecoModeLuaGauge) {
		case LUA_GAUGE_2: gaugeType = SensorType::LuaGauge2; break;
		case LUA_GAUGE_3: gaugeType = SensorType::LuaGauge3; break;
		case LUA_GAUGE_4: gaugeType = SensorType::LuaGauge4; break;
		case LUA_GAUGE_5: gaugeType = SensorType::LuaGauge5; break;
		case LUA_GAUGE_6: gaugeType = SensorType::LuaGauge6; break;
		case LUA_GAUGE_7: gaugeType = SensorType::LuaGauge7; break;
		case LUA_GAUGE_8: gaugeType = SensorType::LuaGauge8; break;
		default: break;
	}
	const auto gaugeResult = Sensor::get(gaugeType);
	if (gaugeResult.Valid) {
		const float value = gaugeResult.Value;
		const float threshold = getCustomPage()->ecoModeLuaGaugeValue;
		switch (getCustomPage()->ecoModeLuaGaugeMeaning) {
			case LUA_GAUGE_LOWER_BOUND: gaugeAsserted = (value >= threshold); break;
			case LUA_GAUGE_UPPER_BOUND: gaugeAsserted = (value <= threshold); break;
		}
	}

	return pinAsserted || gaugeAsserted;
}

bool EngineStateMachine::isEcoModeInhibited() const {
	return getCustomPage()->ecoModeSwitchMode == eco_mode_switch_mode_e::Inhibit
		&& isEcoModeSwitchAsserted();
}

// Sport Mode: shared "driver wants aggressive behavior" activation source, either a hardware
// switch or a Lua gauge compared against a threshold. Mirrors the pin + Lua-gauge reading
// pattern used by Sport Pedal / Exhaust Cutout (see isSportPedalActive() /
// ExhaustCutoutController::getInputHigh()). Limp mode is protective and out-votes Sport Mode,
// same as it out-votes Eco Mode.
void EngineStateMachine::updateSportMode() {
	if (engineSmIsLimp) {
		engineSmIsSportMode = false;
		return;
	}

	auto mode = getCustomPage()->smSportModeActivationMode;

	if (mode == SPORT_MODE_SWITCH) {
		bool switchAsserted = false;
#if !EFI_UNIT_TEST
		const switch_input_pin_e pin = getCustomPage()->smSportModeSwitchPin;
		if (isBrainPinValid(pin)) {
			switchAsserted = efiReadPin(pin, getCustomPage()->smSportModeSwitchPinMode);
		}
#endif
		engineSmIsSportMode = switchAsserted;
		return;
	}

	if (mode == SPORT_MODE_LUA_GAUGE) {
		SensorType gaugeType = SensorType::LuaGauge1;
		switch (getCustomPage()->smSportModeLuaGauge) {
			case LUA_GAUGE_2: gaugeType = SensorType::LuaGauge2; break;
			case LUA_GAUGE_3: gaugeType = SensorType::LuaGauge3; break;
			case LUA_GAUGE_4: gaugeType = SensorType::LuaGauge4; break;
			case LUA_GAUGE_5: gaugeType = SensorType::LuaGauge5; break;
			case LUA_GAUGE_6: gaugeType = SensorType::LuaGauge6; break;
			case LUA_GAUGE_7: gaugeType = SensorType::LuaGauge7; break;
			case LUA_GAUGE_8: gaugeType = SensorType::LuaGauge8; break;
			default: break;
		}
		const auto gaugeResult = Sensor::get(gaugeType);
		bool gaugeAsserted = false;
		if (gaugeResult.Valid) {
			const float value = gaugeResult.Value;
			const float threshold = getCustomPage()->smSportModeLuaGaugeThreshold;
			switch (getCustomPage()->smSportModeLuaGaugeMeaning) {
				case LUA_GAUGE_LOWER_BOUND: gaugeAsserted = (value >= threshold); break;
				case LUA_GAUGE_UPPER_BOUND: gaugeAsserted = (value <= threshold); break;
			}
		}
		engineSmIsSportMode = gaugeAsserted;
		return;
	}

	engineSmIsSportMode = false; // SPORT_MODE_OFF
}

void EngineStateMachine::updateGhostCam() {
#if EFI_GHOST_CAM
	// Requires idle state and non-limp; feature must be enabled.
	if (!getCustomPage()->ghostCamEnabled || !engineSmIsIdle || engineSmIsLimp) {
		engineSmIsGhostCam = false;
		return;
	}

	// CLT gate: don't engage below minimum coolant temperature.
	auto clt = Sensor::get(SensorType::Clt);
	if (clt && clt.Value < static_cast<float>(getCustomPage()->ghostCamCltMin)) {
		engineSmIsGhostCam = false;
		return;
	}

	// Activation: either the dedicated switch pin or Sport Mode, selected by
	// ghostCamActivationSource (0=Switch, 1=Sport Mode).
	if (getCustomPage()->ghostCamActivationSource == 0) {
		bool switchAsserted = false;
#if !EFI_UNIT_TEST
		const switch_input_pin_e pin = getCustomPage()->ghostCamActivatePin;
		if (isBrainPinValid(pin)) {
			switchAsserted = efiReadPin(pin, getCustomPage()->ghostCamActivatePinMode);
		}
#endif
		engineSmIsGhostCam = switchAsserted;
	} else {
		engineSmIsGhostCam = engineSmIsSportMode;
	}
#else // !EFI_GHOST_CAM
	engineSmIsGhostCam = false;
#endif // EFI_GHOST_CAM
}

// Steps `blend` toward 1.0 (active) or 0.0 (!active) by dtMs/SM_SLOW_TRANSITION_MS, or snaps
// directly to the target when smSlowStateTransitionEnabled is off (legacy instant behavior) or on
// the very first tick (dtMs == 0, no elapsed-time reference yet).
void EngineStateMachine::updateOverlayBlend(float& blend, bool active, efitimems_t dtMs) {
	float targetBlend = active ? 1.0f : 0.0f;

	if (!getCustomPage()->smSlowStateTransitionEnabled || dtMs <= 0) {
		blend = targetBlend;
		return;
	}

	float step = dtMs / SM_SLOW_TRANSITION_MS;
	if (blend < targetBlend) {
		blend = std::min(targetBlend, blend + step);
	} else if (blend > targetBlend) {
		blend = std::max(targetBlend, blend - step);
	}
}

void EngineStateMachine::updateSportPedal() {
#if EFI_SPORT_PEDAL
	engineSmIsSportPedal = isSportPedalActive();
#else
	engineSmIsSportPedal = false;
#endif
}

void EngineStateMachine::updatePopsAndBangs(bool isOverrun) {
	if (!engineConfiguration->popsAndBangsEnabled) {
		m_pnbState = PopsAndBangsState::Inactive;
		engineSmIsPopsAndBangs = false;
		return;
	}

	float rpm = Sensor::getOrZero(SensorType::Rpm);

	if (isOverrun) {
		if (!m_wasPnbOverrun) {
			m_pnbOverrunTimer.reset();
			m_pnbState = PopsAndBangsState::Inactive;
		}

		if (m_pnbState == PopsAndBangsState::Inactive) {
			if (rpm > getCustomPage()->popsAndBangsRpmHigh &&
			    rpm < getCustomPage()->popsAndBangsRpmMax &&
			    m_pnbOverrunTimer.hasElapsedSec(getCustomPage()->popsAndBangsDelay)) {
				const auto clt = Sensor::get(SensorType::Clt);
				bool cltOk = clt &&
					clt.Value >= getCustomPage()->popsAndBangsCltMin &&
					clt.Value <= getCustomPage()->popsAndBangsCltMax;
				if (cltOk && !isPopsAndBangsBlocked()) {
					m_pnbState = PopsAndBangsState::Active;
					m_pnbActiveTimer.reset();
					// Start the spark-cut cycle from a clean firing phase.
					m_pnbCutActive = false;
					m_pnbLastFireRev = getRevolutionCounter();
				} else {
					m_pnbState = PopsAndBangsState::Expired;
				}
			}
		} else if (m_pnbState == PopsAndBangsState::Active) {
			if (rpm < getCustomPage()->popsAndBangsRpmLow ||
			    rpm > getCustomPage()->popsAndBangsRpmMax) {
				m_pnbState = PopsAndBangsState::Expired;
			}

			float duration = getCustomPage()->popsAndBangsDuration;
			if (duration > 0 && m_pnbActiveTimer.hasElapsedSec(duration)) {
				m_pnbState = PopsAndBangsState::Expired;
			}

			if (isPopsAndBangsBlocked()) {
				m_pnbState = PopsAndBangsState::Expired;
			}
		}
	} else {
		m_pnbState = PopsAndBangsState::Inactive;
	}

	m_wasPnbOverrun = isOverrun;
	engineSmIsPopsAndBangs = (m_pnbState == PopsAndBangsState::Active);

	if (!engineSmIsPopsAndBangs) {
		// P&B no longer active — make sure no stale cut window is left open.
		m_pnbCutActive = false;
		engineSmPnbSparkCut = false;
	}
}

static tinymt32_t pnbRandom;

// Returns a fresh random cut-window duration within [PNB_CUT_MIN_MS, PNB_CUT_MAX_MS].
static float pnbRandomCutDurationMs() {
	static bool inited = false;
	if (!inited) {
		tinymt32_init(&pnbRandom, 0x50616E42 /* 'PanB' */);
		inited = true;
	}
	// tinymt32_generate_float returns [0, 1)
	float r = tinymt32_generate_float(&pnbRandom);
	return PNB_CUT_MIN_MS + r * (PNB_CUT_MAX_MS - PNB_CUT_MIN_MS);
}

// Skip ratio (0..1) applied while a cut window is open.
static float pnbCutRatio() {
	return clampF(0.0f, getCustomPage()->popsAndBangsCutPercent, 100.0f) / 100.0f;
}

float EngineStateMachine::getPopsAndBangsSparkSkipRatio() {
	if (!engineSmIsPopsAndBangs || !getCustomPage()->popsAndBangsSparkCutEnabled) {
		m_pnbCutActive = false;
		engineSmPnbSparkCut = false;
		return 0.0f;
	}

	const uint32_t rev = getRevolutionCounter();

	if (m_pnbCutActive) {
		// Still inside the current cut window? Keep cutting spark so fuel charges the exhaust.
		if (m_pnbCutWindowTimer.getElapsedSeconds() * 1000.0f < m_pnbCutDurationMs) {
			engineSmPnbSparkCut = true;
			return pnbCutRatio();
		}
		// Window elapsed — resume firing (re-lights the charge) and restart the rev count.
		m_pnbCutActive = false;
		m_pnbLastFireRev = rev;
	}

	// Firing normally until the configured number of revolutions have elapsed.
	uint8_t everyRevs = getCustomPage()->popsAndBangsCutEveryRevs;
	if (everyRevs < 1) {
		everyRevs = 1;
	}

	if (rev - m_pnbLastFireRev >= everyRevs) {
		// Open a new spark-cut window.
		m_pnbCutActive = true;
		m_pnbCutWindowTimer.reset();
		if (getCustomPage()->popsAndBangsCutDurationAuto) {
			m_pnbCutDurationMs = pnbRandomCutDurationMs();
		} else {
			m_pnbCutDurationMs = clampF(PNB_CUT_MIN_MS, getCustomPage()->popsAndBangsCutDurationMs, PNB_CUT_MAX_MS);
		}
		engineSmPnbSparkCut = true;
		return pnbCutRatio();
	}

	engineSmPnbSparkCut = false;
	return 0.0f;
}

bool EngineStateMachine::isPopsAndBangsBlocked() const {
	// Sport Mode gate: when required, pops and bangs cannot activate unless Sport Mode is on.
	bool sportModeBlocked = getCustomPage()->popsAndBangsRequireSportMode && !engineSmIsSportMode;

	bool cutoutBlocked = false;
	if (getCustomPage()->popsAndBangsCutoutInhibitMode == pops_and_bangs_cutout_inhibit_e::Inhibit) {
		// Gate (not a trigger): blocked unless the cutout is actually open.
		cutoutBlocked = !engine->module<ExhaustCutoutController>()->isCutoutOpen;
	}

	return sportModeBlocked || cutoutBlocked;
}

EngineStateMachineState EngineStateMachine::determineState(float rpm, float tps, float vss, bool vssValid) {
	// Priority 1: engine not spinning
	if (engine->rpmCalculator.isStopped()) {
		return EngineStateMachineState::Off;
	}

	// Priority 2: engine below self-sustaining RPM
	if (engine->rpmCalculator.isCranking()) {
		return EngineStateMachineState::Cranking;
	}

	// Priority 3: wide open throttle
	if (tps > engineConfiguration->smWotTpsThreshold) {
		return EngineStateMachineState::WOT;
	}

	// Priority 4: AE-driven transient — active while AE threshold is met, then held
	//             for smTransientHoldTimeMs after it drops.
	{
		bool aeActive = engine->module<TpsAccelEnrichment>()->isAboveAccelThreshold ||
		                engine->module<TpsAccelEnrichment>()->isBelowDecelThreshold;
		if (aeActive) {
			m_transientHoldTimer.reset();
			return EngineStateMachineState::Transient;
		}
		if (!m_transientHoldTimer.hasElapsedMs(static_cast<float>(engineConfiguration->smTransientHoldTimeMs))) {
			return EngineStateMachineState::Transient;
		}
	}

#if EFI_IDLE_CONTROL
	auto idlePhase = engine->module<IdleController>()->getCurrentPhase();
#else
	auto idlePhase = IIdleController::Phase::Running;
#endif

	// Sustained acceleration/deceleration — based on the previous tick's RPM rate of change
	// (m_lastRpmRate, updated by updateShiftAccumulator later this callback; one slow-callback
	// lag is negligible at ~100 ms). Thresholds of 0 disable the respective state.
	// Skipped while the IdleController considers us in closed-loop idle territory: RPM rate
	// noise from idle hunting or AC/load compensation must not be misread as a driver-initiated
	// tip-in/tip-out. Also skipped for smTransientHoldTimeMs after an eco engage/
	// disengage edge, for the same reason (see onSlowCallback()).
	if (!m_ecoSettleHoldTimer.hasElapsedMs(static_cast<float>(engineConfiguration->smTransientHoldTimeMs))) {
		// still within the post-eco-edge settle hold-off
	} else if (idlePhase != IIdleController::Phase::Idling) {
		int16_t accelThr = engineConfiguration->smAccelRateThreshold;
		int16_t decelThr = engineConfiguration->smDecelRateThreshold;

		if (accelThr > 0 && m_lastRpmRate > static_cast<float>(accelThr)) {
			m_accelHoldTimer.reset();
			return EngineStateMachineState::Accelerating;
		}

		// Hold Accelerating for smAccelHoldMs after the rate last crossed threshold, so a brief
		// dip below threshold doesn't cause chatter back to Cruising/Transient. A lift into
		// Coasting breaks the hold immediately -- a closed throttle physically contradicts a
		// stale "still accelerating" claim, and Overrun/DFCO must never be masked by it.
		uint16_t accelHoldMs = engineConfiguration->smAccelHoldMs;
		if (accelHoldMs > 0 && idlePhase != IIdleController::Phase::Coasting &&
				!m_accelHoldTimer.hasElapsedMs(static_cast<float>(accelHoldMs))) {
			return EngineStateMachineState::Accelerating;
		}

		// Decelerating: RPM dropping faster than threshold.
		// With VSS: only when transmission is disengaged (DetectedGear == 0).
		// Without VSS: rate alone is sufficient — gear detector cannot work without VSS.
		// Decelerating requires VSS to confirm the clutch is out (gear == 0).
		// Without VSS we cannot distinguish clutch-out decel from engine braking in gear,
		// so we fall through to let the idle-controller path classify it as Coasting/Overrun.
		if (decelThr > 0 && m_lastRpmRate < -static_cast<float>(decelThr)) {
			auto gearResult = Sensor::get(SensorType::DetectedGear);
			if (vssValid && gearResult.Valid && gearResult.Value == 0) {
				return EngineStateMachineState::Decelerating;
			}
		}
	}

	// Priorities 5–8: defer to IdleController — it is the single source of truth for the idle corner.
	// CrankToIdleTaper → Afterstart (taper table drives duration, CLT-indexed, same as idle control).
	// Idling → Idle; Coasting (elevated RPM) → Coasting or Overrun; Running → Coasting (still
	// rolling, off-throttle) or Cruising.
	{
		if (idlePhase == IIdleController::Phase::CrankToIdleTaper) {
			return EngineStateMachineState::Afterstart;
		}

		if (idlePhase == IIdleController::Phase::Idling) {
			return EngineStateMachineState::Idle;
		}

		if (idlePhase == IIdleController::Phase::Coasting) {
			bool tpsClosed = tps < engineConfiguration->coastingFuelCutTps;
			// Overrun means the transmission is actively spinning the engine (torque flowing
			// from the wheels) -- that requires the transmission engaged, per isTransmissionEngaged().
			if (tpsClosed && rpm > engineConfiguration->coastingFuelCutRpmHigh && isTransmissionEngaged()) {
				return EngineStateMachineState::Overrun;
			}
			return EngineStateMachineState::Coasting;
		}
	}

	// idlePhase is Running here. If the throttle is closed but the vehicle is still rolling
	// above maxIdleVss, the engine is unloaded (idling, producing no ground torque) even though
	// the car is moving -- that's Coasting, not Cruising, even though IdleController itself
	// bows out of closed-loop idle control at this speed.
	{
		uint8_t maxIdleVss = engineConfiguration->maxIdleVss;
		if (maxIdleVss != 0 && vss > static_cast<float>(maxIdleVss) &&
				tps < engineConfiguration->idlePidDeactivationTpsThreshold) {
			return EngineStateMachineState::Coasting;
		}
	}

	// Default: part-throttle cruising (IdleController phase is Running)
	return EngineStateMachineState::Cruising;
}

// Steady-state clutch read: is the transmission currently transmitting road load to the engine?
// Unlike the shift-detection clutch handling elsewhere in this file, this is a level read, not
// edge-triggered, and it is intentionally decoupled from VSS/DetectedGear -- a clutch switch is a
// direct mechanical read and should win over a speed-derived guess. With no switch configured we
// have no way to know the clutch is out, so we assume the transmission is always engaged (i.e.
// Decelerating aside, engine-braking classification can only ever resolve to Overrun, never be
// suppressed by an undetectable disengaged clutch).
bool EngineStateMachine::isTransmissionEngaged() const {
	sm_clutch_switch_e upSw = engineConfiguration->smUpshiftClutchSwitch;
	sm_clutch_switch_e dnSw = engineConfiguration->smDownshiftClutchSwitch;
	bool secondSwitch = engineConfiguration->smSecondClutchSwitchAvailable
		&& (upSw != sm_clutch_switch_e::None || dnSw != sm_clutch_switch_e::None);
	// Physical inputs work independently of shift-direction selection. Selections
	// also describe virtual (Lua) switches that have no hardware pin assigned.
	bool hasClutchDown = isBrainPinValid(engineConfiguration->clutchDownPin) || secondSwitch
		|| upSw == sm_clutch_switch_e::ClutchDown || dnSw == sm_clutch_switch_e::ClutchDown;
	bool hasClutchUp = isBrainPinValid(engineConfiguration->clutchUpPin) || secondSwitch
		|| upSw == sm_clutch_switch_e::ClutchUp || dnSw == sm_clutch_switch_e::ClutchUp;

	// Clutch-Down reads true when the pedal is pressed (clutch disengaged).
	if (hasClutchDown && engine->engineState.clutchDownState) {
		return false;
	}
	// Clutch-Up reads true when the pedal is at rest (clutch engaged).
	if (hasClutchUp && engine->engineState.clutchUpState == 0) {
		return false;
	}

	return true;
}

float EngineStateMachine::recordRpmSampleAndComputeRate(float rpm, efitimems_t nowMs) {
	if (!m_rpmRateHasAnchor) {
		m_rpmRateAnchorMs  = nowMs;
		m_rpmRateAnchorRpm = rpm;
		m_rpmRateHasAnchor = true;
		return 0; // just started, no reference sample yet
	}

	// Floor the window to one slow-callback tick: a window shorter than the sampling cadence
	// carries no additional information, and this also makes a fresh/reset smRpmRateWindowMs
	// == 0 (e.g. from an older tune with no saved value) behave identically to the
	// pre-existing raw-tick calculation.
	uint16_t windowMs = engineConfiguration->smRpmRateWindowMs;
	if (windowMs < SLOW_CALLBACK_PERIOD_MS) {
		windowMs = SLOW_CALLBACK_PERIOD_MS;
	}

	efitimems_t elapsedMs = nowMs - m_rpmRateAnchorMs;
	if (elapsedMs < static_cast<efitimems_t>(windowMs)) {
		// Window hasn't elapsed yet -- hold the last computed rate rather than recomputing
		// against a partial window, so the value only actually changes at the cadence the user
		// configured.
		return m_lastRpmRate;
	}

	float rate = (rpm - m_rpmRateAnchorRpm) / (elapsedMs / 1000.0f);
	m_rpmRateAnchorMs  = nowMs;
	m_rpmRateAnchorRpm = rpm;
	return rate;
}

void EngineStateMachine::updateShiftAccumulator(float rpm, float vss, bool vssValid, efitimems_t nowMs) {
	efitimems_t dtMs = m_accumulatorHasSample ? nowMs - m_accumulatorLastMs : 0;
	m_accumulatorHasSample = true;
	float prevVss = m_accumulatorLastVss;
	m_accumulatorLastMs  = nowMs;
	m_accumulatorLastRpm = rpm;
	m_accumulatorLastVss = vss;

	// Always compute RPM rate for state detection (used by determineState next tick), over the
	// user-configurable window (smRpmRateWindowMs, "RPM/t") rather than a raw single tick.
	m_lastRpmRate = recordRpmSampleAndComputeRate(rpm, nowMs);
	engineSmRpmRate = m_lastRpmRate; // live-data visibility

	// Missing VSS is not a sudden stop. The first recovered sample starts a new interval.
	bool missingVss = engineConfiguration->smShiftDetectionMode == sm_shift_detection_mode_e::VssRate && !vssValid;
	bool noShiftSwitches = engineConfiguration->smUpshiftClutchSwitch == sm_clutch_switch_e::None
		&& engineConfiguration->smDownshiftClutchSwitch == sm_clutch_switch_e::None;
	if (missingVss || noShiftSwitches) {
		m_shiftAccumulator = 0;
		engineSmShiftAccumulator = 0;
		m_accumulatorHasSample = false;
		return;
	}

	// Accumulator is frozen while the shift window is open (clutch out).
	// The first sample establishes the anchor; same-time callbacks add no evidence.
	if (m_shiftWindowOpen || dtMs == 0) {
		engineSmShiftAccumulator = m_shiftAccumulator;
		return;
	}

	// Dwell tracking for the smAccumulatorSnapTimeMs override below: reset the instant we leave
	// the respective state(s) so the timer measures time since the most recent entry.
	if (m_currentState != EngineStateMachineState::Accelerating) {
		m_accelStateTimer.reset();
	}
	if (m_currentState != EngineStateMachineState::Decelerating &&
			m_currentState != EngineStateMachineState::Overrun) {
		m_decelStateTimer.reset();
	}

	float dt = dtMs / 1000.0f;
	float gain      = engineConfiguration->smAccumulatorGain;
	float decayRate = engineConfiguration->smAccumulatorDecayRate;

	auto decay = [&]() {
		float d = decayRate * dt;
		if (m_shiftAccumulator > 0) {
			m_shiftAccumulator -= d;
			if (m_shiftAccumulator < 0) { m_shiftAccumulator = 0; }
		} else if (m_shiftAccumulator < 0) {
			m_shiftAccumulator += d;
			if (m_shiftAccumulator > 0) { m_shiftAccumulator = 0; }
		}
	};

	if (m_currentState == EngineStateMachineState::Idle ||
	    engineSmIsLaunchControl || engineSmIsTorqueReduction) {
		// Idle oscillations and launch-RPM hold are not shift evidence — decay only.
		decay();
		engineSmShiftAccumulator = m_shiftAccumulator;
		return;
	}

	// Time-based override: a same-gear direction reversal (e.g. a long Overrun run immediately
	// followed by acceleration) would otherwise need an equally long run the other way just to
	// null out a large stale opposite-sign accumulator, risking a stale shift-direction call
	// (e.g. a spurious downshift blip on clutch-in shortly after tipping back into the
	// throttle). Once we've dwelled continuously in the state long enough, force the accumulator
	// to 0 so it starts accumulating fresh evidence for the new direction below instead of
	// waiting it out. smAccumulatorSnapTimeMs == 0 disables this (legacy decay/cross behavior).
	uint16_t snapTimeMs = engineConfiguration->smAccumulatorSnapTimeMs;
	if (snapTimeMs > 0) {
		float snapTimeMsF = static_cast<float>(snapTimeMs);
		if (m_currentState == EngineStateMachineState::Accelerating && m_shiftAccumulator < 0 &&
				m_accelStateTimer.hasElapsedMs(snapTimeMsF)) {
			m_shiftAccumulator = 0;
		} else if ((m_currentState == EngineStateMachineState::Decelerating ||
				m_currentState == EngineStateMachineState::Overrun) && m_shiftAccumulator > 0 &&
				m_decelStateTimer.hasElapsedMs(snapTimeMsF)) {
			m_shiftAccumulator = 0;
		}
	}

	// Accumulator integrates only in Accelerating (positive) and Overrun (negative).
	// The accumulator signal source for shift detection follows smShiftDetectionMode;
	// m_lastRpmRate above is always RPM-based regardless of mode.
	sm_shift_detection_mode_e mode = engineConfiguration->smShiftDetectionMode;
	float signal;
	if (mode == sm_shift_detection_mode_e::VssRate) {
		if (engineConfiguration->smShiftMinVss > 0 && vss < (float)engineConfiguration->smShiftMinVss) {
			decay();
			engineSmShiftAccumulator = m_shiftAccumulator;
			return;
		}
		signal = (vss - prevVss) / dt;
	} else {
		signal = m_lastRpmRate;
	}

	float accelThr = (float)engineConfiguration->smAccelRateThreshold;
	float decelThr = (float)engineConfiguration->smDecelRateThreshold;
	if (accelThr <= 0) { accelThr = 1.0f; }
	if (decelThr <= 0) { decelThr = 1.0f; }

	if (mode == sm_shift_detection_mode_e::VssRate) {
		// VSS-rate signal: integrate directly on threshold crossing — no RPM-based state gate,
		// since state (Accelerating/Overrun) is derived from RPM, not VSS.
		if (signal > 0) {
			m_shiftAccumulator += (signal / accelThr) * gain * dt;
		} else if (signal < 0) {
			m_shiftAccumulator += (signal / decelThr) * gain * dt;
		} else {
			decay();
		}
	} else {
		// RpmRate: upshift integrates only in Accelerating state (rate-gated, immune to idle
		// oscillations and launch hold). Downshift integrates whenever RPM is falling — Idle and
		// LaunchControl are already excluded above, so any other state with negative rate means
		// the driver is decelerating toward a downshift.
		if (m_currentState == EngineStateMachineState::Accelerating && signal > 0) {
			m_shiftAccumulator += (signal / accelThr) * gain * dt;
		} else if (signal < 0) {
			m_shiftAccumulator += (signal / decelThr) * gain * dt;
		} else {
			decay();
		}
	}

	m_shiftAccumulator = clampF(-100.0f, m_shiftAccumulator, 100.0f);
	engineSmShiftAccumulator = m_shiftAccumulator;
}

void EngineStateMachine::updateShiftDetection(float vss, bool vssValid, efitimems_t nowMs) {
	// VSS Rate mode requires a valid sample — disable shift detection
	// entirely in that mode when VSS is unavailable. RPM Rate mode can work without
	// VSS since it only needs engine RPM, so it falls through normally.
	if (!vssValid && engineConfiguration->smShiftDetectionMode == sm_shift_detection_mode_e::VssRate) {
		m_shiftWindowOpen = false;
		m_clutchHasSample = false;
		engineSmIsUpshifting   = false;
		engineSmIsDownshifting = false;
		m_shiftLatched         = false;
		return;
	}

	// Read clutch state maintained by the engine (clutchUpState via SwitchedState,
	// clutchDownState via engine.cpp getClutchDownState)
	bool clutchUpActive   = engine->engineState.clutchUpState  != 0;
	bool clutchDownActive = engine->engineState.clutchDownState;

	sm_clutch_switch_e upSw = engineConfiguration->smUpshiftClutchSwitch;
	sm_clutch_switch_e dnSw = engineConfiguration->smDownshiftClutchSwitch;
	bool secondSwitch = engineConfiguration->smSecondClutchSwitchAvailable;

	// No direction configured → disable shift detection
	if (upSw == sm_clutch_switch_e::None && dnSw == sm_clutch_switch_e::None) {
		m_shiftWindowOpen = false;
		m_clutchHasSample = false;
		engineSmIsUpshifting   = false;
		engineSmIsDownshifting = false;
		m_shiftLatched         = false;
		return;
	}

	// A reset or a gap in detection must not turn a held pedal into a new press.
	// Establish the baseline from the first usable sample, after switch inputs update.
	if (!m_clutchHasSample) {
		m_prevClutchUp = clutchUpActive;
		m_prevClutchDown = clutchDownActive;
		m_clutchHasSample = true;
	}

	// Raw physical edges, independent of which direction (if any) a switch is assigned to, so a
	// switch can be consulted even when it isn't the one a given direction is configured with.
	// Clutch-Up reads true with the pedal at rest, so its FALLING edge is the pedal leaving rest
	// (clutch starting to disengage, drivetrain still locked) and its RISING edge is the pedal
	// fully back at rest (clutch re-engaged). Clutch-Down is the inverse: RISING edge is full
	// press (clutch fully disengaged), FALLING edge is release starting.
	bool rawUpFell = !clutchUpActive   && m_prevClutchUp;
	bool rawUpRose =  clutchUpActive   && !m_prevClutchUp;
	bool rawDnFell = !clutchDownActive && m_prevClutchDown;
	bool rawDnRose =  clutchDownActive && !m_prevClutchDown;

	m_prevClutchUp   = clutchUpActive;
	m_prevClutchDown = clutchDownActive;

	// A switch type is usable for open/close timing if some direction is configured to use it,
	// or the operator has told us a second switch is wired alongside whichever one is selected.
	bool upWired = (upSw == sm_clutch_switch_e::ClutchUp)   || (dnSw == sm_clutch_switch_e::ClutchUp)   || secondSwitch;
	bool dnWired = (upSw == sm_clutch_switch_e::ClutchDown) || (dnSw == sm_clutch_switch_e::ClutchDown) || secondSwitch;

	auto openFired  = [&](sm_clutch_switch_e sw) {
		if (sw == sm_clutch_switch_e::ClutchUp)   { return rawUpFell; }
		if (sw == sm_clutch_switch_e::ClutchDown) { return rawDnRose; }
		return false;
	};
	auto closeFired = [&](sm_clutch_switch_e sw) {
		if (sw == sm_clutch_switch_e::ClutchUp)   { return rawUpRose; }
		if (sw == sm_clutch_switch_e::ClutchDown) { return rawDnFell; }
		return false;
	};

	// Open the shift detection window on the earliest available "clutch starting to disengage"
	// edge among the switch(es) actually wired.
	bool upTypeOpens = upWired && rawUpFell;
	bool dnTypeOpens = dnWired && rawDnRose;

	if (!m_shiftWindowOpen && (upTypeOpens || dnTypeOpens)) {
		m_shiftWindowOpen   = true;
		m_shiftWindowOpenMs = nowMs;

		if (upSw != sm_clutch_switch_e::None && dnSw != sm_clutch_switch_e::None) {
			if (upSw == dnSw) {
				// Both directions share the same physical switch. Read direction from the
				// pre-shift accumulator: positive = upshift context, negative = downshift context.
				// evaluateShiftDirection() will confirm or deny against the thresholds.
				m_shiftIsUpshift = (m_shiftAccumulator >= 0);
			} else {
				// Different switch types assigned per direction: whichever type's open edge
				// actually fired identifies the direction.
				m_shiftIsUpshift = openFired(upSw);
			}
		} else {
			// Only one direction configured at all.
			m_shiftIsUpshift = (upSw != sm_clutch_switch_e::None);
		}
	}

	// Relatch-on-clutch-down: while the latch is active, a fresh Clutch-Down full-press resets
	// it back to the full duration instead of letting it expire, independent of window state.
	// Requires a second switch to be wired (rawDnRose is meaningless without one).
	if (m_shiftLatched && secondSwitch && engineConfiguration->smRelatchOnClutchDown && rawDnRose) {
		m_shiftLatchUntilMs = nowMs + engineConfiguration->smShiftLatchTimeMs;
	}

	bool windowConfirmed = false;

	if (m_shiftWindowOpen) {
		// Close the window when the clutch re-engages, or on timeout. With a second switch wired,
		// Clutch-Down's falling edge (release starting) is an earlier, equally valid "re-engaging"
		// signal than waiting for Clutch-Up to fully return to rest.
		sm_clutch_switch_e dirSwitch = m_shiftIsUpshift ? upSw : dnSw;
		bool ownClose   = closeFired(dirSwitch);
		bool earlyClose = secondSwitch && dirSwitch == sm_clutch_switch_e::ClutchUp && rawDnFell;
		bool triggerFell    = ownClose || earlyClose;
		efitimems_t elapsed = nowMs - m_shiftWindowOpenMs;

		if (triggerFell || elapsed > SM_SHIFT_TIMEOUT_MS) {
			// Window closes; reset the accumulator for the next shift.
			// Any active latch keeps running independently (see below).
			m_shiftWindowOpen  = false;
			m_shiftAccumulator = 0;
			engineSmShiftAccumulator = 0;
			m_accumulatorLastMs = nowMs;
		} else {
			// Window active: evaluate direction from the (now frozen) accumulator.
			windowConfirmed = evaluateShiftDirection(m_shiftIsUpshift, vss);
		}
	}

	// Latch: arm fresh on the first confirmation, then hold for smShiftLatchTimeMs without
	// being extended by further confirmations (a true "minimum hold", not an extending one) --
	// the only thing allowed to push the deadline back out is the relatch-on-clutch-down logic
	// above. This masks rate-check flicker without making the overlay outlive a long latch
	// configured shorter than the actual shift.
	if (windowConfirmed) {
		if (!m_shiftLatched) {
			m_shiftLatchUntilMs = nowMs + engineConfiguration->smShiftLatchTimeMs;
		}
		m_shiftLatched          = true;
		m_shiftLatchedIsUpshift = m_shiftIsUpshift;
	} else if (m_shiftLatched && nowMs >= m_shiftLatchUntilMs) {
		m_shiftLatched = false;
	}

	bool active        = windowConfirmed || m_shiftLatched;
	bool activeIsUpshift = windowConfirmed ? m_shiftIsUpshift : m_shiftLatchedIsUpshift;
	engineSmIsUpshifting   = active &&  activeIsUpshift;
	engineSmIsDownshifting = active && !activeIsUpshift;

	// Clutch fully up (pedal at rest) unconditionally ends any shift overlay, overriding the
	// latch/relatch hold. The blipper and RPM-hold modules trust these flags with no clutch
	// check of their own, so this is the only place that can guarantee neither is ever active
	// while the pedal is at rest.
	if (clutchUpActive) {
		bool wasShifting = m_shiftWindowOpen || m_shiftLatched;
		engineSmIsUpshifting   = false;
		engineSmIsDownshifting = false;
		m_shiftWindowOpen      = false;
		m_shiftLatched         = false;
		// Only reset the accumulator when we're actually ending a shift, not on every
		// pedal-at-rest tick (which would prevent the accumulator from building up at all).
		if (wasShifting) {
			m_shiftAccumulator       = 0;
			engineSmShiftAccumulator = 0;
			m_accumulatorLastMs      = nowMs;
		}
	}
}

bool EngineStateMachine::evaluateShiftDirection(bool isUpshift, float currentVss) {
	sm_clutch_switch_e upSw = engineConfiguration->smUpshiftClutchSwitch;
	sm_clutch_switch_e dnSw = engineConfiguration->smDownshiftClutchSwitch;

	// Single direction configured — the switch is authoritative, no accumulator confirmation needed.
	if (upSw == sm_clutch_switch_e::None || dnSw == sm_clutch_switch_e::None) {
		return true;
	}

	// VSS mode: belt-and-suspenders speed gate (accumulator also decays below this threshold).
	if (engineConfiguration->smShiftDetectionMode == sm_shift_detection_mode_e::VssRate &&
	    engineConfiguration->smShiftMinVss > 0 &&
	    currentVss < (float)engineConfiguration->smShiftMinVss) {
		return false;
	}

	// Both directions configured: confirm against the pre-shift accumulator.
	float upThr = (float)engineConfiguration->smAccumulatorUpshiftThreshold;
	float dnThr = (float)engineConfiguration->smAccumulatorDownshiftThreshold;

	if (isUpshift) {
		return m_shiftAccumulator >= upThr;
	} else {
		return m_shiftAccumulator <= -dnThr;
	}
}
