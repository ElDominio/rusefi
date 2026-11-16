#pragma once

#include "engine_module.h"
#include "engine_state_machine.h"
#include "operating_modes_state_generated.h"
#include <rusefi/timer.h>

/**
 * AlphaX operating modes layered on top of the upstream EngineStateMachine
 * (controllers/modules/engine_state_machine): Limp, Sport Mode, Eco Mode, Ghost Cam, Sport Pedal,
 * Quick Warmup, Pops and Bangs, the Eco/Ghost Cam overlay-target blend, and the display-state
 * priority chain. Runs after EngineStateMachine in the module list, so the state machine's flags
 * (engineSmIsIdle, engineSmIsCold, ...) are already current for this slow callback. Does nothing
 * while the state machine is disabled. [tag:alphax_esm_overlay_hook] marks the few places in
 * the state machine itself that cooperate with this module.
 */

enum class PopsAndBangsState : uint8_t {
	Inactive = 0,
	Active   = 1,
	Expired  = 2,
};

class OperatingModes : public operating_modes_state_s, public EngineModule {
public:
	using interface_t = OperatingModes;

	void onSlowCallback() override;

	// Drives the P&B state machine off the given overrun state.
	// Public so unit tests can call it directly without needing a full slow-callback setup.
	void updatePopsAndBangs(bool isOverrun);

	// Eco Mode / Ghost Cam overlay-target transition blend, 0..1: 0 = fully disengaged (normal
	// target), 1 = fully engaged (mode target). While smSlowStateTransitionEnabled is off, this
	// snaps directly to 0/1 alongside engineSmIsEcoMode/engineSmIsGhostCam (legacy instant
	// behavior). While it's on, this ramps linearly over SM_SLOW_TRANSITION_MS on each edge.
	// Consumers that apply an overlay target (AFR, timing, VVT, idle RPM/duty, throttle
	// multiplier, alternator voltage) should interpolate between their normal value and the mode
	// target using this blend instead of switching on the boolean flag directly. The
	// engineSmIsEcoMode/engineSmIsGhostCam flags themselves are unaffected -- they stay instant
	// gates for state logic (mutual exclusivity, dash display, holdoff arming, etc).
	float getEcoModeBlend() const { return m_ecoModeBlend; }
	float getGhostCamBlend() const { return m_ghostCamBlend; }

	// Drives the Eco mode overlay: engages after the engine has been Cruising (brief AE-driven
	// Transient blips tolerated) continuously for ecoModeCruisingTime with MAP at or below
	// ecoModeMapLimit and RPM at or below ecoModeMaxRpm, drops instantly on leaving
	// Cruising/Transient or exceeding the MAP/RPM limit, and can be inhibited by a manual switch /
	// Lua gauge. Once engaged, stays forced active for ecoModeEngageLockTime seconds regardless of
	// state/MAP/VSS/RPM changes (see m_ecoLockTimer) -- Limp/Sport/Inhibit still override the lock
	// instantly. Mutually exclusive with Sport Mode (engineSmIsSportMode out-votes eco, same as
	// Limp). Takes the current state directly so unit tests can drive it without standing up the
	// full state determination.
	void updateEcoMode(EngineStateMachineState currentState);
	void updateSportMode();
	void updateGhostCam();
	void updateSportPedal();

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
	// Eco Mode / Ghost Cam overlay-target transition ramp duration (see getEcoModeBlend()).
	static constexpr float SM_SLOW_TRANSITION_MS = 1000.0f;
	float m_ecoModeBlend      = 0;
	float m_ghostCamBlend     = 0;
	efitimems_t m_lastBlendTickMs = 0;
	void updateOverlayBlend(float& blend, bool active, efitimems_t dtMs);

	// Limp mode latch — set by reportLimpCondition(), never auto-cleared (matches the old
	// ETB-jam behaviour which persisted until reboot). TODO: an un-latch path (e.g. a
	// settings command or a clean key cycle) could be added later if desired.
	bool m_limpModeLatched = false;

	// Eco mode overlay state
	bool isEcoModeSwitchAsserted() const; // manual switch / Lua gauge currently asserting
	bool isEcoModeInhibited() const;      // asserted in Inhibit mode
	Timer m_ecoCruiseTimer;               // measures continuous time in the Cruising state

	// Post-engage lock: reset on the rising edge of engineSmIsEcoMode. While
	// !m_ecoLockTimer.hasElapsedSec(ecoModeEngageLockTime), updateEcoMode() skips all gate/state
	// re-evaluation and leaves engineSmIsEcoMode forced true, so the transient caused by eco's own
	// AFR/timing/VVT/throttle step can't be misread as leaving Cruising and bounce eco back off.
	Timer m_ecoLockTimer;

	// Eco mode's own actuation (ecoThrottleMult, VVT override) is a real RPM-rate transient but
	// not driver-initiated; misreading it as Accelerating/Decelerating would bounce eco straight
	// back off. On any engineSmIsEcoMode edge (see onSlowCallback()) the state machine is asked to
	// hold its rate-based classification off for smTransientHoldTimeMs
	// (EngineStateMachine::holdRateClassification()).
	bool m_prevEcoModeActive = false;

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
