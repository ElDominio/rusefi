#include "pch.h"
#include "cht_clt_estimator.h"

#if EFI_CHT_CLT_ESTIMATOR

#include "stored_value_sensor.h"
#include "rusefi/interpolation.h"
#include "custom_page.h"

static ChtCltEstimator s_estimator;
static StoredValueSensor s_estimatedClt(SensorType::Clt, MS2NT(500));

// Reference CHT that cltEstHeadTransferRate is anchored to -- see cht_clt_estimator.h.
static constexpr float CLT_EST_CHT_REFERENCE = 90.0f;

void ChtCltEstimator::init() {
	m_lastUpdateNt = getTimeNowNt();
	m_initialized = false;
	m_clt = 0;
	m_laggedAirflow = 0;
	airflow = 0;
	rejectionRate = 0;
	valvePos = 0;
}

void ChtCltEstimator::update() {
	efitick_t nowNt = getTimeNowNt();

	float dtSeconds = static_cast<float>(nowNt - m_lastUpdateNt) / NT_PER_SECOND;
	// Clamp to avoid huge jump on first call or after a long sleep
	dtSeconds = minF(dtSeconds, 1.0f);
	m_lastUpdateNt = nowNt;

	// Same fallback chain as before: IAT, then Ambient, then a hard floor.
	float floorTemp = Sensor::get(SensorType::Iat)
	                      .value_or(Sensor::get(SensorType::AmbientTemperature).value_or(-40.0f));

	auto cht = Sensor::get(SensorType::CylinderHeadTemperature);
	if (!cht) {
		m_clt = getCustomPage()->cltEstFallbackClt;
		m_initialized = false; // re-seed once CHT becomes valid again
		valvePos = 0;
		s_estimatedClt.setValidValue(m_clt, nowNt);
		return;
	}

	float chtVal = cht.Value;

	// Radiator-fail safety override: assume zero radiator cooling above this CHT and hard-lock
	// CLT to CHT directly, bypassing the ODE so there's no lag before the true severity shows up.
	if (chtVal >= getCustomPage()->cltEstRadiatorFailTemp) {
		m_clt = chtVal;
		m_initialized = true;
		airflow = 0;
		rejectionRate = 0;
		s_estimatedClt.setValidValue(m_clt, nowNt);
		return;
	}

	float thermostatTemp = getCustomPage()->cltEstThermostatTemp;

	if (!m_initialized) {
		// Seed the integrator so the very first update doesn't have to ramp up from zero.
		// If CHT is already above the thermostat setpoint, seed at the setpoint rather than
		// CHT itself -- CLT is thermostat-regulated and shouldn't start out above it. Below
		// the setpoint the thermostat is closed, so CHT is the best available seed.
		m_clt = (chtVal > thermostatTemp) ? thermostatTemp : chtVal;
		m_initialized = true;
	}

	// Fan on/off state: PWM duty is out of scope, only relay logic state matters.
	bool fan1On = enginePins.fanRelay.getLogicValue();
	bool fan2On = enginePins.fanRelay2.getLogicValue();

	float vssVal = Sensor::getOrZero(SensorType::VehicleSpeed); // soft-degrades to 0, not a fault

	airflow = (fan1On ? getCustomPage()->cltEstFan1Cfm : 0)
	        + (fan2On ? getCustomPage()->cltEstFan2Cfm : 0)
	        + interpolate2d(vssVal, getCustomPage()->cltEstVssBins, getCustomPage()->cltEstVssAirflow);

	// Coolant thermal-mass lag: airflow itself (fan relay, ram-air) changes in a step, but the
	// bulk coolant's heat-rejection rate doesn't -- it takes time for that airflow change to
	// propagate through the block/radiator/hoses. Distinct from the thermostat valve's own
	// mechanical lag below.
	float coolantLag = getCustomPage()->cltEstCoolantLag;
	if (coolantLag > 0) {
		m_laggedAirflow += (airflow - m_laggedAirflow) * (dtSeconds / coolantLag);
	} else {
		m_laggedAirflow = airflow;
	}

	// Head-to-coolant transfer rate and radiator rejection rate are both modeled as a base value
	// plus a gain, rather than a curve -- see cht_clt_estimator.h for the reference points these
	// are anchored to. Clamped to the same [0, 1] 1/s range the old curves were configured with,
	// since a user-entered gain could otherwise push the rate negative at the domain extremes.
	float rHeat = clampF(0.0f,
		getCustomPage()->cltEstHeadTransferRate + getCustomPage()->cltEstHeadTransferGain * (chtVal - CLT_EST_CHT_REFERENCE),
		1.0f);
	float rRejectBase = clampF(0.0f,
		getCustomPage()->cltEstRadiatorBaseRejection + getCustomPage()->cltEstRadiatorAirflowGain * m_laggedAirflow,
		1.0f);

	// Radiator effectiveness is 1.0 (no change) at cltEstRadiatorReferenceTemp, the "typical" air
	// temp the user's calibration is centered on; the gain scales it up/down from there. Clamped
	// to the same [0, 4] range the old curve was configured with.
	float iatVal = Sensor::get(SensorType::Iat).value_or(Sensor::get(SensorType::AmbientTemperature).value_or(chtVal));
	float iatFactor = clampF(0.0f,
		1.0f + getCustomPage()->cltEstRadiatorEffectivenessIatGain * (iatVal - getCustomPage()->cltEstRadiatorReferenceTemp),
		4.0f);

	// Thermostat valve: target position is a cubic function of how far CLT is from the
	// setpoint, so it barely moves right at thermostatTemp and opens/closes rapidly away from
	// it. A negative ratio (CLT below thermostatTemp) cubes to a negative value and clamps to
	// zero, so the valve is always fully closed below the setpoint. The valve's own response
	// lag (not the CHT/CLT thermal lag) is what lets CLT overshoot before rejection catches up.
	float spread = getCustomPage()->cltEstThermostatSpread;
	float valveTarget;
	if (spread > 0) {
		float ratio = (m_clt - thermostatTemp) / spread;
		valveTarget = clampF(0.0f, ratio * ratio * ratio, 1.0f);
	} else {
		// Degenerate spread: binary valve, fully open at/above thermostatTemp.
		valveTarget = (m_clt >= thermostatTemp) ? 1.0f : 0.0f;
	}

	float lag = getCustomPage()->cltEstThermostatLag;
	if (lag > 0) {
		valvePos += (valveTarget - valvePos) * (dtSeconds / lag);
	} else {
		valvePos = valveTarget;
	}

	rejectionRate = rRejectBase * iatFactor * valvePos;

	float dClt = (rHeat * (chtVal - m_clt) - rejectionRate * (m_clt - thermostatTemp)) * dtSeconds;
	m_clt += dClt;

	// Safety clamp: estimate can never exceed CHT (can't be hotter than its own heat source),
	// and never drop below the IAT/ambient/-40 floor even if the ODE overshoots on a big dt.
	m_clt = clampF(floorTemp, m_clt, chtVal);

	s_estimatedClt.setValidValue(m_clt, nowNt);
}

void initChtCltEstimator() {
	s_estimator.init();
	s_estimatedClt.Register();
}

void updateChtCltEstimator() {
	if (!getCustomPage()->cltFromCht) {
		return;
	}
	s_estimator.update();
}

const ChtCltEstimator& getChtCltEstimator() {
	return s_estimator;
}

#else // !EFI_CHT_CLT_ESTIMATOR

// CLT Estimator compiled out (board set -DEFI_CHT_CLT_ESTIMATOR=FALSE).
void initChtCltEstimator() { }
void updateChtCltEstimator() { }

const ChtCltEstimator& getChtCltEstimator() {
	static ChtCltEstimator s_estimator;
	return s_estimator;
}

#endif // EFI_CHT_CLT_ESTIMATOR
