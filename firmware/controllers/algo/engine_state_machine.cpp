#include "pch.h"
#include "engine_state_machine.h"

bool EngineStateMachine::isEnabled() const {
	return engineConfiguration->useEngineStateMachine;
}

EngineStateMachineState EngineStateMachine::getCurrentState() const {
	return m_currentState;
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
	bool flatShiftActive = engine->shiftTorqueReductionController.isFlatShiftConditionSatisfied;
	engineSmIsTorqueReduction = flatShiftActive;

	if (flatShiftActive) {
		engineSmIsUpshifting   = true;
		engineSmIsDownshifting = false;
		// Abort any open clutch-detection window so it doesn't fight the flat-shift signal
		m_shiftWindowOpen = false;
	} else {
		updateShiftDetection(tps, rpm, vss, nowMs);
	}

	// Overlay: Limp Mode — any active fuel or spark cut from LimpManager
	{
		LimpState injState = getLimpManager()->allowInjection();
		LimpState ignState = getLimpManager()->allowIgnition();
		engineSmIsLimp = !injState || !ignState;
	}

	// Override the display integer for overlay priority: Limp > Upshifting > Downshifting > LaunchControl
	if (engineSmIsLimp) {
		engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::Limp);
	} else if (engineSmIsUpshifting) {
		engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::Upshifting);
	} else if (engineSmIsDownshifting) {
		engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::Downshifting);
	} else if (engineSmIsLaunchControl) {
		engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::LaunchControl);
	}
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
	if (tps > engineConfiguration->smWotTpsThreshold) {
		return EngineStateMachineState::WOT;
	}

	// Priority 4: AE-driven transient — active while AE threshold is met, then held
	//             for smTransientHoldoffCallbacks slow-callback periods after it drops.
	{
		bool aeActive = engine->module<TpsAccelEnrichment>()->isAboveAccelThreshold ||
		                engine->module<TpsAccelEnrichment>()->isBelowDecelThreshold;
		if (aeActive) {
			m_transientHoldoffRemaining = engineConfiguration->smTransientHoldoffCallbacks;
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
			if (engine->module<DfcoController>()->isOverrun()) {
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

	sm_clutch_switch_e upSw = engineConfiguration->smUpshiftClutchSwitch;
	sm_clutch_switch_e dnSw = engineConfiguration->smDownshiftClutchSwitch;

	// No direction configured → disable shift detection
	if (upSw == sm_clutch_switch_e::None && dnSw == sm_clutch_switch_e::None) {
		engineSmIsUpshifting   = false;
		engineSmIsDownshifting = false;
		return;
	}

	// Map the configured switch enum to a live boolean
	auto switchActive = [&](sm_clutch_switch_e sw) -> bool {
		if (sw == sm_clutch_switch_e::ClutchUp)   { return clutchUpActive;   }
		if (sw == sm_clutch_switch_e::ClutchDown) { return clutchDownActive; }
		return false; // None
	};

	bool upTrigger = switchActive(upSw);
	bool dnTrigger = switchActive(dnSw);

	bool upRose = upTrigger && !m_prevUpTrigger;
	bool dnRose = dnTrigger && !m_prevDnTrigger;
	bool upFell = !upTrigger && m_prevUpTrigger;
	bool dnFell = !dnTrigger && m_prevDnTrigger;

	m_prevUpTrigger = upTrigger;
	m_prevDnTrigger = dnTrigger;

	// Open a shift detection window on a rising switch edge
	if (!m_shiftWindowOpen && (upRose || dnRose)) {
		m_shiftWindowOpen   = true;
		m_shiftWindowOpenMs = nowMs;

		if (upRose && dnRose) {
			// Both directions share the same physical switch. Use current TPS to disambiguate:
			// above idle threshold = driver was on throttle = upshift.
			m_shiftIsUpshift = (tps >= (float)engineConfiguration->smShiftTpsThreshold);
		} else {
			m_shiftIsUpshift = upRose;
		}

		// Clutch-Up switch fires when the pedal releases, which may lag mechanical disengagement.
		// Apply the configured delay before evaluating sensor data.
		sm_clutch_switch_e dirSwitch = m_shiftIsUpshift ? upSw : dnSw;
		bool usingClutchUpSwitch = (dirSwitch == sm_clutch_switch_e::ClutchUp);
		efitimems_t delay = usingClutchUpSwitch
			? static_cast<efitimems_t>(engineConfiguration->smClutchUpDisengagementDelayMs) : 0u;
		m_shiftEvaluateAtMs = nowMs + delay;
	}

	if (!m_shiftWindowOpen) {
		engineSmIsUpshifting   = false;
		engineSmIsDownshifting = false;
		return;
	}

	// Close the window when the triggering switch falls (clutch re-engages) or on timeout
	bool triggerFell    = m_shiftIsUpshift ? upFell : dnFell;
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
	uint16_t lookbackMs = engineConfiguration->smShiftLookbackMs;
	if (lookbackMs < 100) {
		lookbackMs = 300; // safe default when not yet configured
	}
	if (lookbackMs > 1000) {
		lookbackMs = 1000;
	}

	sm_shift_detection_mode_e mode = engineConfiguration->smShiftDetectionMode;

	const SmHistoryEntry* hist = getHistoryAt(lookbackMs);
	if (!hist) {
		// Buffer not yet full — direction from switch alone is sufficient
		return true;
	}

	if (mode == sm_shift_detection_mode_e::SimpleThrottle) {
		// Simple Throttle: TPS position at lookback time reveals driver intent.
		// Above idle threshold at shift start → driver was on throttle → upshift.
		bool tpsWasOpen = hist->tps >= engineConfiguration->smShiftTpsThreshold;
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
			return deltaPer1s > (float)engineConfiguration->smUpshiftRateThreshold;
		} else {
			return deltaPer1s < -(float)engineConfiguration->smDownshiftRateThreshold;
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
			bool tpsWasOpen = hist->tps >= engineConfiguration->smShiftTpsThreshold;
			return isUpshift ? tpsWasOpen : !tpsWasOpen;
		}
		m_vssRateWarningEmitted = false;

		float deltaPer1s = (currentVss - hist->vss) / (static_cast<float>(lookbackMs) / 1000.0f);
		if (isUpshift) {
			return deltaPer1s > (float)engineConfiguration->smUpshiftRateThreshold;
		} else {
			return deltaPer1s < -(float)engineConfiguration->smDownshiftRateThreshold;
		}
	}

	return true; // unknown mode — assume switch direction is correct
}
