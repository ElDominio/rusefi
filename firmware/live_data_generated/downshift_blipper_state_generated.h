// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/actuators/downshift_blipper_state.txt
// by class com.rusefi.output.CHeaderConsumer
// begin
#pragma once
#include "rusefi_types.h"
// start of downshift_blipper_state_s
struct downshift_blipper_state_s {
	/**
	 * Blipper: active
	offset 0 bit 0 */
	bool downshiftBlipActive : 1 {};
	/**
	offset 0 bit 1 */
	bool unusedBit_1_1 : 1 {};
	/**
	offset 0 bit 2 */
	bool unusedBit_1_2 : 1 {};
	/**
	offset 0 bit 3 */
	bool unusedBit_1_3 : 1 {};
	/**
	offset 0 bit 4 */
	bool unusedBit_1_4 : 1 {};
	/**
	offset 0 bit 5 */
	bool unusedBit_1_5 : 1 {};
	/**
	offset 0 bit 6 */
	bool unusedBit_1_6 : 1 {};
	/**
	offset 0 bit 7 */
	bool unusedBit_1_7 : 1 {};
	/**
	offset 0 bit 8 */
	bool unusedBit_1_8 : 1 {};
	/**
	offset 0 bit 9 */
	bool unusedBit_1_9 : 1 {};
	/**
	offset 0 bit 10 */
	bool unusedBit_1_10 : 1 {};
	/**
	offset 0 bit 11 */
	bool unusedBit_1_11 : 1 {};
	/**
	offset 0 bit 12 */
	bool unusedBit_1_12 : 1 {};
	/**
	offset 0 bit 13 */
	bool unusedBit_1_13 : 1 {};
	/**
	offset 0 bit 14 */
	bool unusedBit_1_14 : 1 {};
	/**
	offset 0 bit 15 */
	bool unusedBit_1_15 : 1 {};
	/**
	offset 0 bit 16 */
	bool unusedBit_1_16 : 1 {};
	/**
	offset 0 bit 17 */
	bool unusedBit_1_17 : 1 {};
	/**
	offset 0 bit 18 */
	bool unusedBit_1_18 : 1 {};
	/**
	offset 0 bit 19 */
	bool unusedBit_1_19 : 1 {};
	/**
	offset 0 bit 20 */
	bool unusedBit_1_20 : 1 {};
	/**
	offset 0 bit 21 */
	bool unusedBit_1_21 : 1 {};
	/**
	offset 0 bit 22 */
	bool unusedBit_1_22 : 1 {};
	/**
	offset 0 bit 23 */
	bool unusedBit_1_23 : 1 {};
	/**
	offset 0 bit 24 */
	bool unusedBit_1_24 : 1 {};
	/**
	offset 0 bit 25 */
	bool unusedBit_1_25 : 1 {};
	/**
	offset 0 bit 26 */
	bool unusedBit_1_26 : 1 {};
	/**
	offset 0 bit 27 */
	bool unusedBit_1_27 : 1 {};
	/**
	offset 0 bit 28 */
	bool unusedBit_1_28 : 1 {};
	/**
	offset 0 bit 29 */
	bool unusedBit_1_29 : 1 {};
	/**
	offset 0 bit 30 */
	bool unusedBit_1_30 : 1 {};
	/**
	offset 0 bit 31 */
	bool unusedBit_1_31 : 1 {};
	/**
	 * Blipper: state
	 * offset 4
	 */
	uint8_t downshiftBlipState = (uint8_t)0;
	/**
	 * Blipper: pre-shift gear
	 * offset 5
	 */
	uint8_t downshiftBlipPreShiftGear = (uint8_t)0;
	/**
	 * Blipper: target gear
	 * offset 6
	 */
	uint8_t downshiftBlipTargetGear = (uint8_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 7
	 */
	uint8_t alignmentFill_at_7[1] = {};
	/**
	 * Blipper: target RPM
	 * offset 8
	 */
	uint16_t downshiftBlipTargetRpm = (uint16_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 10
	 */
	uint8_t alignmentFill_at_10[2] = {};
	/**
	 * Blipper: throttle req
	 * offset 12
	 */
	float downshiftBlipThrottleRequest = (float)0;
	/**
	 * Blipper: PID output
	 * offset 16
	 */
	float downshiftBlipPidOutput = (float)0;
	/**
	 * Blipper: Lua mult
	 * offset 20
	 */
	float downshiftBlipLuaMult = (float)0;
};
static_assert(sizeof(downshift_blipper_state_s) == 24);

// end
// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/actuators/downshift_blipper_state.txt
