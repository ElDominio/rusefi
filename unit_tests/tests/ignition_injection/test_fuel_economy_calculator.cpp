#include "pch.h"

#include "fuel_economy_calculator.h"

TEST(FuelEconomyCalculator, BasicFlowRate) {
	// 4 cylinders, 3000 rpm, 3.5ms commanded pulse width, 0.5ms dead time (3ms effective),
	// 250cc/min injectors -> hand-calculated 4.5 L/hr
	auto result = calculateInstantFuelEconomy(3000, 3.5f, 0.5f, 250, 4, 100);

	EXPECT_NEAR(4.5f, result.fuelFlowLph, 0.001f);
}

TEST(FuelEconomyCalculator, DfcoZeroPulseWidth) {
	// Pulse width at or below dead time: no fuel is actually delivered (DFCO)
	auto result = calculateInstantFuelEconomy(3000, 0.5f, 0.5f, 250, 4, 100);

	EXPECT_EQ(0, result.fuelFlowLph);
	EXPECT_EQ(0, result.fuelEconomyL100km);
	EXPECT_EQ(0, result.fuelEconomyMpg);
}

TEST(FuelEconomyCalculator, DfcoPulseWidthBelowDeadTime) {
	auto result = calculateInstantFuelEconomy(3000, 0.3f, 0.5f, 250, 4, 100);

	EXPECT_EQ(0, result.fuelFlowLph);
}

TEST(FuelEconomyCalculator, ZeroRpm) {
	auto result = calculateInstantFuelEconomy(0, 3.5f, 0.5f, 250, 4, 100);

	EXPECT_EQ(0, result.fuelFlowLph);
	EXPECT_EQ(0, result.fuelEconomyL100km);
	EXPECT_EQ(0, result.fuelEconomyMpg);
}

TEST(FuelEconomyCalculator, ZeroCylinderCount) {
	auto result = calculateInstantFuelEconomy(3000, 3.5f, 0.5f, 250, 0, 100);

	EXPECT_EQ(0, result.fuelFlowLph);
}

TEST(FuelEconomyCalculator, ZeroInjectorFlow) {
	auto result = calculateInstantFuelEconomy(3000, 3.5f, 0.5f, 0, 4, 100);

	EXPECT_EQ(0, result.fuelFlowLph);
}

TEST(FuelEconomyCalculator, SpeedBelowMinimumReportsZeroDistanceEconomy) {
	// Below the 3 km/h threshold: flow rate is still valid, but L/100km and MPG
	// would require dividing by a near-zero speed, so they're reported as zero.
	auto result = calculateInstantFuelEconomy(3000, 3.5f, 0.5f, 250, 4, 2.9f);

	EXPECT_NEAR(4.5f, result.fuelFlowLph, 0.001f);
	EXPECT_EQ(0, result.fuelEconomyL100km);
	EXPECT_EQ(0, result.fuelEconomyMpg);
}

TEST(FuelEconomyCalculator, MissingVssReportsZeroDistanceEconomy) {
	// Callers pass 0 when the VSS sensor is unavailable/invalid, which lands in the
	// same near-zero-speed branch as a stopped vehicle.
	auto result = calculateInstantFuelEconomy(3000, 3.5f, 0.5f, 250, 4, 0);

	EXPECT_NEAR(4.5f, result.fuelFlowLph, 0.001f);
	EXPECT_EQ(0, result.fuelEconomyL100km);
	EXPECT_EQ(0, result.fuelEconomyMpg);
}

TEST(FuelEconomyCalculator, DistanceEconomyAtCruise) {
	// 4 cylinders, 2000 rpm, 2.5ms commanded pulse width, 0.5ms dead time (2ms effective),
	// 200cc/min injectors, cruising at 100 kph
	auto result = calculateInstantFuelEconomy(2000, 2.5f, 0.5f, 200, 4, 100);

	// cc/injection = 2ms * 200/60000 = 0.006667cc
	// events/sec = (2000/120) * 4 = 66.667
	// cc/sec = 0.4444, L/hr = 1.6
	EXPECT_NEAR(1.6f, result.fuelFlowLph, 0.001f);
	EXPECT_NEAR(1.6f, result.fuelEconomyL100km, 0.001f);
	EXPECT_NEAR(147.0f, result.fuelEconomyMpg, 0.5f);
}
