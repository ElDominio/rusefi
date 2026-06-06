// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/actuators/idle_state.txt
// by class com.rusefi.output.CHeaderConsumer
// begin
#pragma once
#include "rusefi_types.h"
// start of idle_state_s
struct idle_state_s {
	/**
	 * offset 0
	 */
	idle_state_e idleState = (idle_state_e)0;
	/**
	 * "idle: base value
	 * current position without adjustments (afterCrankingIACtaperDuration)"
	 * offset 4
	 */
	percent_t baseIdlePosition = (percent_t)0;
	/**
	 * idle: mightResetPid
	 * The idea of 'mightResetPid' is to reset PID only once - each time when TPS > idlePidDeactivationTpsThreshold.
	 * The throttle pedal can be pressed for a long time, making the PID data obsolete (thus the reset is required).
	 * We set 'mightResetPid' to true only if PID was actually used (i.e. idlePid.getOutput() was called) to save some CPU resources.
	 * See automaticIdleController().
	offset 8 bit 0 */
	bool mightResetPid : 1 {};
	/**
	 * Idle: shouldResetPid
	 * This is used when the PID configuration is changed, to guarantee the reset
	offset 8 bit 1 */
	bool shouldResetPid : 1 {};
	/**
	 * Idle: wasResetPid
	 * This is needed to slowly turn on the PID back after it was reset.
	offset 8 bit 2 */
	bool wasResetPid : 1 {};
	/**
	 * Idle: cranking
	offset 8 bit 3 */
	bool isCranking : 1 {};
	/**
	offset 8 bit 4 */
	bool isIacTableForCoasting : 1 {};
	/**
	 * Idle: reset
	offset 8 bit 5 */
	bool needReset : 1 {};
	/**
	 * Idle: dead zone
	offset 8 bit 6 */
	bool isInDeadZone : 1 {};
	/**
	offset 8 bit 7 */
	bool isBlipping : 1 {};
	/**
	offset 8 bit 8 */
	bool badTps : 1 {};
	/**
	offset 8 bit 9 */
	bool looksLikeRunning : 1 {};
	/**
	offset 8 bit 10 */
	bool looksLikeCoasting : 1 {};
	/**
	offset 8 bit 11 */
	bool looksLikeCrankToIdle : 1 {};
	/**
	 * Idle: coasting
	offset 8 bit 12 */
	bool isIdleCoasting : 1 {};
	/**
	 * Idle: Closed loop active
	offset 8 bit 13 */
	bool isIdleClosedLoop : 1 {};
	/**
	 * Idle: idling
	offset 8 bit 14 */
	bool isIdling : 1 {};
	/**
	offset 8 bit 15 */
	bool unusedBit_17_15 : 1 {};
	/**
	offset 8 bit 16 */
	bool unusedBit_17_16 : 1 {};
	/**
	offset 8 bit 17 */
	bool unusedBit_17_17 : 1 {};
	/**
	offset 8 bit 18 */
	bool unusedBit_17_18 : 1 {};
	/**
	offset 8 bit 19 */
	bool unusedBit_17_19 : 1 {};
	/**
	offset 8 bit 20 */
	bool unusedBit_17_20 : 1 {};
	/**
	offset 8 bit 21 */
	bool unusedBit_17_21 : 1 {};
	/**
	offset 8 bit 22 */
	bool unusedBit_17_22 : 1 {};
	/**
	offset 8 bit 23 */
	bool unusedBit_17_23 : 1 {};
	/**
	offset 8 bit 24 */
	bool unusedBit_17_24 : 1 {};
	/**
	offset 8 bit 25 */
	bool unusedBit_17_25 : 1 {};
	/**
	offset 8 bit 26 */
	bool unusedBit_17_26 : 1 {};
	/**
	offset 8 bit 27 */
	bool unusedBit_17_27 : 1 {};
	/**
	offset 8 bit 28 */
	bool unusedBit_17_28 : 1 {};
	/**
	offset 8 bit 29 */
	bool unusedBit_17_29 : 1 {};
	/**
	offset 8 bit 30 */
	bool unusedBit_17_30 : 1 {};
	/**
	offset 8 bit 31 */
	bool unusedBit_17_31 : 1 {};
	/**
	 * Idle: Target RPM
	 * offset 12
	 */
	uint16_t idleTarget = (uint16_t)0;
	/**
	 * Idle: Entry threshold
	 * offset 14
	 */
	uint16_t idleEntryRpm = (uint16_t)0;
	/**
	 * Idle: Exit threshold
	 * offset 16
	 */
	uint16_t idleExitRpm = (uint16_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 18
	 */
	uint8_t alignmentFill_at_18[2] = {};
	/**
	 * Idle: Target RPM base
	 * offset 20
	 */
	int targetRpmByClt = (int)0;
	/**
	 * Idle: Target A/C RPM
	 * offset 24
	 */
	int targetRpmAc = (int)0;
	/**
	 * idle: iacByRpmTaper portion
	 * offset 28
	 */
	percent_t iacByRpmTaper = (percent_t)0;
	/**
	 * idle: Lua Adder
	 * offset 32
	 */
	percent_t luaAdd = (percent_t)0;
	/**
	 * offset 36
	 */
	int m_lastTargetRpm = (int)0;
	/**
	 * Closed loop
	 * offset 40
	 */
	percent_t idleClosedLoop = (percent_t)0;
	/**
	 * @@GAUGE_NAME_IAC@@
	 * units: %
	 * offset 44
	 */
	percent_t currentIdlePosition = (percent_t)0;
	/**
	 * Target airmass
	 * units: mg
	 * offset 48
	 */
	uint16_t idleTargetAirmass = (uint16_t)0;
	/**
	 * Target airflow
	 * units: kg/h
	 * offset 50
	 */
	scaled_channel<uint16_t, 100, 1> idleTargetFlow = (uint16_t)0;
	/**
	 * Idle: off-idle adder active
	offset 52 bit 0 */
	bool offIdleAdderActive : 1 {};
	/**
	offset 52 bit 1 */
	bool unusedBit_48_1 : 1 {};
	/**
	offset 52 bit 2 */
	bool unusedBit_48_2 : 1 {};
	/**
	offset 52 bit 3 */
	bool unusedBit_48_3 : 1 {};
	/**
	offset 52 bit 4 */
	bool unusedBit_48_4 : 1 {};
	/**
	offset 52 bit 5 */
	bool unusedBit_48_5 : 1 {};
	/**
	offset 52 bit 6 */
	bool unusedBit_48_6 : 1 {};
	/**
	offset 52 bit 7 */
	bool unusedBit_48_7 : 1 {};
	/**
	offset 52 bit 8 */
	bool unusedBit_48_8 : 1 {};
	/**
	offset 52 bit 9 */
	bool unusedBit_48_9 : 1 {};
	/**
	offset 52 bit 10 */
	bool unusedBit_48_10 : 1 {};
	/**
	offset 52 bit 11 */
	bool unusedBit_48_11 : 1 {};
	/**
	offset 52 bit 12 */
	bool unusedBit_48_12 : 1 {};
	/**
	offset 52 bit 13 */
	bool unusedBit_48_13 : 1 {};
	/**
	offset 52 bit 14 */
	bool unusedBit_48_14 : 1 {};
	/**
	offset 52 bit 15 */
	bool unusedBit_48_15 : 1 {};
	/**
	offset 52 bit 16 */
	bool unusedBit_48_16 : 1 {};
	/**
	offset 52 bit 17 */
	bool unusedBit_48_17 : 1 {};
	/**
	offset 52 bit 18 */
	bool unusedBit_48_18 : 1 {};
	/**
	offset 52 bit 19 */
	bool unusedBit_48_19 : 1 {};
	/**
	offset 52 bit 20 */
	bool unusedBit_48_20 : 1 {};
	/**
	offset 52 bit 21 */
	bool unusedBit_48_21 : 1 {};
	/**
	offset 52 bit 22 */
	bool unusedBit_48_22 : 1 {};
	/**
	offset 52 bit 23 */
	bool unusedBit_48_23 : 1 {};
	/**
	offset 52 bit 24 */
	bool unusedBit_48_24 : 1 {};
	/**
	offset 52 bit 25 */
	bool unusedBit_48_25 : 1 {};
	/**
	offset 52 bit 26 */
	bool unusedBit_48_26 : 1 {};
	/**
	offset 52 bit 27 */
	bool unusedBit_48_27 : 1 {};
	/**
	offset 52 bit 28 */
	bool unusedBit_48_28 : 1 {};
	/**
	offset 52 bit 29 */
	bool unusedBit_48_29 : 1 {};
	/**
	offset 52 bit 30 */
	bool unusedBit_48_30 : 1 {};
	/**
	offset 52 bit 31 */
	bool unusedBit_48_31 : 1 {};
	/**
	 * Idle: off-idle adder RPM
	 * units: RPM
	 * offset 56
	 */
	int16_t offIdleAdderCurrentRpm = (int16_t)0;
	/**
	 * Idle: off-idle adder state
	 * offset 58
	 */
	uint8_t offIdleAdderStateIndex = (uint8_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 59
	 */
	uint8_t alignmentFill_at_59[1] = {};
};
static_assert(sizeof(idle_state_s) == 60);

// end
// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/actuators/idle_state.txt
