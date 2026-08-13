#include "fuel_economy_calculator.h"

namespace {
// US MPG = (100 km in miles) * (1 US gallon in liters) / (L/100km)
constexpr float MPG_US_CONVERSION_CONSTANT = 235.214583f;
}

FuelEconomyResult calculateInstantFuelEconomy(
	uint16_t rpm,
	float injectorPulseWidthMs,
	float injectorDeadTimeMs,
	uint16_t injectorFlowCcMin,
	uint8_t cylinderCount,
	float vssKph) {
	FuelEconomyResult result;

	if (rpm == 0 || cylinderCount == 0 || injectorFlowCcMin == 0) {
		return result;
	}

	// DFCO, or dead time not yet cleared: no fuel is actually delivered.
	float effectivePulseWidthMs = injectorPulseWidthMs - injectorDeadTimeMs;
	if (effectivePulseWidthMs <= 0) {
		return result;
	}

	// Sequential injection: each cylinder fires once per 720 degrees (one engine cycle).
	// One engine cycle takes 120000/rpm ms, so cycles per second = rpm / 120.
	float injectionEventsPerSecond = (rpm / 120.0f) * cylinderCount;

	float ccPerInjection = effectivePulseWidthMs * (injectorFlowCcMin / 60000.0f);
	float ccPerSecond = ccPerInjection * injectionEventsPerSecond;

	result.fuelFlowLph = ccPerSecond * 3.6f;

	if (vssKph < FUEL_ECONOMY_MIN_VSS_KPH) {
		return result;
	}

	result.fuelEconomyL100km = result.fuelFlowLph / vssKph * 100.0f;

	if (result.fuelEconomyL100km > 0) {
		result.fuelEconomyMpg = MPG_US_CONVERSION_CONSTANT / result.fuelEconomyL100km;
	}

	return result;
}
