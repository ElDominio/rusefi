/**
 * @file misfire_detection.h
 *
 * Tooth-timing-based misfire detection. Active only while the Engine State Machine
 * reports the Idle state (lowest inertia => highest combustion-torque signal-to-noise).
 *
 * Detection is engine-wide, NOT cylinder-specific: every cylinder's expansion-stroke
 * segment time is compared against a single shared slow rolling average (self-referenced
 * EMA of healthy firings), which cancels fixed trigger-wheel tolerance so only an *acute*
 * deceleration (a real misfire) is flagged. To reject one-off noise, a flagged firing is
 * only counted once at least misfireConsecutiveCount flagged firings fall within the last
 * misfireWindowFirings firings (any cylinder) — an "N of last M" rate test. Once the
 * cumulative counted total reaches misfireCountThreshold the malfunction indicator light
 * is latched (until power cycle) via the generic OBD random-misfire code (P0300).
 *
 * Sub-feature of the Engine State Machine — gated by EFI_MISFIRE_DETECTION (FALSE on the
 * F4 base, TRUE on F7/H7). Reads the SM idle state, so it is only useful when the SM is
 * also enabled at runtime.
 */

#pragma once

#include "engine_module.h"
#include "misfire_detection_state_generated.h"

class MisfireController : public misfire_detection_state_s, public EngineModule {
public:
	using interface_t = MisfireController;

	// Per-tooth: bin teeth into per-cylinder expansion windows and time the segments.
	void onEnginePhase(float rpm, efitick_t edgeTimestamp,
					   angle_t currentPhase, angle_t nextPhase) override;

	// Engine stopped: discard transient detection state (baselines/streaks/timers).
	// Cumulative counters and the MIL latch persist until power cycle ("since key-on").
	void onEngineStop() override;

	// Upper bound on the sliding rate-test window (config field misfireWindowFirings is
	// clamped to this). Sized for two cycles of the largest supported engine.
	static constexpr uint8_t MISFIRE_WINDOW_MAX = 32;

private:
	void evaluateSegment(float segDurationUs);
	void registerMisfire();
	void resetDetectionState();

	// Push one firing result into the ring buffer; count flagged firings among the most
	// recent `windowSize` entries (the "M" of the N-of-last-M rate test).
	void recordFiring(bool flagged);
	uint8_t flaggedInWindow(uint8_t windowSize) const;

	// Per-cylinder expansion-stroke segment timing (window TDC+start .. TDC+end). The window
	// is still tracked per cylinder so each firing's segment is timed separately, but every
	// segment is judged against the single shared baseline below — detection is engine-wide.
	efitick_t m_segStart[MAX_CYLINDER_COUNT];
	bool      m_segActive[MAX_CYLINDER_COUNT];

	// Single shared self-referenced baseline: slow EMA of healthy segment duration (us),
	// updated by every clean firing regardless of cylinder.
	float m_emaSeg = 0;
	bool  m_emaSeeded = false;

	// Ring buffer of the most recent firing results (true = flagged), engine-wide. The
	// rate test counts the flagged entries among the last misfireWindowFirings of them.
	bool    m_window[MISFIRE_WINDOW_MAX];
	uint8_t m_windowHead = 0;   // index of the next slot to write (one past newest)
	uint8_t m_windowFill = 0;   // valid entries so far (ramps up to MISFIRE_WINDOW_MAX)

	// Tracks idle<->non-idle transitions so we can reset baselines when re-entering idle.
	bool m_wasMonitoring = false;
};
