#include "pch.h"
#include "engine_state_machine.h"

// Slow callback runs at 20 Hz; deltaTps is per-callback, multiply by 50 to get %/s
static constexpr float SLOW_CALLBACKS_PER_SECOND = 1000.0f / SLOW_CALLBACK_PERIOD_MS;

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

	float rpm = engine->rpmCalculator.getCachedRpm();

	auto tpsSensor = Sensor::get(SensorType::DriverThrottleIntent);
	float tps = tpsSensor ? tpsSensor.Value : 0.0f;

	// Compute TPS rate of change (%/s)
	m_tpsDeltaPerSecond = (tps - m_prevTps) * SLOW_CALLBACKS_PER_SECOND;
	m_prevTps = tps;

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

	// Priority 3: initial seconds after first reaching running RPM
	float secondsSinceStart = engine->rpmCalculator.getSecondsSinceEngineStart(getTimeNowNt());
	if (secondsSinceStart < engineConfiguration->smAfterStartDuration) {
		return EngineStateMachineState::Afterstart;
	}

	// Priority 4: wide open throttle
	if (tps > engineConfiguration->smWotTpsThreshold) {
		return EngineStateMachineState::WOT;
	}

	// Priority 5: rapid throttle movement
	if (fabsf(m_tpsDeltaPerSecond) > engineConfiguration->smTransientTpsRateThreshold) {
		return EngineStateMachineState::Transient;
	}

	// Priorities 6–8: off-throttle (overrun / coasting / idle split by RPM and VSS)
	if (tps < engineConfiguration->smIdleTpsThreshold) {
		float idleExitRpm    = engineConfiguration->smIdleExitRpm;
		float idleBand       = engineConfiguration->smIdleRpmHysteresis;

		// Hysteresis: true = rpm is high (coasting), false = rpm is low (idle)
		bool isHighRpm = m_idleHysteresis.test(
			rpm,
			idleExitRpm + idleBand,   // rising threshold: clearly coasting
			idleExitRpm - idleBand    // falling threshold: clearly idling
		);

		uint8_t vssThreshold = engineConfiguration->smCoastingVssThreshold;
		bool isHighVss = false;
		if (vssThreshold > 0) {
			float vss = Sensor::getOrZero(SensorType::VehicleSpeed);
			isHighVss = m_vssHysteresis.test(vss, vssThreshold, vssThreshold - 2.0f);
		}

		if (isHighRpm || isHighVss) {
			int16_t overrunRpm  = engineConfiguration->smOverrunRpmThreshold;
			int16_t overrunBand = engineConfiguration->smOverrunRpmHysteresis;
			if (overrunRpm > 0 && m_overrunHysteresis.test(rpm, overrunRpm + overrunBand, overrunRpm - overrunBand)) {
				return EngineStateMachineState::Overrun;
			}
			return EngineStateMachineState::Coasting;
		}

		return EngineStateMachineState::Idle;
	}

	// Default: part-throttle cruising
	return EngineStateMachineState::Cruising;
}
