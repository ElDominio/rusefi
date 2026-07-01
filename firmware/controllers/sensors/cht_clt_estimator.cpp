#include "pch.h"
#include "cht_clt_estimator.h"
#include "stored_value_sensor.h"
#include "engine_math.h"

static ChtCltEstimator s_estimator;
static StoredValueSensor s_estimatedClt(SensorType::Clt, MS2NT(500));

void ChtCltEstimator::init() {
	m_lastUpdateNt = getTimeNowNt();
	deltaTActual = 0;
	deltaTTarget = 0;
}

void ChtCltEstimator::update() {
	efitick_t nowNt = getTimeNowNt();

	float dtSeconds = static_cast<float>(nowNt - m_lastUpdateNt) / NT_PER_SECOND;
	// Clamp to avoid huge jump on first call or after a long sleep
	dtSeconds = minF(dtSeconds, 1.0f);
	m_lastUpdateNt = nowNt;

	auto cht = Sensor::get(SensorType::CylinderHeadTemperature);
	if (!cht) {
		s_estimatedClt.setValidValue(engineConfiguration->chtEstFallbackClt, nowNt);
		return;
	}

	float chtVal = cht.Value;
	float load = getFuelingLoad();

	deltaTTarget = engineConfiguration->chtEstK0
	             + engineConfiguration->chtEstK1 * chtVal
	             + engineConfiguration->chtEstK2 * load;

	float tau = (deltaTTarget > deltaTActual)
	           ? engineConfiguration->chtEstTauHeat
	           : engineConfiguration->chtEstTauCool;

	if (tau > 0) {
		deltaTActual += (deltaTTarget - deltaTActual) * (dtSeconds / tau);
	} else {
		deltaTActual = deltaTTarget;
	}

	// Estimated CLT can't exceed CHT or be below IAT/ambient
	float floorTemp = Sensor::get(SensorType::Iat)
	                      .value_or(Sensor::get(SensorType::AmbientTemperature).value_or(-40.0f));
	float estimatedClt = clampF(floorTemp, chtVal - deltaTActual, chtVal);

	s_estimatedClt.setValidValue(estimatedClt, nowNt);
}

void initChtCltEstimator() {
	s_estimator.init();
	s_estimatedClt.Register();
}

void updateChtCltEstimator() {
	if (!engineConfiguration->cltFromCht) {
		return;
	}
	s_estimator.update();
}

const ChtCltEstimator& getChtCltEstimator() {
	return s_estimator;
}
