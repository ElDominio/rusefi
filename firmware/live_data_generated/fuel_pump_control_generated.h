// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/modules/fuel_pump/fuel_pump_control.txt
// by class com.rusefi.output.CHeaderConsumer
// begin
#pragma once
#include "rusefi_types.h"
// start of fuel_pump_control_s
struct fuel_pump_control_s {
	/**
	offset 0 bit 0 */
	bool isPrime : 1 {};
	/**
	offset 0 bit 1 */
	bool engineTurnedRecently : 1 {};
	/**
	offset 0 bit 2 */
	bool isFuelPumpOn : 1 {};
	/**
	offset 0 bit 3 */
	bool ignitionOn : 1 {};
	/**
	offset 0 bit 4 */
	bool isSecondaryPumpOn : 1 {};
	/**
	offset 0 bit 5 */
	bool isFpPidActive : 1 {};
	/**
	offset 0 bit 6 */
	bool unusedBit_6_6 : 1 {};
	/**
	offset 0 bit 7 */
	bool unusedBit_6_7 : 1 {};
	/**
	offset 0 bit 8 */
	bool unusedBit_6_8 : 1 {};
	/**
	offset 0 bit 9 */
	bool unusedBit_6_9 : 1 {};
	/**
	offset 0 bit 10 */
	bool unusedBit_6_10 : 1 {};
	/**
	offset 0 bit 11 */
	bool unusedBit_6_11 : 1 {};
	/**
	offset 0 bit 12 */
	bool unusedBit_6_12 : 1 {};
	/**
	offset 0 bit 13 */
	bool unusedBit_6_13 : 1 {};
	/**
	offset 0 bit 14 */
	bool unusedBit_6_14 : 1 {};
	/**
	offset 0 bit 15 */
	bool unusedBit_6_15 : 1 {};
	/**
	offset 0 bit 16 */
	bool unusedBit_6_16 : 1 {};
	/**
	offset 0 bit 17 */
	bool unusedBit_6_17 : 1 {};
	/**
	offset 0 bit 18 */
	bool unusedBit_6_18 : 1 {};
	/**
	offset 0 bit 19 */
	bool unusedBit_6_19 : 1 {};
	/**
	offset 0 bit 20 */
	bool unusedBit_6_20 : 1 {};
	/**
	offset 0 bit 21 */
	bool unusedBit_6_21 : 1 {};
	/**
	offset 0 bit 22 */
	bool unusedBit_6_22 : 1 {};
	/**
	offset 0 bit 23 */
	bool unusedBit_6_23 : 1 {};
	/**
	offset 0 bit 24 */
	bool unusedBit_6_24 : 1 {};
	/**
	offset 0 bit 25 */
	bool unusedBit_6_25 : 1 {};
	/**
	offset 0 bit 26 */
	bool unusedBit_6_26 : 1 {};
	/**
	offset 0 bit 27 */
	bool unusedBit_6_27 : 1 {};
	/**
	offset 0 bit 28 */
	bool unusedBit_6_28 : 1 {};
	/**
	offset 0 bit 29 */
	bool unusedBit_6_29 : 1 {};
	/**
	offset 0 bit 30 */
	bool unusedBit_6_30 : 1 {};
	/**
	offset 0 bit 31 */
	bool unusedBit_6_31 : 1 {};
	/**
	 * FP duty % (PWM mode)
	 * units: %
	 * offset 4
	 */
	uint8_t fuelPumpDuty = (uint8_t)0;
	/**
	 * FP target kPa (PWM mode)
	 * units: kPa
	 * offset 5
	 */
	scaled_channel<uint8_t, 1, 5> fuelPressureTarget = (uint8_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 6
	 */
	uint8_t alignmentFill_at_6[2] = {};
};
static_assert(sizeof(fuel_pump_control_s) == 8);

// end
// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/modules/fuel_pump/fuel_pump_control.txt
