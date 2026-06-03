#pragma once

#include "engine_module.h"
#include "hysteresis.h"
#include "engine_state_machine_state_generated.h"

enum class EngineStateMachineState : uint8_t {
	Off        = 0,
	Cranking   = 1,
	Afterstart = 2,
	Idle       = 3,
	Coasting   = 4,
	Transient  = 5,
	WOT        = 6,
	Cruising   = 7,
	Overrun    = 8,
	// 9-11 reserved for future overlay states (Warmup, LimpMode, LaunchControl, GearShift)
};

class EngineStateMachine : public engine_state_machine_state_s, public EngineModule {
public:
	using interface_t = EngineStateMachine;

	void onSlowCallback() override;

	EngineStateMachineState getCurrentState() const;
	bool isEnabled() const;

private:
	EngineStateMachineState determineState(float rpm, float tps);

	// RPM hysteresis for the idle/coasting boundary to prevent state flapping
	Hysteresis m_idleHysteresis;

	// VSS hysteresis for the coasting/idle boundary (2 km/h band)
	Hysteresis m_vssHysteresis;

	// RPM hysteresis for the coasting/overrun boundary
	Hysteresis m_overrunHysteresis;

	// Previous TPS value for rate-of-change computation
	float m_prevTps = 0.0f;

	// Cached tps delta rate in %/s (updated each slow callback)
	float m_tpsDeltaPerSecond = 0.0f;

	// Current computed state — uint8_t stores are atomic in hardware on Cortex-M
	EngineStateMachineState m_currentState = EngineStateMachineState::Off;
};
