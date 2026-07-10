#include "pch.h"
#include "sensor_converter_func.h"

class VehicleSpeedConverter : public SensorConverter {
public:
	SensorResult convert(float frequency) const override {
		auto vssRevPerKm = engineConfiguration->driveWheelRevPerKm * engineConfiguration->vssGearRatio;

		auto pulsePerKm = (vssRevPerKm * engineConfiguration->vssToothCount);

		if (pulsePerKm == 0) {
			// avoid div by 0
			return 0;
		}

		auto kmPerPulse = 1 / pulsePerKm;

		//     1 pulse       3600 sec      1 km       km
		//    ---------  *  ---------- * --------- = ----
		//       sec           1 hr       1 pulse     hr
		float speedKmh = frequency *     3600    * kmPerPulse;

		return applyPlausibilityFilter(speedKmh, frequency);
	}

private:
	// Rejects a VSS pulse implying an impossible acceleration (eg. a single
	// glitched tooth) and dead-reckons a replacement from the last known
	// rate of change instead of the raw reading. Gives up and trusts the
	// sensor again after too many consecutive rejects, so a genuine sudden
	// change (or a too-tight threshold) can't wedge the output forever.
	float applyPlausibilityFilter(float speedKmh, float frequency) const {
		float maxAccelKmhPerSec = engineConfiguration->vssMaxAcceleration;

		if (maxAccelKmhPerSec <= 0 || !m_hasPrevious) {
			acceptReading(speedKmh, 0);
			m_hasPrevious = true;
			return speedKmh;
		}

		// Approximate elapsed time since the previous pulse from its own
		// period - convert() is invoked once per edge, so this tracks real
		// elapsed time closely enough for a plausibility bound.
		float dt = frequency > 0 ? clampF(0.001f, 1 / frequency, 1) : 1;
		float maxDeltaKmh = maxAccelKmhPerSec * dt;

		float delta = speedKmh - m_prevSpeedKmh;

		if (absF(delta) > maxDeltaKmh && m_consecutiveRejects < maxConsecutiveRejects) {
			m_consecutiveRejects++;

			float extrapolatedDelta = clampF(-maxDeltaKmh, m_prevRateKmhPerSec * dt, maxDeltaKmh);
			m_prevSpeedKmh += extrapolatedDelta;
			return m_prevSpeedKmh;
		}

		acceptReading(speedKmh, dt);
		return speedKmh;
	}

	void acceptReading(float speedKmh, float dt) const {
		m_prevRateKmhPerSec = dt > 0 ? (speedKmh - m_prevSpeedKmh) / dt : 0;
		m_prevSpeedKmh = speedKmh;
		m_consecutiveRejects = 0;
	}

	mutable float m_prevSpeedKmh = 0;
	mutable float m_prevRateKmhPerSec = 0;
	mutable bool m_hasPrevious = false;
	mutable uint8_t m_consecutiveRejects = 0;

	static constexpr uint8_t maxConsecutiveRejects = 30;
};
