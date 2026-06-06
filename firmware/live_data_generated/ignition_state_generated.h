// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/algo/ignition/ignition_state.txt
// by class com.rusefi.output.CHeaderConsumer
// begin
#pragma once
#include "rusefi_types.h"
// start of ignition_state_s
struct ignition_state_s {
	/**
	 * "Ignition: base dwell"
	 * units: ms
	 * offset 0
	 */
	float baseDwell = (float)0;
	/**
	 * @@GAUGE_COIL_DWELL_TIME@@
	 * units: ms
	 * offset 4
	 */
	floatms_t sparkDwell = (floatms_t)0;
	/**
	 * Ignition: dwell duration
	 * as crankshaft angle
	 * NAN if engine is stopped
	 * See also sparkDwell
	 * units: deg
	 * offset 8
	 */
	angle_t dwellDurationAngle = (angle_t)0;
	/**
	 * Ign: CLT correction
	 * units: deg
	 * offset 12
	 */
	scaled_channel<int16_t, 100, 1> cltTimingCorrection = (int16_t)0;
	/**
	 * Ign: IAT correction
	 * units: deg
	 * offset 14
	 */
	scaled_channel<int16_t, 100, 1> timingIatCorrection = (int16_t)0;
	/**
	 * Idle: Timing adjustment
	 * units: deg
	 * offset 16
	 */
	scaled_channel<int16_t, 100, 1> timingPidCorrection = (int16_t)0;
	/**
	 * Ign: VVT Intake correction
	 * units: deg
	 * offset 18
	 */
	scaled_channel<int16_t, 100, 1> vvtIntakeTimingCorrection = (int16_t)0;
	/**
	 * Ign: VVT Exhaust correction
	 * units: deg
	 * offset 20
	 */
	scaled_channel<int16_t, 100, 1> vvtExhaustTimingCorrection = (int16_t)0;
	/**
	 * DFCO: Timing retard
	 * units: deg
	 * offset 22
	 */
	scaled_channel<int16_t, 100, 1> dfcoTimingRetard = (int16_t)0;
	/**
	 * @@GAUGE_NAME_TIMING_ADVANCE@@
	 * units: deg
	 * offset 24
	 */
	scaled_channel<int16_t, 50, 1> baseIgnitionAdvance = (int16_t)0;
	/**
	 * @@GAUGE_NAME_ADJUSTED_TIMING@@
	 * units: deg
	 * offset 26
	 */
	scaled_channel<int16_t, 50, 1> correctedIgnitionAdvance = (int16_t)0;
	/**
	 * Traction: timing correction
	 * units: deg
	 * offset 28
	 */
	scaled_channel<int16_t, 50, 1> tractionAdvanceDrop = (int16_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 30
	 */
	uint8_t alignmentFill_at_30[2] = {};
	/**
	 * Ign: Dwell voltage correction
	 * offset 32
	 */
	float dwellVoltageCorrection = (float)0;
	/**
	 * Ign: Lua timing add
	 * units: deg
	 * offset 36
	 */
	float luaTimingAdd = (float)0;
	/**
	 * Ign: Lua timing mult
	 * units: deg
	 * offset 40
	 */
	float luaTimingMult = (float)0;
	/**
	 * Ign: Lua Spark Skip
	offset 44 bit 0 */
	bool luaIgnitionSkip : 1 {};
	/**
	offset 44 bit 1 */
	bool unusedBit_17_1 : 1 {};
	/**
	offset 44 bit 2 */
	bool unusedBit_17_2 : 1 {};
	/**
	offset 44 bit 3 */
	bool unusedBit_17_3 : 1 {};
	/**
	offset 44 bit 4 */
	bool unusedBit_17_4 : 1 {};
	/**
	offset 44 bit 5 */
	bool unusedBit_17_5 : 1 {};
	/**
	offset 44 bit 6 */
	bool unusedBit_17_6 : 1 {};
	/**
	offset 44 bit 7 */
	bool unusedBit_17_7 : 1 {};
	/**
	offset 44 bit 8 */
	bool unusedBit_17_8 : 1 {};
	/**
	offset 44 bit 9 */
	bool unusedBit_17_9 : 1 {};
	/**
	offset 44 bit 10 */
	bool unusedBit_17_10 : 1 {};
	/**
	offset 44 bit 11 */
	bool unusedBit_17_11 : 1 {};
	/**
	offset 44 bit 12 */
	bool unusedBit_17_12 : 1 {};
	/**
	offset 44 bit 13 */
	bool unusedBit_17_13 : 1 {};
	/**
	offset 44 bit 14 */
	bool unusedBit_17_14 : 1 {};
	/**
	offset 44 bit 15 */
	bool unusedBit_17_15 : 1 {};
	/**
	offset 44 bit 16 */
	bool unusedBit_17_16 : 1 {};
	/**
	offset 44 bit 17 */
	bool unusedBit_17_17 : 1 {};
	/**
	offset 44 bit 18 */
	bool unusedBit_17_18 : 1 {};
	/**
	offset 44 bit 19 */
	bool unusedBit_17_19 : 1 {};
	/**
	offset 44 bit 20 */
	bool unusedBit_17_20 : 1 {};
	/**
	offset 44 bit 21 */
	bool unusedBit_17_21 : 1 {};
	/**
	offset 44 bit 22 */
	bool unusedBit_17_22 : 1 {};
	/**
	offset 44 bit 23 */
	bool unusedBit_17_23 : 1 {};
	/**
	offset 44 bit 24 */
	bool unusedBit_17_24 : 1 {};
	/**
	offset 44 bit 25 */
	bool unusedBit_17_25 : 1 {};
	/**
	offset 44 bit 26 */
	bool unusedBit_17_26 : 1 {};
	/**
	offset 44 bit 27 */
	bool unusedBit_17_27 : 1 {};
	/**
	offset 44 bit 28 */
	bool unusedBit_17_28 : 1 {};
	/**
	offset 44 bit 29 */
	bool unusedBit_17_29 : 1 {};
	/**
	offset 44 bit 30 */
	bool unusedBit_17_30 : 1 {};
	/**
	offset 44 bit 31 */
	bool unusedBit_17_31 : 1 {};
	/**
	 * Ign: Trailing spark deg
	 * units: deg
	 * offset 48
	 */
	scaled_channel<int16_t, 100, 1> trailingSparkAngle = (int16_t)0;
	/**
	 * offset 50
	 */
	int16_t rpmForIgnitionTableDot = (int16_t)0;
	/**
	 * offset 52
	 */
	int16_t rpmForIgnitionIdleTableDot = (int16_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 54
	 */
	uint8_t alignmentFill_at_54[2] = {};
	/**
	 * offset 56
	 */
	float loadForIgnitionTableDot = (float)0;
};
static_assert(sizeof(ignition_state_s) == 60);

// end
// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/algo/ignition/ignition_state.txt
