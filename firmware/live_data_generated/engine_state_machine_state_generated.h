// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/algo/engine_state_machine_state.txt
// by class com.rusefi.output.CHeaderConsumer
// begin
#pragma once
#include "rusefi_types.h"
// start of engine_state_machine_state_s
struct engine_state_machine_state_s {
	/**
	 * Engine SM: enabled
	offset 0 bit 0 */
	bool engineSmEnabled : 1 {};
	/**
	 * Engine SM: Off
	offset 0 bit 1 */
	bool engineSmIsOff : 1 {};
	/**
	 * Engine SM: Cranking
	offset 0 bit 2 */
	bool engineSmIsCranking : 1 {};
	/**
	 * Engine SM: Afterstart
	offset 0 bit 3 */
	bool engineSmIsAfterStart : 1 {};
	/**
	 * Engine SM: Idle
	offset 0 bit 4 */
	bool engineSmIsIdle : 1 {};
	/**
	 * Engine SM: Coasting
	offset 0 bit 5 */
	bool engineSmIsCoasting : 1 {};
	/**
	 * Engine SM: Transient
	offset 0 bit 6 */
	bool engineSmIsTransient : 1 {};
	/**
	 * Engine SM: WOT
	offset 0 bit 7 */
	bool engineSmIsWot : 1 {};
	/**
	 * Engine SM: Cruising
	offset 0 bit 8 */
	bool engineSmIsCruising : 1 {};
	/**
	 * Engine SM: Overrun
	offset 0 bit 9 */
	bool engineSmIsOverrun : 1 {};
	/**
	 * Engine SM: Launch Control
	offset 0 bit 10 */
	bool engineSmIsLaunchControl : 1 {};
	/**
	 * Engine SM: Torque Reduction
	offset 0 bit 11 */
	bool engineSmIsTorqueReduction : 1 {};
	/**
	 * Engine SM: Upshifting
	offset 0 bit 12 */
	bool engineSmIsUpshifting : 1 {};
	/**
	 * Engine SM: Downshifting
	offset 0 bit 13 */
	bool engineSmIsDownshifting : 1 {};
	/**
	 * Engine SM: Limp/Cut Active
	offset 0 bit 14 */
	bool engineSmIsLimp : 1 {};
	/**
	offset 0 bit 15 */
	bool unusedBit_15_15 : 1 {};
	/**
	offset 0 bit 16 */
	bool unusedBit_15_16 : 1 {};
	/**
	offset 0 bit 17 */
	bool unusedBit_15_17 : 1 {};
	/**
	offset 0 bit 18 */
	bool unusedBit_15_18 : 1 {};
	/**
	offset 0 bit 19 */
	bool unusedBit_15_19 : 1 {};
	/**
	offset 0 bit 20 */
	bool unusedBit_15_20 : 1 {};
	/**
	offset 0 bit 21 */
	bool unusedBit_15_21 : 1 {};
	/**
	offset 0 bit 22 */
	bool unusedBit_15_22 : 1 {};
	/**
	offset 0 bit 23 */
	bool unusedBit_15_23 : 1 {};
	/**
	offset 0 bit 24 */
	bool unusedBit_15_24 : 1 {};
	/**
	offset 0 bit 25 */
	bool unusedBit_15_25 : 1 {};
	/**
	offset 0 bit 26 */
	bool unusedBit_15_26 : 1 {};
	/**
	offset 0 bit 27 */
	bool unusedBit_15_27 : 1 {};
	/**
	offset 0 bit 28 */
	bool unusedBit_15_28 : 1 {};
	/**
	offset 0 bit 29 */
	bool unusedBit_15_29 : 1 {};
	/**
	offset 0 bit 30 */
	bool unusedBit_15_30 : 1 {};
	/**
	offset 0 bit 31 */
	bool unusedBit_15_31 : 1 {};
	/**
	 * Engine SM: primary state
	 * offset 4
	 */
	uint8_t engineSmCurrentState = (uint8_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 5
	 */
	uint8_t alignmentFill_at_5[3] = {};
};
static_assert(sizeof(engine_state_machine_state_s) == 8);

// end
// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/algo/engine_state_machine_state.txt
