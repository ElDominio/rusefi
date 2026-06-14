#include "pch.h"
#include "misfire_detection.h"

#if EFI_MISFIRE_DETECTION

#include "custom_page.h"
#include "engine_state_machine.h"
#include "malfunction_central.h"
#include "efilib.h"

// Fallbacks if misfireEmaAlphaDecel/Fall are misconfigured (zero or out of range).
static constexpr float MISFIRE_EMA_ALPHA_RISE_DEFAULT = 0.05f;
static constexpr float MISFIRE_EMA_ALPHA_FALL_DEFAULT = 0.005f;

void MisfireController::resetDetectionState() {
	for (size_t i = 0; i < MAX_CYLINDER_COUNT; i++) {
		m_segActive[i] = false;
	}
	m_emaSeeded = false;
	m_emaSeg = 0;
	m_windowHead = 0;
	m_windowFill = 0;
}

void MisfireController::onEngineStop() {
	// Prepare for the next start: drop the baseline, rate-test window and segment timers.
	// misfireTotalCount and the MIL latch persist until power cycle.
	resetDetectionState();
}

void MisfireController::recordFiring(bool flagged) {
	m_window[m_windowHead] = flagged;
	m_windowHead = (m_windowHead + 1) % MISFIRE_WINDOW_MAX;
	if (m_windowFill < MISFIRE_WINDOW_MAX) {
		m_windowFill++;
	}
}

uint8_t MisfireController::flaggedInWindow(uint8_t windowSize) const {
	uint8_t n = windowSize < m_windowFill ? windowSize : m_windowFill;
	uint8_t flagged = 0;
	uint8_t idx = m_windowHead; // one past the newest entry
	for (uint8_t k = 0; k < n; k++) {
		idx = (idx == 0) ? (MISFIRE_WINDOW_MAX - 1) : (idx - 1);
		if (m_window[idx]) {
			flagged++;
		}
	}
	return flagged;
}

void MisfireController::registerMisfire() {
	if (misfireTotalCount < UINT16_MAX) {
		misfireTotalCount++;
	}

	uint16_t threshold = getCustomPage()->misfireCountThreshold;
	if (!misfireLatched && threshold > 0 && misfireTotalCount >= threshold) {
		misfireLatched = true;
		// Engine-wide detection => generic random/multiple-cylinder misfire code (P0300).
		addError(ObdCode::OBD_Random_Misfire);
	}
}

void MisfireController::evaluateSegment(float segDurationUs) {
	if (segDurationUs <= 0) {
		// Degenerate timing (phase wrap / first sample) — ignore.
		return;
	}

	if (!m_emaSeeded) {
		// Warm-up: seed the shared baseline with the first valid segment, don't judge it.
		m_emaSeg = segDurationUs;
		m_emaSeeded = true;
		return;
	}

	float ratio = getCustomPage()->misfireThresholdRatio;
	float thresh = m_emaSeg * ratio;
	bool flagged = segDurationUs > thresh;

	// Update live data so the log always shows current segment vs. baseline/threshold.
	misfireLastSegUs = segDurationUs;
	misfireEmaUs     = m_emaSeg;
	misfireThreshUs  = thresh;

	// Slot this firing into the engine-wide rate-test window.
	recordFiring(flagged);

	if (flagged) {
		// "N of last M": a flagged firing only counts once at least misfireConsecutiveCount
		// flagged firings (any cylinder) fall within the last misfireWindowFirings firings.
		// This catches both a single dead cylinder and a whole-engine breakup while ignoring
		// a lone outlier. The baseline is frozen on flagged firings so it cannot drift up.
		uint8_t windowSize = getCustomPage()->misfireWindowFirings;
		if (windowSize < 1) {
			windowSize = 1;
		}
		if (windowSize > MISFIRE_WINDOW_MAX) {
			windowSize = MISFIRE_WINDOW_MAX;
		}
		uint8_t need = getCustomPage()->misfireConsecutiveCount;
		if (need < 1) {
			need = 1;
		}
		if (flaggedInWindow(windowSize) >= need) {
			registerMisfire();
		}
	} else {
		// Clean firing: update the shared baseline with an asymmetric EMA.
		// Positive delta (segment slower than baseline = engine decelerating) uses a higher
		// alpha so genuine RPM drops are tracked quickly and don't cause false positives.
		// Negative delta (segment faster = engine recovering) uses a lower alpha to resist
		// upward baseline drift from the borderline-slow clean firings that occur between
		// misfires when only one or two cylinders are dead.
		float diff = segDurationUs - m_emaSeg;
		float alpha;
		if (diff > 0.0f) {
			alpha = getCustomPage()->misfireEmaAlphaDecel;
			if (alpha <= 0.0f || alpha > 1.0f) {
				alpha = MISFIRE_EMA_ALPHA_RISE_DEFAULT;
			}
		} else {
			alpha = getCustomPage()->misfireEmaAlphaAccel;
			if (alpha <= 0.0f || alpha > 1.0f) {
				alpha = MISFIRE_EMA_ALPHA_FALL_DEFAULT;
			}
		}
		m_emaSeg += alpha * diff;
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
			evaluateSegment(segUs);
		}
	}
}

#else // !EFI_MISFIRE_DETECTION

void MisfireController::onEnginePhase(float, efitick_t, angle_t, angle_t) {}
void MisfireController::onEngineStop() {}
void MisfireController::evaluateSegment(float) {}
void MisfireController::registerMisfire() {}
void MisfireController::resetDetectionState() {}
void MisfireController::recordFiring(bool) {}
uint8_t MisfireController::flaggedInWindow(uint8_t) const { return 0; }

#endif // EFI_MISFIRE_DETECTION
