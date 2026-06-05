#pragma once

#include "engine_module.h"
#include "hysteresis.h"
#include "engine_state_machine_state_generated.h"

enum class EngineStateMachineState : uint8_t {
	Off          = 0,
	Cranking     = 1,
	Afterstart   = 2,
	Idle         = 3,
	Coasting     = 4,
	Transient    = 5,
	WOT          = 6,
	Cruising     = 7,
	Overrun      = 8,
	LaunchControl = 9,
	Upshifting   = 10,
	Downshifting = 11,
};

// History buffer: 20 samples at 20 Hz = 1000 ms of lookback
static constexpr int SM_HISTORY_SIZE = 20;
// A shift window that stays open longer than this is considered stale and is closed.
static constexpr efitimems_t SM_SHIFT_TIMEOUT_MS = 3000;

class EngineStateMachine : public engine_state_machine_state_s, public EngineModule {
public:
	using interface_t = EngineStateMachine;

	void onSlowCallback() override;

	EngineStateMachineState getCurrentState() const;
	bool isEnabled() const;

private:
	struct SmHistoryEntry {
		efitimems_t timestampMs;
		float tps;
		float rpm;
		float vss;
	};

	EngineStateMachineState determineState(float rpm, float tps);
	void recordHistory(float tps, float rpm, float vss, efitimems_t nowMs);
	const SmHistoryEntry* getHistoryAt(efitimems_t lookbackMs) const;
	void updateShiftDetection(float tps, float rpm, float vss, efitimems_t nowMs);
	bool evaluateShiftDirection(bool isUpshift, float currentRpm, float currentVss, efitimems_t nowMs);

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

	// Sensor history circular buffer (oldest→newest as head advances)
	SmHistoryEntry m_history[SM_HISTORY_SIZE]{};
	int m_historyHead  = 0;
	int m_historyCount = 0;

	// Shift detection window state
	bool         m_shiftWindowOpen   = false;
	bool         m_shiftIsUpshift    = false;
	efitimems_t  m_shiftWindowOpenMs = 0;
	efitimems_t  m_shiftEvaluateAtMs = 0;
	bool         m_prevUpTrigger     = false;
	bool         m_prevDnTrigger     = false;

	// Suppresses repeated VSS-unavailable warnings once emitted
	bool m_vssRateWarningEmitted = false;
};
