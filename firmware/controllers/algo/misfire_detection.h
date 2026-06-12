/**
 * @file misfire_detection.h
 *
 * Tooth-timing-based misfire detection. Active only while the Engine State Machine
 * reports the Idle state (lowest inertia => highest combustion-torque signal-to-noise).
 *
 * Each cylinder's expansion-stroke segment time is compared against its own slow
 * rolling average (self-referenced EMA), which cancels fixed trigger-wheel tolerance
 * and chronic weak-cylinder bias, so only an *acute* deceleration (a real misfire)
 * trips it. A configurable number of consecutive flagged firings arms the per-cylinder
 * counter; once the cumulative total reaches the threshold the malfunction indicator
 * light is latched (until power cycle) via an OBD misfire code.
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

private:
	void evaluateSegment(uint8_t cyl, float segDurationUs);
	void registerMisfire(uint8_t cyl);
	void resetDetectionState();

	// Per-cylinder expansion-stroke segment timing (window TDC+start .. TDC+end)
	efitick_t m_segStart[MAX_CYLINDER_COUNT];
	bool      m_segActive[MAX_CYLINDER_COUNT];

	// Per-cylinder self-referenced baseline: slow EMA of healthy segment duration (us)
	float m_emaSeg[MAX_CYLINDER_COUNT];
	bool  m_emaSeeded[MAX_CYLINDER_COUNT];

	// Consecutive-event debounce: a streak must reach misfireConsecutiveCount to arm;
	// once armed every flagged firing counts, a clean firing disarms.
	uint8_t m_streak[MAX_CYLINDER_COUNT];
	bool    m_armed[MAX_CYLINDER_COUNT];

	// Tracks idle<->non-idle transitions so we can reset baselines when re-entering idle.
	bool m_wasMonitoring = false;
};
