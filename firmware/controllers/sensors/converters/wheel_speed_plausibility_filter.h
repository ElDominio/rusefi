#pragma once

#include "custom_page.h"

// Rejects a reading implying an impossible rate of change (a single glitched tooth) and
// dead-reckons a replacement from the last known rate of change instead of the raw reading, same
// idiom as vehicle_speed_converter.h's applyPlausibilityFilter(). Percentage-based (rather than an
// absolute per-unit bound) so the ONE shared config value (getCustomPage()->wheelSpeedMaxAccelPercent)
// is physically meaningful whichever native unit the owning converter reads in -- RPM for Output
// Shaft Speed, km/h for Front/Rear Axle. Each owning converter holds its own independent instance
// (its own state), they just all read the same shared threshold.
class WheelSpeedPlausibilityFilter {
public:
	float apply(float rawValue, float dt) const {
		float maxPercent = getCustomPage()->wheelSpeedMaxAccelPercent;

		if (maxPercent <= 0 || !m_hasPrevious) {
			accept(rawValue, 0);
			return rawValue;
		}

		// Floor the percentage base so a near-zero starting reading (eg. pulling away from a
		// stop) doesn't produce a near-zero maxDelta and permanently reject the first real
		// acceleration.
		float base = absF(m_prevValue) < 1.0f ? 1.0f : absF(m_prevValue);
		float maxDelta = base * (maxPercent / 100.0f) * dt;

		float delta = rawValue - m_prevValue;

		if (absF(delta) > maxDelta && m_consecutiveRejects < maxConsecutiveRejects) {
			m_consecutiveRejects++;

			float extrapolatedDelta = clampF(-maxDelta, m_prevRate * dt, maxDelta);
			m_prevValue += extrapolatedDelta;
			return m_prevValue;
		}

		accept(rawValue, dt);
		return rawValue;
	}

private:
	void accept(float value, float dt) const {
		m_prevRate = dt > 0 ? (value - m_prevValue) / dt : 0;
		m_prevValue = value;
		m_hasPrevious = true;
		m_consecutiveRejects = 0;
	}

	mutable float m_prevValue = 0;
	mutable float m_prevRate = 0;
	mutable bool m_hasPrevious = false;
	mutable uint8_t m_consecutiveRejects = 0;

	static constexpr uint8_t maxConsecutiveRejects = 30;
};
