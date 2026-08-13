#pragma once

#include <cstdint>

// Instantaneous fuel flow/economy figures derived from injector timing and vehicle speed.
struct FuelEconomyResult {
	// Fuel flow rate, liters per hour.
	float fuelFlowLph = 0;
	// Fuel economy, liters per 100 km. Zero below the minimum speed threshold.
	float fuelEconomyL100km = 0;
	// Fuel economy, US miles per gallon. Zero below the minimum speed threshold or with no fuel flow.
	float fuelEconomyMpg = 0;
};

// Vehicle speed, km/h, below which distance-based economy figures are not meaningful
// and are reported as zero instead of dividing by a near-zero speed.
static constexpr float FUEL_ECONOMY_MIN_VSS_KPH = 3.0f;

// Computes instantaneous fuel flow and economy for a sequentially-injected engine, ie
// one injection event per cylinder per 720 degrees (one engine cycle).
//
// injectorPulseWidthMs is the raw commanded injector on-time, including dead time.
// The fuel-delivering portion is (injectorPulseWidthMs - injectorDeadTimeMs); when that
// is not positive (DFCO, or dead time not cleared) every result field is zero.
//
// vssKph below FUEL_ECONOMY_MIN_VSS_KPH yields zero for the distance-based economy
// figures (fuelEconomyL100km, fuelEconomyMpg) while fuelFlowLph is still reported.
// Callers without a valid VSS reading should pass 0 so the gauge reads zero, per the
// "VSS required" contract of this feature.
FuelEconomyResult calculateInstantFuelEconomy(
	uint16_t rpm,
	float injectorPulseWidthMs,
	float injectorDeadTimeMs,
	uint16_t injectorFlowCcMin,
	uint8_t cylinderCount,
	float vssKph);
