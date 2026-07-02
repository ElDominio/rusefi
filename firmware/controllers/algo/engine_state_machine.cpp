#include "pch.h"
#include "custom_page.h"
#include "engine_state_machine.h"
#include "electronic_throttle.h"
#include "malfunction_central.h"
#include "dfco.h"
#include "exhaust_cutout.h"
#include "tinymt32.h" // basic 'random' for the P&B automatic cut duration

#if EFI_ENGINE_STATE_MACHINE

// Lower/upper bounds (ms) for the P&B spark-cut window, also used by the 'automatic' random mode.
static constexpr float PNB_CUT_MIN_MS = 20.0f;
static constexpr float PNB_CUT_MAX_MS = 500.0f;

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
	engineSmEnabled = isEnabled();

	if (!isEnabled()) {
		return;
	}

	efitimems_t nowMs = getTimeNowMs();
	float rpm = engine->rpmCalculator.getCachedRpm();

	auto tpsSensor = Sensor::get(SensorType::DriverThrottleIntent);
	float tps = tpsSensor ? tpsSensor.Value : 0.0f;

	float vss = Sensor::getOrZero(SensorType::VehicleSpeed);

	// Primary state determination (engine lifecycle, throttle position)
	EngineStateMachineState newState = determineState(rpm, tps);
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

	if (flatShiftActive) {
		engineSmIsUpshifting   = true;
		engineSmIsDownshifting = false;
		// Abort any open clutch-detection window/latch so it doesn't fight the flat-shift signal
		m_shiftWindowOpen = false;
		m_shiftLatched    = false;
	} else {
		updateShiftDetection(tps, rpm, vss, nowMs);
	}

	// Overlay: Limp Mode — explicit latched protective state.
	//
	// This used to be derived from ANY LimpManager fuel/spark cut, which incorrectly
	// flagged limp for the normal rev limit, boost cut, launch cut, etc. Limp mode is now
	// a dedicated latch (m_limpModeLatched) raised only via reportLimpCondition(). Triggers:
	//   - ETB jam (LimpManager::reportEtbJammed), and
	//   - cumulative DTC severity (below): when active fault codes add up to at least the
	//     user-configurable limpSeverityThreshold. Severity weights are fixed per fault class
	//     in obdCodeSeverity(); a latched misfire (5 pts) trips limp at the default threshold,
	//     while light informational codes (0 pts) never do.
	// The ordinary rev limit and other transient cuts intentionally do NOT enter limp mode.
	uint16_t limpThreshold = getCustomPage()->limpSeverityThreshold;
	if (limpThreshold > 0 && getErrorSeverityTotal() >= limpThreshold) {
		reportLimpCondition();
	}

	engineSmIsLimp = m_limpModeLatched;

	// Eco mode overlay — must run before the display override below so the dash reflects it
	// this cycle. Depends on m_currentState (set above) and engineSmIsLimp (limp wins).
	updateEcoMode(m_currentState);

	// Ghost Cam overlay — idle-only manual lope effect via switch or Lua gauge.
	// Must run after engineSmIsIdle and engineSmIsLimp are set.
	updateGhostCam();

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
	} else if (engineSmIsGhostCam) {
		engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::GhostCam);
	}

	updatePopsAndBangs(engineSmIsOverrun);
}

