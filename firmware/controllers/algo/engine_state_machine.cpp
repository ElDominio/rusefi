#include "pch.h"
#include "custom_page.h"
#include "engine_state_machine.h"
#include "malfunction_central.h"
#include "dfco.h"
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

	// Record sensor snapshot into history buffer
	recordHistory(tps, rpm, vss, nowMs);

	// Primary state determination (engine lifecycle, throttle position)
	EngineStateMachineState newState = determineState(rpm, tps);
	m_currentState = newState;

	uint8_t stateNum = static_cast<uint8_t>(newState);
	engineSmCurrentState = stateNum;

	engineSmIsOff        = (newState == EngineStateMachineState::Off);
	engineSmIsCranking   = (newState == EngineStateMachineState::Cranking);
	engineSmIsAfterStart = (newState == EngineStateMachineState::Afterstart);
	engineSmIsIdle       = (newState == EngineStateMachineState::Idle);
	engineSmIsCoasting   = (newState == EngineStateMachineState::Coasting);
	engineSmIsTransient  = (newState == EngineStateMachineState::Transient);
	engineSmIsWot        = (newState == EngineStateMachineState::WOT);
	engineSmIsCruising   = (newState == EngineStateMachineState::Cruising);
	engineSmIsOverrun    = (newState == EngineStateMachineState::Overrun);

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
		// Abort any open clutch-detection window so it doesn't fight the flat-shift signal
		m_shiftWindowOpen = false;
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

	return pinBlocked || gaugeBlocked;
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

	// Priorities 5–8: defer to IdleController — it is the single source of truth for the idle corner.
	// CrankToIdleTaper → Afterstart (taper table drives duration, CLT-indexed, same as idle control).
	// Idling → Idle; Coasting → Coasting or Overrun; Running → fall through to Cruising.
	{
		auto idlePhase = engine->module<IdleController>()->getCurrentPhase();

		if (idlePhase == IIdleController::Phase::CrankToIdleTaper) {
			return EngineStateMachineState::Afterstart;
		}

		if (idlePhase == IIdleController::Phase::Idling) {
			return EngineStateMachineState::Idle;
		}

		if (idlePhase == IIdleController::Phase::Coasting) {
			// VSS is intentionally excluded: overrun is foot-off-throttle at high RPM regardless of speed.
			// DFCO's VSS guard lives in getState() and is a fuel-cut-only constraint.
			bool tpsClosed = tps < engineConfiguration->coastingFuelCutTps;
			if (tpsClosed && rpm > engineConfiguration->coastingFuelCutRpmHigh) {
				return EngineStateMachineState::Overrun;
			}
			return EngineStateMachineState::Coasting;
		}
	}

	// Default: part-throttle cruising (IdleController phase is Running)
	return EngineStateMachineState::Cruising;
}

void EngineStateMachine::recordHistory(float tps, float rpm, float vss, efitimems_t nowMs) {
	m_history[m_historyHead] = { nowMs, tps, rpm, vss };
	m_historyHead = (m_historyHead + 1) % SM_HISTORY_SIZE;
	if (m_historyCount < SM_HISTORY_SIZE) {
		m_historyCount++;
	}
}

// Returns the history entry approximately `lookbackMs` ago, or nullptr if insufficient history.
// Assumes the buffer is sampled at SLOW_CALLBACK_PERIOD_MS intervals.
const EngineStateMachine::SmHistoryEntry* EngineStateMachine::getHistoryAt(efitimems_t lookbackMs) const {
	if (m_historyCount == 0) {
		return nullptr;
	}

	int entriesBack = static_cast<int>(lookbackMs / SLOW_CALLBACK_PERIOD_MS);
	// Clamp: need at least 1 to get a different sample; can't go further than available history
	if (entriesBack < 1) {
		entriesBack = 1;
	}
	if (entriesBack >= m_historyCount) {
		entriesBack = m_historyCount - 1;
	}

	// Most recent entry is at (head-1); target is (head-1-entriesBack)
	int idx = ((m_historyHead - 1 - entriesBack) % SM_HISTORY_SIZE + SM_HISTORY_SIZE) % SM_HISTORY_SIZE;
	return &m_history[idx];
}

