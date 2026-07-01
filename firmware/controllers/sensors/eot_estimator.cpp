#include "pch.h"
#include "eot_estimator.h"
#include "stored_value_sensor.h"

static EotEstimator s_estimator;
static StoredValueSensor s_estimatedEot(SensorType::OilTemperature, MS2NT(500));

void EotEstimator::init() {
	m_lastUpdateNt = getTimeNowNt();
	deltaTActual = 0;
	deltaTTarget = 0;
	// Sentinel above any real EOT reading so the first update's cooling-only clamp is a no-op.
	m_lastEot = 1000;
}

void EotEstimator::update() {
	efitick_t nowNt = getTimeNowNt();

	float dtSeconds = static_cast<float>(nowNt - m_lastUpdateNt) / NT_PER_SECOND;
	// Clamp to avoid huge jump on first call or after a long sleep
	dtSeconds = minF(dtSeconds, 1.0f);
	m_lastUpdateNt = nowNt;

	// CHT and oil pressure are both required: oil pressure is our proxy for oil flow rate,
	// which strongly affects how close the oil tracks the head temperature.
	auto cht = Sensor::get(SensorType::CylinderHeadTemperature);
	auto oilPressure = Sensor::get(SensorType::OilPressure);
	if (!cht || !oilPressure) {
		s_estimatedEot.setValidValue(engineConfiguration->eotEstFallbackEot, nowNt);
		return;
	}

	float chtVal = cht.Value;
	float iatVal = Sensor::get(SensorType::Iat).value_or(Sensor::get(SensorType::AmbientTemperature).value_or(chtVal));
	float oilPressureVal = oilPressure.Value;
	float rpmVal = Sensor::get(SensorType::Rpm).value_or(0);

	// With no flow there's no convective coupling to CHT at all: the steady-state formula
	// below doesn't apply, and the estimate must only be allowed to fall toward ambient.
	bool oilCirculating = rpmVal > 0 && oilPressureVal > 0;

	deltaTTarget = engineConfiguration->eotEstK0
	             + engineConfiguration->eotEstK1 * chtVal
	             + engineConfiguration->eotEstK2 * iatVal
	             + engineConfiguration->eotEstK3 * oilPressureVal;

	if (!oilCirculating) {
		// Floor the target delta at the current delta: the integrator below can then only
		// grow the delta (estimate falling), never shrink it (estimate rising toward CHT).
		deltaTTarget = maxF(deltaTTarget, deltaTActual);
	}

	// deltaTTarget < deltaTActual means the implied steady-state EOT (CHT - deltaTTarget) is
	// above the current estimate, i.e. the oil should be heating up toward it.
	float tau = (deltaTTarget < deltaTActual)
	           ? engineConfiguration->eotEstTauHeat
	           : engineConfiguration->eotEstTauCool;

	if (tau > 0) {
		deltaTActual += (deltaTTarget - deltaTActual) * (dtSeconds / tau);
	} else {
		deltaTActual = deltaTTarget;
	}

	// Oil also picks up heat from the crank/rod bearings and other internals, so it commonly
	// runs hotter than CHT, not just up to it: only floor at IAT/ambient, and ceiling at a fixed
	// absolute sanity limit (350F) rather than at chtVal.
	float floorTemp = Sensor::get(SensorType::Iat)
	                      .value_or(Sensor::get(SensorType::AmbientTemperature).value_or(-40.0f));
	constexpr float eotHardCeiling = 176.7f; // 350F
	float estimatedEot = clampF(floorTemp, chtVal - deltaTActual, eotHardCeiling);

	if (!oilCirculating) {
		// Belt-and-suspenders: even a CHT heat-soak bump can't drag the published estimate
		// upward while there's no circulation to actually heat the oil.
		estimatedEot = minF(estimatedEot, m_lastEot);
		deltaTActual = chtVal - estimatedEot;
	}

	m_lastEot = estimatedEot;
	s_estimatedEot.setValidValue(estimatedEot, nowNt);
}

void initEotEstimator() {
	s_estimator.init();
	s_estimatedEot.Register();
}

void updateEotEstimator() {
	if (!engineConfiguration->eotFromIatCht) {
		return;
	}
	s_estimator.update();
}

const EotEstimator& getEotEstimator() {
	return s_estimator;
}