void EngineStateMachine::updateEcoMode(EngineStateMachineState currentState) {
	// Limp mode is protective and out-votes eco; a disabled feature is always inactive.
	if (!getCustomPage()->ecoModeEnabled || engineSmIsLimp) {
		m_ecoCruiseTimer.reset();
		engineSmIsEcoMode = false;
		return;
	}

	// Instant drop: only the Cruising state accumulates time. Any other state pins the timer at
	// zero, so re-entry must accumulate the full ecoModeCruisingTime again before eco re-engages.
	bool cruiseElapsed = false;
	if (currentState == EngineStateMachineState::Cruising) {
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

	// Inhibit blocks the timer-based engage; there is no Force-On override of the timer/MAP gate.
	engineSmIsEcoMode = cruiseElapsed && !isEcoModeInhibited();
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
	engineSmIsCold      = (temp < getCustomPage()->smColdTempThreshold);
	engineSmIsHot       = (temp > getCustomPage()->smHotTempThreshold);
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

	// Activation: either pin or Lua gauge, selected by ghostCamActivationSource (0=Pin, 1=Gauge).
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
		bool gaugeAsserted = false;
		SensorType gaugeType = SensorType::LuaGauge1;
		switch (getCustomPage()->ghostCamLuaGauge) {
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
			const float threshold = getCustomPage()->ghostCamLuaGaugeThreshold;
			switch (getCustomPage()->ghostCamLuaGaugeMeaning) {
				case LUA_GAUGE_LOWER_BOUND: gaugeAsserted = (value >= threshold); break;
				case LUA_GAUGE_UPPER_BOUND: gaugeAsserted = (value <= threshold); break;
			}
		}
		engineSmIsGhostCam = gaugeAsserted;
	}
#else // !EFI_GHOST_CAM
	engineSmIsGhostCam = false;
#endif // EFI_GHOST_CAM
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
	auto mode = getCustomPage()->popsAndBangsDisableMode;

	bool pinBlocked = false;
	bool gaugeBlocked = false;

#if !EFI_UNIT_TEST
	if (mode == POPS_AND_BANGS_DISABLE_MODE_SWITCH_INPUT ||
	    mode == POPS_AND_BANGS_DISABLE_MODE_SWITCH_OR_LUA_GAUGE) {
		const switch_input_pin_e pin = getCustomPage()->popsAndBangsDisablePin;
		const pin_input_mode_e pinMode = getCustomPage()->popsAndBangsDisablePinMode;
		if (isBrainPinValid(pin)) {
			pinBlocked = efiReadPin(pin, pinMode);
		}
	}
#endif

	if (mode == POPS_AND_BANGS_DISABLE_MODE_LUA_GAUGE ||
	    mode == POPS_AND_BANGS_DISABLE_MODE_SWITCH_OR_LUA_GAUGE) {
		SensorType gaugeType = SensorType::LuaGauge1;
		switch (getCustomPage()->popsAndBangsLuaGauge) {
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
			const float threshold = getCustomPage()->popsAndBangsLuaGaugeValue;
			switch (getCustomPage()->popsAndBangsLuaGaugeMeaning) {
				case LUA_GAUGE_LOWER_BOUND:
					gaugeBlocked = (value >= threshold);
					break;
				case LUA_GAUGE_UPPER_BOUND:
					gaugeBlocked = (value <= threshold);
					break;
			}
		}
	}

	bool cutoutBlocked = false;
	if (getCustomPage()->popsAndBangsCutoutInhibitMode == pops_and_bangs_cutout_inhibit_e::Inhibit) {
		// Gate (not a trigger): blocked unless the cutout is actually open.
		cutoutBlocked = !engine->module<ExhaustCutoutController>()->isCutoutOpen;
	}

	return pinBlocked || gaugeBlocked || cutoutBlocked;
}

