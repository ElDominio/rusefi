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

	// Priorities 6 & 7: off-throttle (idle or coasting split by RPM)
	if (tps < engineConfiguration->smIdleTpsThreshold) {
		float idleExitRpm  = engineConfiguration->smIdleExitRpm;
		float hysteresisBand = engineConfiguration->smIdleRpmHysteresis;

		// Hysteresis: true = rpm is high (coasting), false = rpm is low (idle)
		bool isHighRpm = m_idleHysteresis.test(
			rpm,
			idleExitRpm + hysteresisBand,   // rising threshold: clearly coasting
			idleExitRpm - hysteresisBand    // falling threshold: clearly idling
		);

		return isHighRpm ? EngineStateMachineState::Coasting
		                 : EngineStateMachineState::Idle;
	}

	// Default: part-throttle cruising
	return EngineStateMachineState::Cruising;
}
