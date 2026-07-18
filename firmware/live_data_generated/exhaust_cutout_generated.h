// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/actuators/exhaust_cutout.txt
// by class com.rusefi.output.CHeaderConsumer
// begin
#pragma once
#include "rusefi_types.h"
// start of exhaust_cutout_s
struct exhaust_cutout_s {
	/**
	 * RPM trigger active
	offset 0 bit 0 */
	bool isTriggerRpm : 1 {};
	/**
	 * TPS trigger active, anti-blip OK
	offset 0 bit 1 */
	bool isTriggerTps : 1 {};
	/**
	 * MAP/boost trigger active
	offset 0 bit 2 */
	bool isTriggerMap : 1 {};
	/**
	 * Activation input in HIGH state
	offset 0 bit 3 */
	bool isInputHigh : 1 {};
	/**
	 * Control logic requests cutout open
	offset 0 bit 4 */
	bool targetOpen : 1 {};
	/**
	 * Cutout is fully open
	offset 0 bit 5 */
	bool isCutoutOpen : 1 {};
	/**
	 * H-Bridge motor is running
	offset 0 bit 6 */
	bool isCutoutMoving : 1 {};
	/**
	 * Actuator test sequence is running
	offset 0 bit 7 */
	bool isTestActive : 1 {};
	/**
	 * Forced open by Lua override
	offset 0 bit 8 */
	bool isLuaOverrideActive : 1 {};
	/**
	offset 0 bit 9 */
	bool unusedBit_9_9 : 1 {};
	/**
	offset 0 bit 10 */
	bool unusedBit_9_10 : 1 {};
	/**
	offset 0 bit 11 */
	bool unusedBit_9_11 : 1 {};
	/**
	offset 0 bit 12 */
	bool unusedBit_9_12 : 1 {};
	/**
	offset 0 bit 13 */
	bool unusedBit_9_13 : 1 {};
	/**
	offset 0 bit 14 */
	bool unusedBit_9_14 : 1 {};
	/**
	offset 0 bit 15 */
	bool unusedBit_9_15 : 1 {};
	/**
	offset 0 bit 16 */
	bool unusedBit_9_16 : 1 {};
	/**
	offset 0 bit 17 */
	bool unusedBit_9_17 : 1 {};
	/**
	offset 0 bit 18 */
	bool unusedBit_9_18 : 1 {};
	/**
	offset 0 bit 19 */
	bool unusedBit_9_19 : 1 {};
	/**
	offset 0 bit 20 */
	bool unusedBit_9_20 : 1 {};
	/**
	offset 0 bit 21 */
	bool unusedBit_9_21 : 1 {};
	/**
	offset 0 bit 22 */
	bool unusedBit_9_22 : 1 {};
	/**
	offset 0 bit 23 */
	bool unusedBit_9_23 : 1 {};
	/**
	offset 0 bit 24 */
	bool unusedBit_9_24 : 1 {};
	/**
	offset 0 bit 25 */
	bool unusedBit_9_25 : 1 {};
	/**
	offset 0 bit 26 */
	bool unusedBit_9_26 : 1 {};
	/**
	offset 0 bit 27 */
	bool unusedBit_9_27 : 1 {};
	/**
	offset 0 bit 28 */
	bool unusedBit_9_28 : 1 {};
	/**
	offset 0 bit 29 */
	bool unusedBit_9_29 : 1 {};
	/**
	offset 0 bit 30 */
	bool unusedBit_9_30 : 1 {};
	/**
	offset 0 bit 31 */
	bool unusedBit_9_31 : 1 {};
};
static_assert(sizeof(exhaust_cutout_s) == 4);

// end
// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/actuators/exhaust_cutout.txt