EngineStateMachineState EngineStateMachine::determineState(float rpm, float tps) {
	// Priority 1: engine not spinning
	if (engine->rpmCalculator.isStopped()) {
		return EngineStateMachineState::Off;
	}

	// Priority 2: engine below self-sustaining RPM
	if (engine->rpmCalculator.isCranking()) {
		return EngineStateMachineState::Cranking;
	}

	// Priority 3: wide open throttle
	if (tps > getCustomPage()->smWotTpsThreshold) {
		return EngineStateMachineState::WOT;
	}

	// Priority 4: AE-driven transient — active while AE threshold is met, then held
	//             for smTransientHoldoffCallbacks slow-callback periods after it drops.
	{
		bool aeActive = engine->module<TpsAccelEnrichment>()->isAboveAccelThreshold ||
		                engine->module<TpsAccelEnrichment>()->isBelowDecelThreshold;
		if (aeActive) {
			m_transientHoldoffRemaining = getCustomPage()->smTransientHoldoffCallbacks;
			return EngineStateMachineState::Transient;
		}
		if (m_transientHoldoffRemaining > 0) {
			m_transientHoldoffRemaining--;
			return EngineStateMachineState::Transient;
		}
	}

	auto idlePhase = engine->module<IdleController>()->getCurrentPhase();

	// Sustained acceleration/deceleration — based on the previous tick's RPM rate of change
	// (m_lastRpmRate, updated by updateShiftAccumulator later this callback; one slow-callback
	// lag is negligible at ~100 ms). Thresholds of 0 disable the respective state.
	// Skipped while the IdleController considers us in closed-loop idle territory: RPM rate
	// noise from idle hunting or AC/load compensation must not be misread as a driver-initiated
	// tip-in/tip-out.
	if (idlePhase != IIdleController::Phase::Idling) {
		int16_t accelThr = getCustomPage()->smAccelRateThreshold;
		int16_t decelThr = getCustomPage()->smDecelRateThreshold;

		if (accelThr > 0 && m_lastRpmRate > static_cast<float>(accelThr)) {
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
			if (Sensor::hasSensor(SensorType::VehicleSpeed) && gearResult.Valid && gearResult.Value == 0) {
				return EngineStateMachineState::Decelerating;
			}
		}
	}

	// Priorities 5–8: defer to IdleController — it is the single source of truth for the idle corner.
	// CrankToIdleTaper → Afterstart (taper table drives duration, CLT-indexed, same as idle control).
	// Idling → Idle; Coasting → Coasting or Overrun; Running → fall through to Cruising.
	{
		if (idlePhase == IIdleController::Phase::CrankToIdleTaper) {
			return EngineStateMachineState::Afterstart;
		}

		if (idlePhase == IIdleController::Phase::Idling) {
			return EngineStateMachineState::Idle;
		}

		if (idlePhase == IIdleController::Phase::Coasting) {
			bool tpsClosed = tps < engineConfiguration->coastingFuelCutTps;
			if (tpsClosed && rpm > engineConfiguration->coastingFuelCutRpmHigh) {
				// Overrun requires the transmission to be engaged. With VSS, the gear detector
				// reflects clutch state (DetectedGear == 0 when clutch is out). Without VSS,
				// we cannot determine clutch state so we fall back to the RPM/TPS check alone.
				auto gearResult = Sensor::get(SensorType::DetectedGear);
				bool vssPresent   = Sensor::hasSensor(SensorType::VehicleSpeed);
				bool transmissionEngaged = !vssPresent || (gearResult.Valid && gearResult.Value > 0);
				if (transmissionEngaged) {
					return EngineStateMachineState::Overrun;
				}
			}
			return EngineStateMachineState::Coasting;
		}
	}

	// Default: part-throttle cruising (IdleController phase is Running)
	return EngineStateMachineState::Cruising;
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
	uint16_t windowMs = getCustomPage()->smRpmRateWindowMs;
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

void EngineStateMachine::updateShiftAccumulator(float rpm, float vss, efitimems_t nowMs) {
	efitimems_t dtMs = nowMs - m_accumulatorLastMs;
	float prevVss = m_accumulatorLastVss;
	m_accumulatorLastMs  = nowMs;
	m_accumulatorLastRpm = rpm;
	m_accumulatorLastVss = vss;

	// Always compute RPM rate for state detection (used by determineState next tick), over the
	// user-configurable window (smRpmRateWindowMs, "RPM/t") rather than a raw single tick.
	m_lastRpmRate = recordRpmSampleAndComputeRate(rpm, nowMs);
	engineSmRpmRate = m_lastRpmRate; // live-data visibility

	// Accumulator is frozen while the shift window is open (clutch out).
	// dtMs == 0 guard covers the first-ever call where we have no previous data.
	if (m_shiftWindowOpen || dtMs == 0) {
		engineSmShiftAccumulator = m_shiftAccumulator;
		return;
	}

	float dt = dtMs / 1000.0f;
	float gain      = getCustomPage()->smAccumulatorGain;
	float decayRate = getCustomPage()->smAccumulatorDecayRate;

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
	    m_currentState == EngineStateMachineState::LaunchControl) {
		// Idle oscillations and launch-RPM hold are not shift evidence — decay only.
		decay();
		engineSmShiftAccumulator = m_shiftAccumulator;
		return;
	}

	// Accumulator integrates only in Accelerating (positive) and Overrun (negative).
	// The accumulator signal source for shift detection follows smShiftDetectionMode;
	// m_lastRpmRate above is always RPM-based regardless of mode.
	sm_shift_detection_mode_e mode = getCustomPage()->smShiftDetectionMode;
	float signal;
	if (mode == sm_shift_detection_mode_e::VssRate) {
		if (getCustomPage()->smShiftMinVss > 0 && vss < (float)getCustomPage()->smShiftMinVss) {
			decay();
			engineSmShiftAccumulator = m_shiftAccumulator;
			return;
		}
		signal = (vss - prevVss) / dt;
	} else {
		signal = m_lastRpmRate;
	}

	float accelThr = (float)getCustomPage()->smAccelRateThreshold;
	float decelThr = (float)getCustomPage()->smDecelRateThreshold;
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

void EngineStateMachine::updateShiftDetection(float /*tps*/, float rpm, float vss, efitimems_t nowMs) {
	// VSS Rate mode requires VSS to have a meaningful signal — disable shift detection
	// entirely in that mode when no VSS sensor is present. RPM Rate mode can work without
	// VSS since it only needs engine RPM, so it falls through normally.
	bool vssPresent = Sensor::hasSensor(SensorType::VehicleSpeed);
	if (!vssPresent && getCustomPage()->smShiftDetectionMode == sm_shift_detection_mode_e::VssRate) {
		updateShiftAccumulator(rpm, vss, nowMs);
		engineSmIsUpshifting   = false;
		engineSmIsDownshifting = false;
		m_shiftLatched         = false;
		return;
	}

	// Read clutch state maintained by the engine (clutchUpState via SwitchedState,
	// clutchDownState via engine.cpp getClutchDownState)
	bool clutchUpActive   = engine->engineState.clutchUpState  != 0;
	bool clutchDownActive = engine->engineState.clutchDownState;

	sm_clutch_switch_e upSw = getCustomPage()->smUpshiftClutchSwitch;
	sm_clutch_switch_e dnSw = getCustomPage()->smDownshiftClutchSwitch;
	bool secondSwitch = getCustomPage()->smSecondClutchSwitchAvailable;

	// No direction configured → disable shift detection
	if (upSw == sm_clutch_switch_e::None && dnSw == sm_clutch_switch_e::None) {
		engineSmIsUpshifting   = false;
		engineSmIsDownshifting = false;
		m_shiftLatched         = false;
		return;
	}

	// Update the direction accumulator. This runs every tick; the accumulator itself
	// only advances while the window is closed (clutch engaged).
	updateShiftAccumulator(rpm, vss, nowMs);

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
	if (m_shiftLatched && secondSwitch && getCustomPage()->smRelatchOnClutchDown && rawDnRose) {
		m_shiftLatchUntilMs = nowMs + getCustomPage()->smShiftLatchTimeMs;
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
			m_shiftLatchUntilMs = nowMs + getCustomPage()->smShiftLatchTimeMs;
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
	sm_clutch_switch_e upSw = getCustomPage()->smUpshiftClutchSwitch;
	sm_clutch_switch_e dnSw = getCustomPage()->smDownshiftClutchSwitch;

	// Single direction configured — the switch is authoritative, no accumulator confirmation needed.
	if (upSw == sm_clutch_switch_e::None || dnSw == sm_clutch_switch_e::None) {
		return true;
	}

	// VSS mode: belt-and-suspenders speed gate (accumulator also decays below this threshold).
	if (getCustomPage()->smShiftDetectionMode == sm_shift_detection_mode_e::VssRate &&
	    getCustomPage()->smShiftMinVss > 0 &&
	    currentVss < (float)getCustomPage()->smShiftMinVss) {
		return false;
	}

	// Both directions configured: confirm against the pre-shift accumulator.
	float upThr = (float)getCustomPage()->smAccumulatorUpshiftThreshold;
	float dnThr = (float)getCustomPage()->smAccumulatorDownshiftThreshold;

	if (isUpshift) {
		return m_shiftAccumulator >= upThr;
	} else {
		return m_shiftAccumulator <= -dnThr;
	}
}

#else // !EFI_ENGINE_STATE_MACHINE

// Engine State Machine compiled out (board set -DEFI_ENGINE_STATE_MACHINE=FALSE).
// The module stays in the tuple so consumers, live-data and Lua lookups still link;
// it simply does nothing and all engineSm* flags stay at their default (false).
bool EngineStateMachine::isEnabled() const { return false; }
EngineStateMachineState EngineStateMachine::getCurrentState() const { return EngineStateMachineState::Off; }
void EngineStateMachine::reportLimpCondition() { /* state machine compiled out — no limp mode */ }
void EngineStateMachine::onSlowCallback() { }
void EngineStateMachine::updatePopsAndBangs(bool /*isOverrun*/) { engineSmIsPopsAndBangs = false; }
void EngineStateMachine::updateEcoMode(EngineStateMachineState /*currentState*/) { engineSmIsEcoMode = false; }
void EngineStateMachine::updateGhostCam() { engineSmIsGhostCam = false; }
void EngineStateMachine::updateSportPedal() { engineSmIsSportPedal = false; }
void EngineStateMachine::updateTempOverlay() { engineSmIsCold = false; engineSmIsOperating = false; engineSmIsHot = false; }float EngineStateMachine::getPopsAndBangsSparkSkipRatio() { engineSmPnbSparkCut = false; return 0.0f; }

#endif // EFI_ENGINE_STATE_MACHINE
