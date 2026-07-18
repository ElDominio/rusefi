// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/algo/fuel/fuel_computer.txt
// by class com.rusefi.output.CHeaderConsumer
// begin
#pragma once
#include "rusefi_types.h"
// start of running_fuel_s
struct running_fuel_s {
	/**
	 * Fuel: Post cranking mult
	 * offset 0
	 */
	float postCrankingFuelCorrection = (float)0;
	/**
	 * @@GAUGE_NAME_FUEL_IAT_CORR@@
	 * offset 4
	 */
	float intakeTemperatureCoefficient = (float)0;
	/**
	 * @@GAUGE_NAME_FUEL_CLT_CORR@@
	 * offset 8
	 */
	float coolantTemperatureCoefficient = (float)0;
	/**
	 * units: secs
	 * offset 12
	 */
	float timeSinceCrankingInSecs = (float)0;
	/**
	 * @@GAUGE_NAME_FUEL_BASE@@
	 * This is the raw value we take from the fuel map or base fuel algorithm, before the corrections
	 * units: mg
	 * offset 16
	 */
	scaled_channel<uint16_t, 100, 1> baseFuel = (uint16_t)0;
	/**
	 * @@GAUGE_NAME_FUEL_RUNNING@@
	 * Total fuel with CLT IAT and TPS acceleration without injector lag corrections per cycle, as pulse per cycle
	 * units: mg
	 * offset 18
	 */
	scaled_channel<uint16_t, 100, 1> fuel = (uint16_t)0;
};
static_assert(sizeof(running_fuel_s) == 20);

// start of fuel_computer_s
struct fuel_computer_s {
	/**
	 * Fuel: Total correction
	 * units: mult
	 * offset 0
	 */
	float totalFuelCorrection = (float)0;
	/**
	 * Fuel: VVT Intake correction
	 * units: %
	 * offset 4
	 */
	float vvtFuelIntakeCorrection = (float)0;
	/**
	 * Fuel: VVT Exhaust correction
	 * units: %
	 * offset 8
	 */
	float vvtFuelExhaustCorrection = (float)0;
	/**
	 * offset 12
	 */
	running_fuel_s running;
	/**
	 * units: %
	 * offset 32
	 */
	scaled_channel<uint16_t, 100, 1> afrTableYAxis = (uint16_t)0;
	/**
	 * @@GAUGE_NAME_TARGET_LAMBDA@@
	 * offset 34
	 */
	scaled_channel<uint16_t, 10000, 1> targetLambda = (uint16_t)0;
	/**
	 * @@GAUGE_NAME_TARGET_AFR@@
	 * units: ratio
	 * offset 36
	 */
	scaled_channel<uint16_t, 1000, 1> targetAFR = (uint16_t)0;
	/**
	 * Fuel: Stoich ratio
	 * units: ratio
	 * offset 38
	 */
	scaled_channel<uint16_t, 1000, 1> stoichiometricRatio = (uint16_t)0;
	/**
	 * offset 40
	 */
	float sdTcharge_coff = (float)0;
	/**
	 * @@GAUGE_NAME_AIR_MASS@@
	 * units: g
	 * offset 44
	 */
	float sdAirMassInOneCylinder = (float)0;
	/**
	 * Air: Normalized cyl filling
	 * units: %
	 * offset 48
	 */
	float normalizedCylinderFilling = (float)0;
	/**
	 * offset 52
	 */
	uint16_t idealEngineTorque = (uint16_t)0;
	/**
	 * offset 54
	 */
	uint8_t brokenInjector = (uint8_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 55
	 */
	uint8_t alignmentFill_at_55[1] = {};
	/**
	offset 56 bit 0 */
	bool injectorHwIssue : 1 {};
	/**
	offset 56 bit 1 */
	bool unusedBit_15_1 : 1 {};
	/**
	offset 56 bit 2 */
	bool unusedBit_15_2 : 1 {};
	/**
	offset 56 bit 3 */
	bool unusedBit_15_3 : 1 {};
	/**
	offset 56 bit 4 */
	bool unusedBit_15_4 : 1 {};
	/**
	offset 56 bit 5 */
	bool unusedBit_15_5 : 1 {};
	/**
	offset 56 bit 6 */
	bool unusedBit_15_6 : 1 {};
	/**
	offset 56 bit 7 */
	bool unusedBit_15_7 : 1 {};
	/**
	offset 56 bit 8 */
	bool unusedBit_15_8 : 1 {};
	/**
	offset 56 bit 9 */
	bool unusedBit_15_9 : 1 {};
	/**
	offset 56 bit 10 */
	bool unusedBit_15_10 : 1 {};
	/**
	offset 56 bit 11 */
	bool unusedBit_15_11 : 1 {};
	/**
	offset 56 bit 12 */
	bool unusedBit_15_12 : 1 {};
	/**
	offset 56 bit 13 */
	bool unusedBit_15_13 : 1 {};
	/**
	offset 56 bit 14 */
	bool unusedBit_15_14 : 1 {};
	/**
	offset 56 bit 15 */
	bool unusedBit_15_15 : 1 {};
	/**
	offset 56 bit 16 */
	bool unusedBit_15_16 : 1 {};
	/**
	offset 56 bit 17 */
	bool unusedBit_15_17 : 1 {};
	/**
	offset 56 bit 18 */
	bool unusedBit_15_18 : 1 {};
	/**
	offset 56 bit 19 */
	bool unusedBit_15_19 : 1 {};
	/**
	offset 56 bit 20 */
	bool unusedBit_15_20 : 1 {};
	/**
	offset 56 bit 21 */
	bool unusedBit_15_21 : 1 {};
	/**
	offset 56 bit 22 */
	bool unusedBit_15_22 : 1 {};
	/**
	offset 56 bit 23 */
	bool unusedBit_15_23 : 1 {};
	/**
	offset 56 bit 24 */
	bool unusedBit_15_24 : 1 {};
	/**
	offset 56 bit 25 */
	bool unusedBit_15_25 : 1 {};
	/**
	offset 56 bit 26 */
	bool unusedBit_15_26 : 1 {};
	/**
	offset 56 bit 27 */
	bool unusedBit_15_27 : 1 {};
	/**
	offset 56 bit 28 */
	bool unusedBit_15_28 : 1 {};
	/**
	offset 56 bit 29 */
	bool unusedBit_15_29 : 1 {};
	/**
	offset 56 bit 30 */
	bool unusedBit_15_30 : 1 {};
	/**
	offset 56 bit 31 */
	bool unusedBit_15_31 : 1 {};
};
static_assert(sizeof(fuel_computer_s) == 60);

// end
// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/algo/fuel/fuel_computer.txt
