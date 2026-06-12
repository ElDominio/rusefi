#include "pch.h"
#include "misfire_detection.h"

#if EFI_MISFIRE_DETECTION

#include "custom_page.h"
#include "engine_state_machine.h"
#include "malfunction_central.h"
#include "efilib.h"

// Baseline EMA blend factor — small so a single misfire barely moves the per-cylinder
// baseline, and the baseline is only updated on clean (non-flagged) firings anyway.
static constexpr float MISFIRE_EMA_ALPHA = 0.05f;

// Maps a 0-based cylinder index to its OBD per-cylinder misfire code (P0301..P0312),
// falling back to the generic random/multiple misfire code (P0300).
static ObdCode misfireObdCode(uint8_t cyl) {
	if (cyl < 12) {
		return static_cast<ObdCode>(static_cast<uint16_t>(ObdCode::OBD_Cylinder_1_Misfire) + cyl);
	}
	return ObdCode::OBD_Random_Misfire;
}

void MisfireController::resetDetectionState() {
	for (size_t i = 0; i < MAX_CYLINDER_COUNT; i++) {
		m_segActive[i] = false;
		m_emaSeeded[i] = false;
		m_streak[i] = 0;
		m_armed[i] = false;
	}
}

void MisfireController::onEngineStop() {
	// Prepare for the next start: drop baselines/streaks/timers. Cumulative counters
	// (misfireTotalCount / misfireCylCount) and the MIL latch persist until power cycle.
	resetDetectionState();
}

void MisfireController::registerMisfire(uint8_t cyl) {
	if (misfireCylCount[cyl] < UINT16_MAX) {
		misfireCylCount[cyl]++;
	}
	if (misfireTotalCount < UINT16_MAX) {
		misfireTotalCount++;
	}
	misfireLastCylinder = cyl + 1;

	uint16_t threshold = getCustomPage()->misfireCountThreshold;
	if (!misfireLatched && threshold > 0 && misfireTotalCount >= threshold) {
		misfireLatched = true;
		addError(misfireObdCode(cyl));
	}
}

void MisfireController::evaluateSegment(uint8_t cyl, float segDurationUs) {
	if (segDurationUs <= 0) {
		// Degenerate timing (phase wrap / first sample) — ignore.
		return;
	}

	if (!m_emaSeeded[cyl]) {
		// Warm-up: seed the baseline with the first valid segment, don't judge it.
		m_emaSeg[cyl] = segDurationUs;
		m_emaSeeded[cyl] = true;
		m_streak[cyl] = 0;
		m_armed[cyl] = false;
		return;
	}

	float ratio = getCustomPage()->misfireThresholdRatio;
	bool flagged = segDurationUs > m_emaSeg[cyl] * ratio;

	if (flagged) {
		// Streak must reach the consecutive count to arm; once armed, every flagged
		// firing counts (so a hard fault racks up fast). Freeze the baseline meanwhile.
		if (m_streak[cyl] < UINT8_MAX) {
			m_streak[cyl]++;
		}
		if (m_streak[cyl] >= getCustomPage()->misfireConsecutiveCount) {
			m_armed[cyl] = true;
		}
		if (m_armed[cyl]) {
			registerMisfire(cyl);
		}
	} else {
		// Clean firing: disarm and let the baseline track slow RPM/load drift.
		m_streak[cyl] = 0;
		m_armed[cyl] = false;
		m_emaSeg[cyl] += MISFIRE_EMA_ALPHA * (segDurationUs - m_emaSeg[cyl]);
	}
}

void MisfireController::onEnginePhase(float /*rpm*/, efitick_t edgeTimestamp,
									  angle_t currentPhase, angle_t nextPhase) {
	bool enabled = getCustomPage()->misfireDetectionEnabled;
	bool idle = engine->module<EngineStateMachine>().unmock().engineSmIsIdle;
	bool monitoring = enabled && idle;

	misfireDetectionActive = monitoring;

	if (!monitoring) {
		// Reset transient detection state once when leaving idle so a later re-idle
		// re-seeds baselines instead of judging against stale norms.
		if (m_wasMonitoring) {
			resetDetectionState();
		}
		m_wasMonitoring = false;
		return;
	}
	m_wasMonitoring = true;

	const auto* page = getCustomPage();
	float windowStartOffset = page->misfireWindowStart;
	float windowEndOffset = page->misfireWindowEnd;

	size_t cylCount = engineConfiguration->cylindersCount;
	for (size_t i = 0; i < cylCount; i++) {
		// getAngleOffset() gives each cylinder's TDC reference within the engine cycle
		// (handles firing order + odd-fire). The window sits in the expansion stroke.
		angle_t tdc = engine->cylinders[i].getAngleOffset();

		angle_t windowStart = tdc + windowStartOffset;
		wrapAngle(windowStart, "misfireStart", ObdCode::CUSTOM_ERR_6562);
		angle_t windowEnd = tdc + windowEndOffset;
		wrapAngle(windowEnd, "misfireEnd", ObdCode::CUSTOM_ERR_6562);

		if (isPhaseInRange(windowStart, currentPhase, nextPhase)) {
			m_segStart[i] = edgeTimestamp;
			m_segActive[i] = true;
		}

		if (m_segActive[i] && isPhaseInRange(windowEnd, currentPhase, nextPhase)) {
			m_segActive[i] = false;
			float segUs = NT2US((float)(edgeTimestamp - m_segStart[i]));
			evaluateSegment(i, segUs);
		}
	}
}

#else // !EFI_MISFIRE_DETECTION

void MisfireController::onEnginePhase(float, efitick_t, angle_t, angle_t) {}
void MisfireController::onEngineStop() {}
void MisfireController::evaluateSegment(uint8_t, float) {}
void MisfireController::registerMisfire(uint8_t) {}
void MisfireController::resetDetectionState() {}

#endif // EFI_MISFIRE_DETECTION