void EngineStateMachine::updateShiftDetection(float tps, float rpm, float vss, efitimems_t nowMs) {
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
		return;
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
				// Both directions share the same physical switch. Use current TPS to disambiguate:
				// above idle threshold = driver was on throttle = upshift.
				m_shiftIsUpshift = (tps >= (float)getCustomPage()->smShiftTpsThreshold);
			} else {
				// Different switch types assigned per direction: whichever type's open edge
				// actually fired identifies the direction.
				m_shiftIsUpshift = openFired(upSw);
			}
		} else {
			// Only one direction configured at all.
			m_shiftIsUpshift = (upSw != sm_clutch_switch_e::None);
		}

		// Clutch-Up's falling edge fires as soon as the pedal leaves rest, which may lag the
		// clutch plate actually being mechanically disengaged. Apply the configured delay
		// before evaluating sensor data.
		sm_clutch_switch_e dirSwitch = m_shiftIsUpshift ? upSw : dnSw;
		bool usingClutchUpSwitch = (dirSwitch == sm_clutch_switch_e::ClutchUp);
		efitimems_t delay = usingClutchUpSwitch
			? static_cast<efitimems_t>(getCustomPage()->smClutchUpDisengagementDelayMs) : 0u;
		m_shiftEvaluateAtMs = nowMs + delay;
	}

	if (!m_shiftWindowOpen) {
		engineSmIsUpshifting   = false;
		engineSmIsDownshifting = false;
		return;
	}

	// Close the window when the clutch re-engages, or on timeout. With a second switch wired,
	// Clutch-Down's falling edge (release starting) is an earlier, equally valid "re-engaging"
	// signal than waiting for Clutch-Up to fully return to rest.
	sm_clutch_switch_e dirSwitch = m_shiftIsUpshift ? upSw : dnSw;
	bool ownClose   = closeFired(dirSwitch);
	bool earlyClose = secondSwitch && dirSwitch == sm_clutch_switch_e::ClutchUp && rawDnFell;
	bool triggerFell    = ownClose || earlyClose;
	efitimems_t elapsed = nowMs - m_shiftWindowOpenMs;

	if (triggerFell || elapsed > SM_SHIFT_TIMEOUT_MS) {
		m_shiftWindowOpen      = false;
		engineSmIsUpshifting   = false;
		engineSmIsDownshifting = false;
		return;
	}

	// Still within the disengagement delay — don't commit direction yet
	if (nowMs < m_shiftEvaluateAtMs) {
		return;
	}

	// Window active and past delay: evaluate direction via sensor history
	bool confirmed = evaluateShiftDirection(m_shiftIsUpshift, rpm, vss, nowMs);
	engineSmIsUpshifting   =  m_shiftIsUpshift && confirmed;
	engineSmIsDownshifting = !m_shiftIsUpshift && confirmed;
}

bool EngineStateMachine::evaluateShiftDirection(bool isUpshift, float /*currentRpm*/, float currentVss, efitimems_t /*nowMs*/) {
	uint16_t lookbackMs = getCustomPage()->smShiftLookbackMs;
	if (lookbackMs < 100) {
		lookbackMs = 300; // safe default when not yet configured
	}
	if (lookbackMs > 1000) {
		lookbackMs = 1000;
	}

	sm_shift_detection_mode_e mode = getCustomPage()->smShiftDetectionMode;

	const SmHistoryEntry* hist = getHistoryAt(lookbackMs);
	if (!hist) {
		// Buffer not yet full — direction from switch alone is sufficient
		return true;
	}

	if (mode == sm_shift_detection_mode_e::SimpleThrottle) {
		// Simple Throttle: TPS position at lookback time reveals driver intent.
		// Above idle threshold at shift start → driver was on throttle → upshift.
		bool tpsWasOpen = hist->tps >= getCustomPage()->smShiftTpsThreshold;
		return isUpshift ? tpsWasOpen : !tpsWasOpen;
	}

	if (mode == sm_shift_detection_mode_e::RpmRate) {
		// Use two pre-shift history points, mirroring the TPS lookback approach.
		// hist  = RPM at ~shift time (lookbackMs ago); hist2 = RPM before the shift trend (2× ago).
		// Rising RPM going into the shift → upshift; falling RPM → downshift.
		const SmHistoryEntry* hist2 = getHistoryAt(static_cast<efitimems_t>(lookbackMs) * 2);
		if (!hist2 || hist2 == hist) {
			// Not enough history depth — trust the switch direction
			return true;
		}
		float deltaPer1s = (hist->rpm - hist2->rpm) / (static_cast<float>(lookbackMs) / 1000.0f);
		if (isUpshift) {
			return deltaPer1s > (float)getCustomPage()->smUpshiftRateThreshold;
		} else {
			return deltaPer1s < -(float)getCustomPage()->smDownshiftRateThreshold;
		}
	}

	if (mode == sm_shift_detection_mode_e::VssRate) {
		// VSS Rate: upshift context = accelerating (VSS rising),
		//           downshift context = braking/trail-braking (VSS falling).
		// Requires a VSS sensor — fall back to Simple Throttle with a warning if absent.
		auto vssSensor = Sensor::get(SensorType::VehicleSpeed);
		if (!vssSensor) {
			if (!m_vssRateWarningEmitted) {
				warning(ObdCode::OBD_PCM_Processor_Fault,
				        "Engine SM: VSS Rate mode selected but no VSS sensor configured — falling back to Simple Throttle");
				m_vssRateWarningEmitted = true;
			}
			bool tpsWasOpen = hist->tps >= getCustomPage()->smShiftTpsThreshold;
			return isUpshift ? tpsWasOpen : !tpsWasOpen;
		}
		m_vssRateWarningEmitted = false;

		float deltaPer1s = (currentVss - hist->vss) / (static_cast<float>(lookbackMs) / 1000.0f);
		if (isUpshift) {
			return deltaPer1s > (float)getCustomPage()->smUpshiftRateThreshold;
		} else {
			return deltaPer1s < -(float)getCustomPage()->smDownshiftRateThreshold;
		}
	}

	return true; // unknown mode — assume switch direction is correct
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
void EngineStateMachine::updateTempOverlay() { engineSmIsCold = false; engineSmIsOperating = false; engineSmIsHot = false; }float EngineStateMachine::getPopsAndBangsSparkSkipRatio() { engineSmPnbSparkCut = false; return 0.0f; }

#endif // EFI_ENGINE_STATE_MACHINE
