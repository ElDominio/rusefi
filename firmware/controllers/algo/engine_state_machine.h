#pragma once

#include "engine_module.h"
#include "hysteresis.h"
#include "engine_state_machine_state_generated.h"
#include <rusefi/timer.h>

enum class PopsAndBangsState : uint8_t {
	Inactive = 0,
	Active   = 1,
	Expired  = 2,
};

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
	Limp         = 12,
	Eco          = 13,
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

	// Drives the P&B state machine off the given overrun state.
	// Public so unit tests can call it directly without needing a full slow-callback setup.
	void updatePopsAndBangs(bool isOverrun);

	// Drives the Eco mode overlay: engages after the engine has been Cruising continuously for
	// ecoModeCruisingTime with MAP at or below ecoModeMapLimit, drops instantly on leaving Cruising
	// or exceeding the MAP limit, and can be inhibited by a manual switch / Lua gauge. Takes the
	// current state directly so unit tests can drive it without standing up the full state
	// determination.
	void updateEcoMode(EngineStateMachineState currentState);

	// P&B spark-cut overlay: returns the spark skip ratio (0..1) to feed the hard spark
	// limiter. Non-zero only while a cut window is open (every popsAndBangsCutEveryRevs
	// revolutions, held for popsAndBangsCutDurationMs), so raw fuel charges the exhaust and
	// is re-lit when the window closes. Returns 0 outside the window.
	float getPopsAndBangsSparkSkipRatio();

	// Latches the engine into Limp mode. Once latched, the engineSmIsLimp overlay stays set
	// (published every slow callback) and the configured limp limits — rev / boost / ETB /
	// timing / AFR — are enforced by their respective subsystems until power cycle.
	//
	// Currently the only caller is LimpManager::reportEtbJammed (ETB jam). Other fault
	// conditions could call this in the future — see the TODO in onSlowCallback().
	void reportLimpCondition();

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

	// Remaining slow-callback hold-off periods after AE threshold drops
	uint8_t m_transientHoldoffRemaining = 0;

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
	// Raw physical clutch switch states from the previous call, used for edge detection.
	// Tracked independently of which direction(s) are configured to use which switch, so a
	// switch can be consulted for early-open/early-close even when it isn't the one assigned
	// to the direction currently being evaluated (see smSecondClutchSwitchAvailable).
	bool         m_prevClutchUp      = false;
	bool         m_prevClutchDown    = false;

	// Shift latch: once a direction is confirmed, Upshifting/Downshifting is held true until
	// this deadline, masking flicker from the instantaneous rate check (smShiftLatchTimeMs).
	// Tracked separately from m_shiftIsUpshift so a new window opening in the opposite
	// direction doesn't retroactively flip the direction of a still-active latch.
	bool         m_shiftLatched         = false;
	bool         m_shiftLatchedIsUpshift = false;
	efitimems_t  m_shiftLatchUntilMs     = 0;

	void updateTempOverlay();

	// Suppresses repeated VSS-unavailable warnings once emitted
	bool m_vssRateWarningEmitted = false;

	// Limp mode latch — set by reportLimpCondition(), never auto-cleared (matches the old
	// ETB-jam behaviour which persisted until reboot). TODO: an un-latch path (e.g. a
	// settings command or a clean key cycle) could be added later if desired.
	bool m_limpModeLatched = false;

	// Eco mode overlay state
	bool isEcoModeSwitchAsserted() const; // manual switch / Lua gauge currently asserting
	bool isEcoModeInhibited() const;      // asserted in Inhibit mode
	Timer m_ecoCruiseTimer;               // measures continuous time in the Cruising state

	// Pops and Bangs state machine
	bool isPopsAndBangsBlocked() const;
	PopsAndBangsState m_pnbState  = PopsAndBangsState::Inactive;
	bool m_wasPnbOverrun          = false;
	Timer m_pnbOverrunTimer;
	Timer m_pnbActiveTimer;

	// P&B spark-cut overlay state
	bool m_pnbCutActive           = false;  // currently inside a spark-cut window
	uint32_t m_pnbLastFireRev     = 0;      // revolution counter at the last resume-firing point
	float m_pnbCutDurationMs      = 0;      // duration chosen for the current cut window
	Timer m_pnbCutWindowTimer;              // measures the current cut window
};
