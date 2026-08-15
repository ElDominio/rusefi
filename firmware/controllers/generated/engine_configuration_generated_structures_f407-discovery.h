// this section was generated automatically by rusEFI tool config_definition-all.jar based on (unknown script) integration/rusefi_config.txt
// by class com.rusefi.output.CHeaderConsumer
// begin
#pragma once
#include "rusefi_types.h"
// start of stft_cell_cfg_s
struct stft_cell_cfg_s {
	/**
	 * Maximum % that the short term fuel trim can add
	 * units: %
	 * offset 0
	 */
	scaled_channel<uint8_t, 10, 1> maxAdd;
	/**
	 * Maximum % that the short term fuel trim can remove
	 * units: %
	 * offset 1
	 */
	scaled_channel<uint8_t, 10, 1> maxRemove;
	/**
	 * Commonly referred as Integral gain.
	 * Time constant for correction while in this cell: this sets responsiveness of the closed loop correction. A value of 5.0 means it will try to make most of the correction within 5 seconds, and a value of 1.0 will try to correct within 1 second.
	 * Lower values makes the correction more sensitive, higher values slow the correction down.
	 * units: sec
	 * offset 2
	 */
	scaled_channel<uint16_t, 10, 1> timeConstant;
};
static_assert(sizeof(stft_cell_cfg_s) == 4);

// start of stft_s
struct stft_s {
	/**
	 * Below this RPM, the idle region is active, idle+300 would be a good value
	 * units: RPM
	 * offset 0
	 */
	scaled_channel<uint8_t, 1, 50> maxIdleRegionRpm;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1
	 */
	uint8_t alignmentFill_at_1[1] = {};
	/**
	 * Below this engine load, the overrun region is active
	 * When tuning by MAP the units are kPa/psi, e.g. 30 would mean 30kPa. When tuning TPS, 30 would be 30%
	 * units: load
	 * offset 2
	 */
	uint16_t maxOverrunLoad;
	/**
	 * Above this engine load, the power region is active
	 * When tuning by MAP the units are kPa/psi
	 * units: load
	 * offset 4
	 */
	uint16_t minPowerLoad;
	/**
	 * When close to correct AFR, pause correction. This can improve stability by not changing the adjustment if the error is extremely small, but is not required.
	 * units: %
	 * offset 6
	 */
	scaled_channel<uint8_t, 10, 1> deadband;
	/**
	 * Minimum coolant temperature before closed loop operation is allowed.
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 7
	 */
	int8_t minClt;
	/**
	 * Below this AFR, correction is paused
	 * units: afr
	 * offset 8
	 */
	scaled_channel<uint8_t, 10, 1> minAfr;
	/**
	 * Above this AFR, correction is paused
	 * units: afr
	 * offset 9
	 */
	scaled_channel<uint8_t, 10, 1> maxAfr;
	/**
	 * Time after startup before closed loop operation is allowed.
	 * units: seconds
	 * offset 10
	 */
	uint8_t startupDelay;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 11
	 */
	uint8_t alignmentFill_at_11[1] = {};
	/**
	 * offset 12
	 */
	stft_cell_cfg_s cellCfgs[STFT_CELL_COUNT] = {};
};
static_assert(sizeof(stft_s) == 28);

// start of ltft_s
struct ltft_s {
	/**
	 * Enables lambda sensor long term fuel corrections data gathering into LTFT trim tables
	offset 0 bit 0 */
	bool enabled : 1 {};
	/**
	 * Apply LTFT trims into fuel calculation on top of VE table.
	 * We do not adjust VE table automatically, please click 'Apply to VE' if you want to adjust your VE tables and reset trims.
	offset 0 bit 1 */
	bool correctionEnabled : 1 {};
	/**
	offset 0 bit 2 */
	bool unusedBit_2_2 : 1 {};
	/**
	offset 0 bit 3 */
	bool unusedBit_2_3 : 1 {};
	/**
	offset 0 bit 4 */
	bool unusedBit_2_4 : 1 {};
	/**
	offset 0 bit 5 */
	bool unusedBit_2_5 : 1 {};
	/**
	offset 0 bit 6 */
	bool unusedBit_2_6 : 1 {};
	/**
	offset 0 bit 7 */
	bool unusedBit_2_7 : 1 {};
	/**
	offset 0 bit 8 */
	bool unusedBit_2_8 : 1 {};
	/**
	offset 0 bit 9 */
	bool unusedBit_2_9 : 1 {};
	/**
	offset 0 bit 10 */
	bool unusedBit_2_10 : 1 {};
	/**
	offset 0 bit 11 */
	bool unusedBit_2_11 : 1 {};
	/**
	offset 0 bit 12 */
	bool unusedBit_2_12 : 1 {};
	/**
	offset 0 bit 13 */
	bool unusedBit_2_13 : 1 {};
	/**
	offset 0 bit 14 */
	bool unusedBit_2_14 : 1 {};
	/**
	offset 0 bit 15 */
	bool unusedBit_2_15 : 1 {};
	/**
	offset 0 bit 16 */
	bool unusedBit_2_16 : 1 {};
	/**
	offset 0 bit 17 */
	bool unusedBit_2_17 : 1 {};
	/**
	offset 0 bit 18 */
	bool unusedBit_2_18 : 1 {};
	/**
	offset 0 bit 19 */
	bool unusedBit_2_19 : 1 {};
	/**
	offset 0 bit 20 */
	bool unusedBit_2_20 : 1 {};
	/**
	offset 0 bit 21 */
	bool unusedBit_2_21 : 1 {};
	/**
	offset 0 bit 22 */
	bool unusedBit_2_22 : 1 {};
	/**
	offset 0 bit 23 */
	bool unusedBit_2_23 : 1 {};
	/**
	offset 0 bit 24 */
	bool unusedBit_2_24 : 1 {};
	/**
	offset 0 bit 25 */
	bool unusedBit_2_25 : 1 {};
	/**
	offset 0 bit 26 */
	bool unusedBit_2_26 : 1 {};
	/**
	offset 0 bit 27 */
	bool unusedBit_2_27 : 1 {};
	/**
	offset 0 bit 28 */
	bool unusedBit_2_28 : 1 {};
	/**
	offset 0 bit 29 */
	bool unusedBit_2_29 : 1 {};
	/**
	offset 0 bit 30 */
	bool unusedBit_2_30 : 1 {};
	/**
	offset 0 bit 31 */
	bool unusedBit_2_31 : 1 {};
	/**
	 * When close to correct AFR, pause correction. This can improve stability by not changing the adjustment if the error is extremely small, but is not required.
	 * units: %
	 * offset 4
	 */
	scaled_channel<uint8_t, 10, 1> deadband;
	/**
	 * Maximum % that the long term fuel trim can add
	 * units: %
	 * offset 5
	 */
	scaled_channel<uint8_t, 10, 1> maxAdd;
	/**
	 * Maximum % that the long term fuel trim can remove
	 * units: %
	 * offset 6
	 */
	scaled_channel<uint8_t, 10, 1> maxRemove;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 7
	 */
	uint8_t alignmentFill_at_7[1] = {};
	/**
	 * Commonly referred as Integral gain.
	 * Time constant for correction while in this cell: this sets responsiveness of the closed loop correction. A value of 30.0 means it will try to make most of the correction within 30 seconds, and a value of 300.0 will try to correct within 5 minutes.
	 * Lower values makes the correction more sensitive, higher values slow the correction down.
	 * units: sec
	 * offset 8
	 */
	scaled_channel<uint16_t, 10, 1> timeConstant[STFT_CELL_COUNT] = {};
};
static_assert(sizeof(ltft_s) == 16);

// start of pid_s
struct pid_s {
	/**
	 * offset 0
	 */
	float pFactor;
	/**
	 * offset 4
	 */
	float iFactor;
	/**
	 * offset 8
	 */
	float dFactor;
	/**
	 * Linear addition to PID logic
	 * Also known as feedforward.
	 * offset 12
	 */
	int16_t offset;
	/**
	 * PID dTime
	 * units: ms
	 * offset 14
	 */
	int16_t periodMs;
	/**
	 * Output Min Duty Cycle
	 * offset 16
	 */
	int16_t minValue;
	/**
	 * Output Max Duty Cycle
	 * offset 18
	 */
	int16_t maxValue;
};
static_assert(sizeof(pid_s) == 20);

// start of MsIoBox_config_s
struct MsIoBox_config_s {
	/**
	 * offset 0
	 */
	MsIoBoxId id;
	/**
	 * offset 1
	 */
	MsIoBoxVss vss;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2
	 */
	uint8_t alignmentFill_at_2[2] = {};
};
static_assert(sizeof(MsIoBox_config_s) == 4);

// start of cranking_parameters_s
struct cranking_parameters_s {
	/**
	 * This sets the RPM limit below which the ECU will use cranking fuel and ignition logic, typically this is around 350-450rpm. 
	 * set cranking_rpm X
	 * units: RPM
	 * offset 0
	 */
	int16_t rpm;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2
	 */
	uint8_t alignmentFill_at_2[2] = {};
};
static_assert(sizeof(cranking_parameters_s) == 4);

// start of gppwm_channel
struct gppwm_channel {
	/**
	 * Select a pin to use for PWM or on-off output.
	 * offset 0
	 */
	output_pin_e pin;
	/**
	 * If an error (with a sensor, etc) is detected, this value is used instead of reading from the table.
	 * This should be a safe value for whatever hardware is connected to prevent damage.
	 * units: %
	 * offset 2
	 */
	uint8_t dutyIfError;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3
	 */
	uint8_t alignmentFill_at_3[1] = {};
	/**
	 * Select a frequency to run PWM at.
	 * Set this to 0hz to enable on-off mode.
	 * units: hz
	 * offset 4
	 */
	uint16_t pwmFrequency;
	/**
	 * Hysteresis: in on-off mode, turn the output on when the table value is above this duty.
	 * units: %
	 * offset 6
	 */
	uint8_t onAboveDuty;
	/**
	 * Hysteresis: in on-off mode, turn the output off when the table value is below this duty.
	 * units: %
	 * offset 7
	 */
	uint8_t offBelowDuty;
	/**
	 * Selects the Y axis to use for the table.
	 * offset 8
	 */
	gppwm_channel_e loadAxis;
	/**
	 * Selects the X axis to use for the table.
	 * offset 9
	 */
	gppwm_channel_e rpmAxis;
	/**
	 * offset 10
	 */
	scaled_channel<int16_t, 2, 1> loadBins[GPPWM_LOAD_COUNT] = {};
	/**
	 * offset 26
	 */
	int16_t rpmBins[GPPWM_RPM_COUNT] = {};
	/**
	 * units: duty
	 * offset 42
	 */
	scaled_channel<uint8_t, 2, 1> table[GPPWM_LOAD_COUNT][GPPWM_RPM_COUNT] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 106
	 */
	uint8_t alignmentFill_at_106[2] = {};
};
static_assert(sizeof(gppwm_channel) == 108);

// start of air_pressure_sensor_config_s
struct air_pressure_sensor_config_s {
	/**
	 * kPa/psi value at low volts
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 0
	 */
	float lowValue;
	/**
	 * kPa/psi value at high volts
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 4
	 */
	float highValue;
	/**
	 * offset 8
	 */
	air_pressure_sensor_type_e type;
	/**
	 * offset 9
	 */
	adc_channel_e hwChannel;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 10
	 */
	uint8_t alignmentFill_at_10[2] = {};
};
static_assert(sizeof(air_pressure_sensor_config_s) == 12);

// start of MAP_sensor_config_s
struct MAP_sensor_config_s {
	/**
	 * offset 0
	 */
	float samplingAngleBins[MAP_ANGLE_SIZE] = {};
	/**
	 * MAP averaging sampling start crank degree angle
	 * units: deg
	 * offset 32
	 */
	float samplingAngle[MAP_ANGLE_SIZE] = {};
	/**
	 * offset 64
	 */
	float samplingWindowBins[MAP_WINDOW_SIZE] = {};
	/**
	 * MAP averaging angle crank degree duration
	 * units: deg
	 * offset 96
	 */
	float samplingWindow[MAP_WINDOW_SIZE] = {};
	/**
	 * offset 128
	 */
	air_pressure_sensor_config_s sensor;
};
static_assert(sizeof(MAP_sensor_config_s) == 140);

/**
 * @brief Thermistor known values

*/
// start of thermistor_conf_s
struct thermistor_conf_s {
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 0
	 */
	float tempC_1;
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 4
	 */
	float tempC_2;
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 8
	 */
	float tempC_3;
	/**
	 * units: Ohm
	 * offset 12
	 */
	float resistance_1;
	/**
	 * units: Ohm
	 * offset 16
	 */
	float resistance_2;
	/**
	 * units: Ohm
	 * offset 20
	 */
	float resistance_3;
	/**
	 * Pull-up resistor value on your board
	 * units: Ohm
	 * offset 24
	 */
	float bias_resistor;
};
static_assert(sizeof(thermistor_conf_s) == 28);

// start of linear_sensor_s
struct linear_sensor_s {
	/**
	 * offset 0
	 */
	adc_channel_e hwChannel;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1
	 */
	uint8_t alignmentFill_at_1[3] = {};
	/**
	 * units: volts
	 * offset 4
	 */
	float v1;
	/**
	 * offset 8
	 */
	float value1;
	/**
	 * units: volts
	 * offset 12
	 */
	float v2;
	/**
	 * offset 16
	 */
	float value2;
};
static_assert(sizeof(linear_sensor_s) == 20);

// start of ThermistorConf
struct ThermistorConf {
	/**
	 * offset 0
	 */
	thermistor_conf_s config;
	/**
	 * offset 28
	 */
	adc_channel_e adcChannel;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 29
	 */
	uint8_t alignmentFill_at_29[3] = {};
};
static_assert(sizeof(ThermistorConf) == 32);

// start of injector_s
struct injector_s {
	/**
	 * This is your injector flow at the fuel pressure used in the vehicle
	 * See units setting below
	 * offset 0
	 */
	float flow;
	/**
	 * units: volts
	 * offset 4
	 */
	scaled_channel<int16_t, 100, 1> battLagCorrBattBins[VBAT_INJECTOR_CURVE_SIZE] = {};
	/**
	 * Injector correction pressure
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 20
	 */
	scaled_channel<uint32_t, 10, 1> battLagCorrPressBins[VBAT_INJECTOR_CURVE_PRESSURE_SIZE] = {};
	/**
	 * ms delay between injector open and close dead times
	 * units: ms
	 * offset 28
	 */
	scaled_channel<int16_t, 100, 1> battLagCorrTable[VBAT_INJECTOR_CURVE_PRESSURE_SIZE][VBAT_INJECTOR_CURVE_SIZE] = {};
};
static_assert(sizeof(injector_s) == 60);

// start of trigger_config_s
struct trigger_config_s {
	/**
	 * https://wiki.rusefi.com/All-Supported-Triggers
	 * offset 0
	 */
	trigger_type_e type;
	/**
	 * units: number
	 * offset 4
	 */
	int customTotalToothCount;
	/**
	 * units: number
	 * offset 8
	 */
	int customSkippedToothCount;
};
static_assert(sizeof(trigger_config_s) == 12);

// start of afr_sensor_s
struct afr_sensor_s {
	/**
	 * offset 0
	 */
	adc_channel_e hwChannel;
	/**
	 * offset 1
	 */
	adc_channel_e hwChannel2;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2
	 */
	uint8_t alignmentFill_at_2[2] = {};
	/**
	 * units: volts
	 * offset 4
	 */
	float v1;
	/**
	 * units: AFR
	 * offset 8
	 */
	float value1;
	/**
	 * units: volts
	 * offset 12
	 */
	float v2;
	/**
	 * units: AFR
	 * offset 16
	 */
	float value2;
};
static_assert(sizeof(afr_sensor_s) == 20);

// start of idle_hardware_s
struct idle_hardware_s {
	/**
	 * units: Hz
	 * offset 0
	 */
	int solenoidFrequency;
	/**
	 * offset 4
	 */
	output_pin_e solenoidPin;
	/**
	 * offset 6
	 */
	Gpio stepperDirectionPin;
	/**
	 * offset 8
	 */
	Gpio stepperStepPin;
	/**
	 * offset 10
	 */
	pin_output_mode_e solenoidPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 11
	 */
	uint8_t alignmentFill_at_11[1] = {};
};
static_assert(sizeof(idle_hardware_s) == 12);

// start of dc_io
struct dc_io {
	/**
	 * offset 0
	 */
	Gpio directionPin1;
	/**
	 * offset 2
	 */
	Gpio directionPin2;
	/**
	 * Acts as EN pin in two-wire mode
	 * offset 4
	 */
	Gpio controlPin;
	/**
	 * offset 6
	 */
	Gpio disablePin;
};
static_assert(sizeof(dc_io) == 8);

// start of vr_threshold_s
struct vr_threshold_s {
	/**
	 * units: rpm
	 * offset 0
	 */
	scaled_channel<uint8_t, 1, 50> rpmBins[6] = {};
	/**
	 * units: volts
	 * offset 6
	 */
	scaled_channel<uint8_t, 100, 1> values[6] = {};
	/**
	 * offset 12
	 */
	Gpio pin;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 14
	 */
	uint8_t alignmentFill_at_14[2] = {};
};
static_assert(sizeof(vr_threshold_s) == 16);

// start of wbo_s
struct wbo_s {
	/**
	 * offset 0
	 */
	can_wbo_type_e type;
	/**
	 * offset 1
	 */
	can_wbo_re_id_e reId;
	/**
	 * offset 2
	 */
	can_wbo_aem_id_e aemId;
	/**
	 * offset 3
	 */
	can_wbo_re_hwidx_e reHwidx;
	/**
	offset 4 bit 0 */
	bool enableRemap : 1 {};
	/**
	offset 4 bit 1 */
	bool unusedBit_5_1 : 1 {};
	/**
	offset 4 bit 2 */
	bool unusedBit_5_2 : 1 {};
	/**
	offset 4 bit 3 */
	bool unusedBit_5_3 : 1 {};
	/**
	offset 4 bit 4 */
	bool unusedBit_5_4 : 1 {};
	/**
	offset 4 bit 5 */
	bool unusedBit_5_5 : 1 {};
	/**
	offset 4 bit 6 */
	bool unusedBit_5_6 : 1 {};
	/**
	offset 4 bit 7 */
	bool unusedBit_5_7 : 1 {};
	/**
	offset 4 bit 8 */
	bool unusedBit_5_8 : 1 {};
	/**
	offset 4 bit 9 */
	bool unusedBit_5_9 : 1 {};
	/**
	offset 4 bit 10 */
	bool unusedBit_5_10 : 1 {};
	/**
	offset 4 bit 11 */
	bool unusedBit_5_11 : 1 {};
	/**
	offset 4 bit 12 */
	bool unusedBit_5_12 : 1 {};
	/**
	offset 4 bit 13 */
	bool unusedBit_5_13 : 1 {};
	/**
	offset 4 bit 14 */
	bool unusedBit_5_14 : 1 {};
	/**
	offset 4 bit 15 */
	bool unusedBit_5_15 : 1 {};
	/**
	offset 4 bit 16 */
	bool unusedBit_5_16 : 1 {};
	/**
	offset 4 bit 17 */
	bool unusedBit_5_17 : 1 {};
	/**
	offset 4 bit 18 */
	bool unusedBit_5_18 : 1 {};
	/**
	offset 4 bit 19 */
	bool unusedBit_5_19 : 1 {};
	/**
	offset 4 bit 20 */
	bool unusedBit_5_20 : 1 {};
	/**
	offset 4 bit 21 */
	bool unusedBit_5_21 : 1 {};
	/**
	offset 4 bit 22 */
	bool unusedBit_5_22 : 1 {};
	/**
	offset 4 bit 23 */
	bool unusedBit_5_23 : 1 {};
	/**
	offset 4 bit 24 */
	bool unusedBit_5_24 : 1 {};
	/**
	offset 4 bit 25 */
	bool unusedBit_5_25 : 1 {};
	/**
	offset 4 bit 26 */
	bool unusedBit_5_26 : 1 {};
	/**
	offset 4 bit 27 */
	bool unusedBit_5_27 : 1 {};
	/**
	offset 4 bit 28 */
	bool unusedBit_5_28 : 1 {};
	/**
	offset 4 bit 29 */
	bool unusedBit_5_29 : 1 {};
	/**
	offset 4 bit 30 */
	bool unusedBit_5_30 : 1 {};
	/**
	offset 4 bit 31 */
	bool unusedBit_5_31 : 1 {};
};
static_assert(sizeof(wbo_s) == 8);

// start of vvl_s
struct vvl_s {
	/**
	 * units: %
	 * offset 0
	 */
	int8_t fuelAdderPercent;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1
	 */
	uint8_t alignmentFill_at_1[3] = {};
	/**
	 * Retard timing to remove from actual final timing (after all corrections) due to additional air.
	 * units: deg
	 * offset 4
	 */
	float ignitionRetard;
	/**
	 * offset 8
	 */
	int minimumTps;
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 12
	 */
	int16_t minimumClt;
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 14
	 */
	int16_t maximumMap;
	/**
	 * units: afr
	 * offset 16
	 */
	scaled_channel<uint8_t, 10, 1> maximumAfr;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 17
	 */
	uint8_t alignmentFill_at_17[1] = {};
	/**
	 * units: rpm
	 * offset 18
	 */
	uint16_t activationRpm;
	/**
	 * units: rpm
	 * offset 20
	 */
	uint16_t deactivationRpm;
	/**
	 * units: rpm
	 * offset 22
	 */
	uint16_t deactivationRpmWindow;
};
static_assert(sizeof(vvl_s) == 24);

// start of rotational_idle_accumulator_s
struct rotational_idle_accumulator_s {
	/**
	 * Accumulator adder
	 * offset 0
	 */
	uint8_t acc_adder;
	/**
	 * Max value for the rotational idle accumulator to skip
	 * offset 1
	 */
	uint8_t acc_max;
	/**
	 * Rotational pattern shift
	 * offset 2
	 */
	uint8_t acc_offset;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3
	 */
	uint8_t alignmentFill_at_3[1] = {};
};
static_assert(sizeof(rotational_idle_accumulator_s) == 4);

// start of rotational_idle_s
struct rotational_idle_s {
	/**
	 * rotational idle enable feature
	offset 0 bit 0 */
	bool enabled : 1 {};
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
	 * rotational idle cut mode
	 * offset 4
	 */
	RotationalCutMode cut_mode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 5
	 */
	uint8_t alignmentFill_at_5[3] = {};
	/**
	 * Automatic engagement of rotational idle
	offset 8 bit 0 */
	bool auto_engage : 1 {};
	/**
	offset 8 bit 1 */
	bool unusedBit_35_1 : 1 {};
	/**
	offset 8 bit 2 */
	bool unusedBit_35_2 : 1 {};
	/**
	offset 8 bit 3 */
	bool unusedBit_35_3 : 1 {};
	/**
	offset 8 bit 4 */
	bool unusedBit_35_4 : 1 {};
	/**
	offset 8 bit 5 */
	bool unusedBit_35_5 : 1 {};
	/**
	offset 8 bit 6 */
	bool unusedBit_35_6 : 1 {};
	/**
	offset 8 bit 7 */
	bool unusedBit_35_7 : 1 {};
	/**
	offset 8 bit 8 */
	bool unusedBit_35_8 : 1 {};
	/**
	offset 8 bit 9 */
	bool unusedBit_35_9 : 1 {};
	/**
	offset 8 bit 10 */
	bool unusedBit_35_10 : 1 {};
	/**
	offset 8 bit 11 */
	bool unusedBit_35_11 : 1 {};
	/**
	offset 8 bit 12 */
	bool unusedBit_35_12 : 1 {};
	/**
	offset 8 bit 13 */
	bool unusedBit_35_13 : 1 {};
	/**
	offset 8 bit 14 */
	bool unusedBit_35_14 : 1 {};
	/**
	offset 8 bit 15 */
	bool unusedBit_35_15 : 1 {};
	/**
	offset 8 bit 16 */
	bool unusedBit_35_16 : 1 {};
	/**
	offset 8 bit 17 */
	bool unusedBit_35_17 : 1 {};
	/**
	offset 8 bit 18 */
	bool unusedBit_35_18 : 1 {};
	/**
	offset 8 bit 19 */
	bool unusedBit_35_19 : 1 {};
	/**
	offset 8 bit 20 */
	bool unusedBit_35_20 : 1 {};
	/**
	offset 8 bit 21 */
	bool unusedBit_35_21 : 1 {};
	/**
	offset 8 bit 22 */
	bool unusedBit_35_22 : 1 {};
	/**
	offset 8 bit 23 */
	bool unusedBit_35_23 : 1 {};
	/**
	offset 8 bit 24 */
	bool unusedBit_35_24 : 1 {};
	/**
	offset 8 bit 25 */
	bool unusedBit_35_25 : 1 {};
	/**
	offset 8 bit 26 */
	bool unusedBit_35_26 : 1 {};
	/**
	offset 8 bit 27 */
	bool unusedBit_35_27 : 1 {};
	/**
	offset 8 bit 28 */
	bool unusedBit_35_28 : 1 {};
	/**
	offset 8 bit 29 */
	bool unusedBit_35_29 : 1 {};
	/**
	offset 8 bit 30 */
	bool unusedBit_35_30 : 1 {};
	/**
	offset 8 bit 31 */
	bool unusedBit_35_31 : 1 {};
	/**
	 * Engage rotational idle under this Driver Intent.
	 * units: %TPS
	 * offset 12
	 */
	uint8_t max_tps;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 13
	 */
	uint8_t alignmentFill_at_13[3] = {};
	/**
	 * Rotational Idle Auto engage CLT
	offset 16 bit 0 */
	bool auto_engage_clt_enable : 1 {};
	/**
	offset 16 bit 1 */
	bool unusedBit_69_1 : 1 {};
	/**
	offset 16 bit 2 */
	bool unusedBit_69_2 : 1 {};
	/**
	offset 16 bit 3 */
	bool unusedBit_69_3 : 1 {};
	/**
	offset 16 bit 4 */
	bool unusedBit_69_4 : 1 {};
	/**
	offset 16 bit 5 */
	bool unusedBit_69_5 : 1 {};
	/**
	offset 16 bit 6 */
	bool unusedBit_69_6 : 1 {};
	/**
	offset 16 bit 7 */
	bool unusedBit_69_7 : 1 {};
	/**
	offset 16 bit 8 */
	bool unusedBit_69_8 : 1 {};
	/**
	offset 16 bit 9 */
	bool unusedBit_69_9 : 1 {};
	/**
	offset 16 bit 10 */
	bool unusedBit_69_10 : 1 {};
	/**
	offset 16 bit 11 */
	bool unusedBit_69_11 : 1 {};
	/**
	offset 16 bit 12 */
	bool unusedBit_69_12 : 1 {};
	/**
	offset 16 bit 13 */
	bool unusedBit_69_13 : 1 {};
	/**
	offset 16 bit 14 */
	bool unusedBit_69_14 : 1 {};
	/**
	offset 16 bit 15 */
	bool unusedBit_69_15 : 1 {};
	/**
	offset 16 bit 16 */
	bool unusedBit_69_16 : 1 {};
	/**
	offset 16 bit 17 */
	bool unusedBit_69_17 : 1 {};
	/**
	offset 16 bit 18 */
	bool unusedBit_69_18 : 1 {};
	/**
	offset 16 bit 19 */
	bool unusedBit_69_19 : 1 {};
	/**
	offset 16 bit 20 */
	bool unusedBit_69_20 : 1 {};
	/**
	offset 16 bit 21 */
	bool unusedBit_69_21 : 1 {};
	/**
	offset 16 bit 22 */
	bool unusedBit_69_22 : 1 {};
	/**
	offset 16 bit 23 */
	bool unusedBit_69_23 : 1 {};
	/**
	offset 16 bit 24 */
	bool unusedBit_69_24 : 1 {};
	/**
	offset 16 bit 25 */
	bool unusedBit_69_25 : 1 {};
	/**
	offset 16 bit 26 */
	bool unusedBit_69_26 : 1 {};
	/**
	offset 16 bit 27 */
	bool unusedBit_69_27 : 1 {};
	/**
	offset 16 bit 28 */
	bool unusedBit_69_28 : 1 {};
	/**
	offset 16 bit 29 */
	bool unusedBit_69_29 : 1 {};
	/**
	offset 16 bit 30 */
	bool unusedBit_69_30 : 1 {};
	/**
	offset 16 bit 31 */
	bool unusedBit_69_31 : 1 {};
	/**
	 * Rotational Idle Auto engage CLT.
	 * units: C
	 * offset 20
	 */
	int8_t auto_engage_clt;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 21
	 */
	uint8_t alignmentFill_at_21[3] = {};
	/**
	 * Rotational Idle accumulators
	 * offset 24
	 */
	rotational_idle_accumulator_s accumulators[3] = {};
};
static_assert(sizeof(rotational_idle_s) == 36);

// start of i2c_config_s
struct i2c_config_s {
	/**
	offset 0 bit 0 */
	bool enabled : 1 {};
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
	 * offset 4
	 */
	i2c_speed_e speed;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 5
	 */
	uint8_t alignmentFill_at_5[1] = {};
	/**
	 * offset 6
	 */
	Gpio sdaPin;
	/**
	 * offset 8
	 */
	Gpio sclPin;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 10
	 */
	uint8_t alignmentFill_at_10[2] = {};
};
static_assert(sizeof(i2c_config_s) == 12);

// start of can_sniffer_channel_s
struct can_sniffer_channel_s {
	/**
	 * Sniffer listens to this CAN channel
	offset 0 bit 0 */
	bool read : 1 {};
	/**
	 * Sniffer also listen to what RusEFI ECU says
	offset 0 bit 1 */
	bool listenOurs : 1 {};
	/**
	 * Messages transmitted from PC will be also handled by RusEFI ECU like it received from CAN
	offset 0 bit 2 */
	bool handleInjected : 1 {};
	/**
	offset 0 bit 3 */
	bool unusedBit_3_3 : 1 {};
	/**
	offset 0 bit 4 */
	bool unusedBit_3_4 : 1 {};
	/**
	offset 0 bit 5 */
	bool unusedBit_3_5 : 1 {};
	/**
	offset 0 bit 6 */
	bool unusedBit_3_6 : 1 {};
	/**
	offset 0 bit 7 */
	bool unusedBit_3_7 : 1 {};
	/**
	offset 0 bit 8 */
	bool unusedBit_3_8 : 1 {};
	/**
	offset 0 bit 9 */
	bool unusedBit_3_9 : 1 {};
	/**
	offset 0 bit 10 */
	bool unusedBit_3_10 : 1 {};
	/**
	offset 0 bit 11 */
	bool unusedBit_3_11 : 1 {};
	/**
	offset 0 bit 12 */
	bool unusedBit_3_12 : 1 {};
	/**
	offset 0 bit 13 */
	bool unusedBit_3_13 : 1 {};
	/**
	offset 0 bit 14 */
	bool unusedBit_3_14 : 1 {};
	/**
	offset 0 bit 15 */
	bool unusedBit_3_15 : 1 {};
	/**
	offset 0 bit 16 */
	bool unusedBit_3_16 : 1 {};
	/**
	offset 0 bit 17 */
	bool unusedBit_3_17 : 1 {};
	/**
	offset 0 bit 18 */
	bool unusedBit_3_18 : 1 {};
	/**
	offset 0 bit 19 */
	bool unusedBit_3_19 : 1 {};
	/**
	offset 0 bit 20 */
	bool unusedBit_3_20 : 1 {};
	/**
	offset 0 bit 21 */
	bool unusedBit_3_21 : 1 {};
	/**
	offset 0 bit 22 */
	bool unusedBit_3_22 : 1 {};
	/**
	offset 0 bit 23 */
	bool unusedBit_3_23 : 1 {};
	/**
	offset 0 bit 24 */
	bool unusedBit_3_24 : 1 {};
	/**
	offset 0 bit 25 */
	bool unusedBit_3_25 : 1 {};
	/**
	offset 0 bit 26 */
	bool unusedBit_3_26 : 1 {};
	/**
	offset 0 bit 27 */
	bool unusedBit_3_27 : 1 {};
	/**
	offset 0 bit 28 */
	bool unusedBit_3_28 : 1 {};
	/**
	offset 0 bit 29 */
	bool unusedBit_3_29 : 1 {};
	/**
	offset 0 bit 30 */
	bool unusedBit_3_30 : 1 {};
	/**
	offset 0 bit 31 */
	bool unusedBit_3_31 : 1 {};
};
static_assert(sizeof(can_sniffer_channel_s) == 4);

// start of engine_configuration_s
struct engine_configuration_s {
	/**
	 * http://rusefi.com/wiki/index.php?title=Manual:Engine_Type
	 * set engine_type X
	 * offset 0
	 */
	engine_type_e engineType;
	/**
	 * offset 2
	 */
	uint16_t startButtonSuppressOnStartUpMs;
	/**
	 * The target engine speed (RPM) to maintain during launch.
	 * units: rpm
	 * offset 4
	 */
	uint16_t launchRpm;
	/**
	 * set rpm_hard_limit X
	 * units: rpm
	 * offset 6
	 */
	uint16_t rpmHardLimit;
	/**
	 * Engine sniffer would be disabled above this rpm
	 * set engineSnifferRpmThreshold X
	 * units: RPM
	 * offset 8
	 */
	uint16_t engineSnifferRpmThreshold;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 10
	 */
	uint8_t alignmentFill_at_10[2] = {};
	/**
	 * Enable LTIT (Long Term Idle Trim) learning
	offset 12 bit 0 */
	bool ltitEnabled : 1 {};
	/**
	offset 12 bit 1 */
	bool useMetricOnInterface : 1 {};
	/**
	offset 12 bit 2 */
	bool useLambdaOnInterface : 1 {};
	/**
	offset 12 bit 3 */
	bool unusedBit_9_3 : 1 {};
	/**
	offset 12 bit 4 */
	bool unusedBit_9_4 : 1 {};
	/**
	offset 12 bit 5 */
	bool unusedBit_9_5 : 1 {};
	/**
	offset 12 bit 6 */
	bool unusedBit_9_6 : 1 {};
	/**
	offset 12 bit 7 */
	bool unusedBit_9_7 : 1 {};
	/**
	offset 12 bit 8 */
	bool unusedBit_9_8 : 1 {};
	/**
	offset 12 bit 9 */
	bool unusedBit_9_9 : 1 {};
	/**
	offset 12 bit 10 */
	bool unusedBit_9_10 : 1 {};
	/**
	offset 12 bit 11 */
	bool unusedBit_9_11 : 1 {};
	/**
	offset 12 bit 12 */
	bool unusedBit_9_12 : 1 {};
	/**
	offset 12 bit 13 */
	bool unusedBit_9_13 : 1 {};
	/**
	offset 12 bit 14 */
	bool unusedBit_9_14 : 1 {};
	/**
	offset 12 bit 15 */
	bool unusedBit_9_15 : 1 {};
	/**
	offset 12 bit 16 */
	bool unusedBit_9_16 : 1 {};
	/**
	offset 12 bit 17 */
	bool unusedBit_9_17 : 1 {};
	/**
	offset 12 bit 18 */
	bool unusedBit_9_18 : 1 {};
	/**
	offset 12 bit 19 */
	bool unusedBit_9_19 : 1 {};
	/**
	offset 12 bit 20 */
	bool unusedBit_9_20 : 1 {};
	/**
	offset 12 bit 21 */
	bool unusedBit_9_21 : 1 {};
	/**
	offset 12 bit 22 */
	bool unusedBit_9_22 : 1 {};
	/**
	offset 12 bit 23 */
	bool unusedBit_9_23 : 1 {};
	/**
	offset 12 bit 24 */
	bool unusedBit_9_24 : 1 {};
	/**
	offset 12 bit 25 */
	bool unusedBit_9_25 : 1 {};
	/**
	offset 12 bit 26 */
	bool unusedBit_9_26 : 1 {};
	/**
	offset 12 bit 27 */
	bool unusedBit_9_27 : 1 {};
	/**
	offset 12 bit 28 */
	bool unusedBit_9_28 : 1 {};
	/**
	offset 12 bit 29 */
	bool unusedBit_9_29 : 1 {};
	/**
	offset 12 bit 30 */
	bool unusedBit_9_30 : 1 {};
	/**
	offset 12 bit 31 */
	bool unusedBit_9_31 : 1 {};
	/**
	 * Disable multispark above this engine speed.
	 * units: rpm
	 * offset 16
	 */
	scaled_channel<uint8_t, 1, 50> multisparkMaxRpm;
	/**
	 * Above this RPM, disable AC. Set to 0 to disable check.
	 * units: rpm
	 * offset 17
	 */
	scaled_channel<uint8_t, 1, 50> maxAcRpm;
	/**
	 * Above this TPS, disable AC. Set to 0 to disable check.
	 * units: %
	 * offset 18
	 */
	uint8_t maxAcTps;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 19
	 */
	uint8_t alignmentFill_at_19[1] = {};
	/**
	 * Above this CLT, disable AC to prevent overheating the engine. Set to 0 to disable check.
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 20
	 */
	int16_t maxAcClt;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 22
	 */
	uint8_t alignmentFill_at_22[2] = {};
	/**
	 * Just for reference really, not taken into account by any logic at this point
	 * units: CR
	 * offset 24
	 */
	float compressionRatio;
	/**
	 * Voltage when the idle valve is closed.
	 * You probably don't have one of these!
	 * units: mv
	 * offset 28
	 */
	uint16_t idlePositionMin;
	/**
	 * Voltage when the idle valve is open.
	 * You probably don't have one of these!
	 * 1 volt = 1000 units
	 * units: mv
	 * offset 30
	 */
	uint16_t idlePositionMax;
	/**
	 * EMA filter constant for LTIT (0-255)
	 * units: 0-255
	 * offset 32
	 */
	uint8_t ltitEmaAlpha;
	/**
	 * RPM range to consider stable idle
	 * units: rpm
	 * offset 33
	 */
	uint8_t ltitStableRpmThreshold;
	/**
	 * Minimum time of stable idle before learning
	 * units: s
	 * offset 34
	 */
	uint8_t ltitStableTime;
	/**
	 * LTIT learning rate
	 * units: %/s
	 * offset 35
	 */
	uint8_t ltitCorrectionRate;
	/**
	 * Delay after ignition ON before LTIT learning/application
	 * units: s
	 * offset 36
	 */
	uint8_t ltitIgnitionOnDelay;
	/**
	 * Delay after ignition OFF before LTIT save
	 * units: s
	 * offset 37
	 */
	uint8_t ltitIgnitionOffSaveDelay;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 38
	 */
	uint8_t alignmentFill_at_38[2] = {};
	/**
	 * Minimum LTIT multiplicative correction value
	 * units: %
	 * offset 40
	 */
	float ltitClampMin;
	/**
	 * Maximum LTIT multiplicative correction value
	 * units: %
	 * offset 44
	 */
	float ltitClampMax;
	/**
	 * LTIT table regional smoothing intensity (0=no smoothing)
	 * units: ratio
	 * offset 48
	 */
	scaled_channel<uint8_t, 100, 1> ltitSmoothingIntensity;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 49
	 */
	uint8_t alignmentFill_at_49[3] = {};
	/**
	 * Minimum threshold of PID integrator for LTIT correction
	 * units: %
	 * offset 52
	 */
	float ltitIntegratorThreshold;
	/**
	 * offset 56
	 */
	output_pin_e mainRelayPin;
	/**
	 * offset 58
	 */
	Gpio sdCardCsPin;
	/**
	 * offset 60
	 */
	Gpio canTxPin;
	/**
	 * offset 62
	 */
	Gpio canRxPin;
	/**
	 * Pin that activates the reduction/cut for shifting. Sometimes shared with the Launch Control pin
	 * offset 64
	 */
	switch_input_pin_e torqueReductionTriggerPin;
	/**
	 * Fuel enrichment adder percentage.
	 * units: %
	 * offset 66
	 */
	int8_t launchFuelAdderPercent;
	/**
	 * Time after which the throttle is considered jammed.
	 * units: sec
	 * offset 67
	 */
	scaled_channel<uint8_t, 50, 1> etbJamTimeout;
	/**
	 * offset 68
	 */
	output_pin_e tachOutputPin;
	/**
	 * offset 70
	 */
	pin_output_mode_e tachOutputPinMode;
	/**
	 * Number of primary trigger teeth to see (since ignition-on) before firing the priming pulse. Counting is raw and does not require trigger sync. Only used when 'primeOnTriggerTeeth' is enabled.
	 * units: teeth
	 * offset 71
	 */
	uint8_t primingTriggerTeeth;
	/**
	 * This parameter sets the latest that the last multispark can occur after the main ignition event. For example, if the ignition timing is 30 degrees BTDC, and this parameter is set to 45, no multispark will ever be fired after 15 degrees ATDC.
	 * units: deg
	 * offset 72
	 */
	uint8_t multisparkMaxSparkingAngle;
	/**
	 * Configures the maximum number of extra sparks to fire (does not include main spark)
	 * units: count
	 * offset 73
	 */
	uint8_t multisparkMaxExtraSparkCount;
	/**
	 * units: RPM
	 * offset 74
	 */
	int16_t vvtControlMinRpm;
	/**
	 * offset 76
	 */
	injector_s injector;
	/**
	 * offset 136
	 */
	injector_s injectorSecondary;
	/**
	 * Does the vehicle have a turbo or supercharger?
	offset 196 bit 0 */
	bool isForcedInduction : 1 {};
	/**
	 * On some Ford and Toyota vehicles one of the throttle sensors is not linear on the full range, i.e. in the specific range of the positions we effectively have only one sensor.
	offset 196 bit 1 */
	bool useFordRedundantTps : 1 {};
	/**
	offset 196 bit 2 */
	bool enableKline : 1 {};
	/**
	offset 196 bit 3 */
	bool overrideTriggerGaps : 1 {};
	/**
	offset 196 bit 4 */
	bool chtSensorPulldown : 1 {};
	/**
	offset 196 bit 5 */
	bool useLinearChtSensor : 1 {};
	/**
	 * Enable secondary spark outputs that fire after the primary (rotaries, twin plug engines).
	offset 196 bit 6 */
	bool enableTrailingSparks : 1 {};
	/**
	 * TLE7209 and L6205 use two-wire mode. TLE9201 and VNH2SP30 do NOT use two wire mode.
	offset 196 bit 7 */
	bool etb_use_two_wires : 1 {};
	/**
	 * Subaru/BMW style where default valve position is somewhere in the middle. First solenoid opens it more while second can close it more than default position.
	offset 196 bit 8 */
	bool isDoubleSolenoidIdle : 1 {};
	/**
	offset 196 bit 9 */
	bool useEeprom : 1 {};
	/**
	 * Switch between Industrial and Cic PID implementation
	offset 196 bit 10 */
	bool useCicPidForIdle : 1 {};
	/**
	offset 196 bit 11 */
	bool useTLE8888_cranking_hack : 1 {};
	/**
	offset 196 bit 12 */
	bool kickStartCranking : 1 {};
	/**
	 * This uses separate ignition timing and VE tables not only for idle conditions, also during the postcranking-to-idle taper transition (See also afterCrankingIACtaperDuration).
	offset 196 bit 13 */
	bool useSeparateIdleTablesForCrankingTaper : 1 {};
	/**
	offset 196 bit 14 */
	bool launchControlEnabled : 1 {};
	/**
	offset 196 bit 15 */
	bool antiLagEnabled : 1 {};
	/**
	 * For cranking either use the specified fixed base fuel mass, or use the normal running math (VE table). Note: in 'Fuel Map' (running math) mode the base mass already reflects the flex-adjusted stoich ratio, so the cranking flex multipliers act as ADDITIONAL enrichment on top of that - do not re-apply the full ethanol correction there.
	offset 196 bit 16 */
	bool useRunningMathForCranking : 1 {};
	/**
	 * Enable CLT-based cranking air amount table. During cranking, open-loop valve position is taken from this table instead of the running idle tables.
	offset 196 bit 17 */
	bool crankingAirAmountEnabled : 1 {};
	/**
	 * Enable CLT-based cranking idle RPM flare. An RPM adder from the table is applied during cranking, tapering to zero as the engine transitions to idle.
	offset 196 bit 18 */
	bool crankingIdleRpmFlareEnabled : 1 {};
	/**
	 * Shall we display real life signal or just the part consumed by trigger decoder.
	 * Applies to both trigger and cam/vvt input.
	offset 196 bit 19 */
	bool displayLogicLevelsInEngineSniffer : 1 {};
	/**
	offset 196 bit 20 */
	bool useTLE8888_stepper : 1 {};
	/**
	offset 196 bit 21 */
	bool usescriptTableForCanSniffingFiltering : 1 {};
	/**
	 * Print incoming and outgoing first bus CAN messages in rusEFI console
	offset 196 bit 22 */
	bool verboseCan : 1 {};
	/**
	 * Experimental setting that will cause a misfire
	 * DO NOT ENABLE.
	offset 196 bit 23 */
	bool artificialTestMisfire : 1 {};
	/**
	 * On some Ford and Toyota vehicles one of the pedal sensors is not linear on the full range, i.e. in the specific range of the positions we effectively have only one sensor.
	offset 196 bit 24 */
	bool useFordRedundantPps : 1 {};
	/**
	offset 196 bit 25 */
	bool cltSensorPulldown : 1 {};
	/**
	offset 196 bit 26 */
	bool iatSensorPulldown : 1 {};
	/**
	offset 196 bit 27 */
	bool allowIdenticalPps : 1 {};
	/**
	offset 196 bit 28 */
	bool overrideVvtTriggerGaps : 1 {};
	/**
	 * If enabled - use onboard SPI Accelerometer, otherwise listen for CAN messages
	offset 196 bit 29 */
	bool useSpiImu : 1 {};
	/**
	offset 196 bit 30 */
	bool enableStagedInjection : 1 {};
	/**
	offset 196 bit 31 */
	bool useIdleAdvanceWhileCoasting : 1 {};
	/**
	 * Closed voltage for primary throttle position sensor
	 * offset 200
	 */
	tps_limit_t tpsMin;
	/**
	 * Fully opened voltage for primary throttle position sensor
	 * offset 202
	 */
	tps_limit_t tpsMax;
	/**
	 * TPS error detection: what throttle % is unrealistically low?
	 * Also used for accelerator pedal error detection if so equipped.
	 * units: %
	 * offset 204
	 */
	int16_t tpsErrorDetectionTooLow;
	/**
	 * TPS error detection: what throttle % is unrealistically high?
	 * Also used for accelerator pedal error detection if so equipped.
	 * units: %
	 * offset 206
	 */
	int16_t tpsErrorDetectionTooHigh;
	/**
	 * offset 208
	 */
	cranking_parameters_s cranking;
	/**
	 * Dwell duration while cranking
	 * units: ms
	 * offset 212
	 */
	float ignitionDwellForCrankingMs;
	/**
	 * RPM below the hard RPM limit at which the ETB rev limiter PID starts managing throttle position. Below this window throttle control is normal.
	 * units: rpm
	 * offset 216
	 */
	uint16_t etbRevLimitRange;
	/**
	 * Throttle position the ETB rev limiter PID seeds itself with as soon as it engages, so it starts near the right operating point instead of ramping up from zero.
	 * units: %
	 * offset 218
	 */
	scaled_channel<uint8_t, 2, 1> etbRevLimitSeedTps;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 219
	 */
	uint8_t alignmentFill_at_219[1] = {};
	/**
	 * Proportional gain for the ETB rev limiter PID.
	 * offset 220
	 */
	float etbRevLimitKp;
	/**
	 * Integral gain for the ETB rev limiter PID.
	 * offset 224
	 */
	float etbRevLimitKi;
	/**
	 * Derivative gain for the ETB rev limiter PID.
	 * offset 228
	 */
	float etbRevLimitKd;
	/**
	 * @see isMapAveragingEnabled
	 * offset 232
	 */
	MAP_sensor_config_s map;
	/**
	 * todo: merge with channel settings, use full-scale Thermistor here!
	 * offset 372
	 */
	ThermistorConf clt;
	/**
	 * offset 404
	 */
	ThermistorConf iat;
	/**
	 * The target absolute ignition timing value (e.g., -10 means -10 degrees, not 10 degrees of retard relative to base timing).
	 * units: deg
	 * offset 436
	 */
	float launchTimingRetard;
	/**
	 * Maximum commanded airmass for the idle controller.
	 * units: mg
	 * offset 440
	 */
	scaled_channel<uint8_t, 1, 2> idleMaximumAirmass;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 441
	 */
	uint8_t alignmentFill_at_441[1] = {};
	/**
	 * iTerm min value
	 * offset 442
	 */
	int16_t alternator_iTermMin;
	/**
	 * iTerm max value
	 * offset 444
	 */
	int16_t alternator_iTermMax;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 446
	 */
	uint8_t alignmentFill_at_446[2] = {};
	/**
	 * @@DISPLACEMENT_TOOLTIP@@
	 * units: L
	 * offset 448
	 */
	float displacement;
	/**
	 * units: RPM
	 * offset 452
	 */
	uint16_t triggerSimulatorRpm;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 454
	 */
	uint8_t alignmentFill_at_454[2] = {};
	/**
	 * Number of cylinder the engine has.
	 * offset 456
	 */
	uint32_t cylindersCount;
	/**
	 * offset 460
	 */
	firing_order_e firingOrder;
	/**
	 * offset 461
	 */
	uint8_t justATempTest;
	/**
	 * Delta kPa/psi for MAP sync
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 462
	 */
	uint8_t mapSyncThreshold;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 463
	 */
	uint8_t alignmentFill_at_463[1] = {};
	/**
	 * @@CYLINDER_BORE_TOOLTIP@@
	 * units: mm
	 * offset 464
	 */
	float cylinderBore;
	/**
	 * Determines the method used for calculating fuel delivery. The following options are available:
	 * Uses intake manifold pressure (MAP) and intake air temperature (IAT) to calculate air density and fuel requirements. This is a common strategy, especially for naturally aspirated or turbocharged engines.
	 * Alpha-N: Uses throttle position as the primary load input for fuel calculation. This strategy is generally used in engines with individual throttle bodies or those that lack a reliable MAP signal.
	 * MAF Air Charge: Relies on a Mass Air Flow (MAF) sensor to measure the amount of air entering the engine directly, making it effective for engines equipped with a MAF sensor.
	 * Lua: Allows for custom fuel calculations using Lua scripting, enabling highly specific tuning applications where the other strategies don't apply.
	 * offset 468
	 */
	engine_load_mode_e fuelAlgorithm;
	/**
	 * units: %
	 * offset 469
	 */
	uint8_t ALSMaxTPS;
	/**
	 * This is the injection strategy during engine start. See Fuel/Injection settings for more detail. It is suggested to use "Simultaneous".
	 * offset 470
	 */
	injection_mode_e crankingInjectionMode;
	/**
	 * This is where the fuel injection type is defined: "Simultaneous" means all injectors will fire together at once. "Sequential" fires the injectors on a per cylinder basis, which requires individually wired injectors. "Batched" will fire the injectors in groups.
	 * offset 471
	 */
	injection_mode_e injectionMode;
	/**
	 * Minimum RPM to enable boost control. Use this to avoid solenoid noise at idle, and help spool in some cases.
	 * offset 472
	 */
	uint16_t boostControlMinRpm;
	/**
	 * Minimum TPS to enable boost control. Use this to avoid solenoid noise at idle, and help spool in some cases.
	 * offset 474
	 */
	uint8_t boostControlMinTps;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 475
	 */
	uint8_t alignmentFill_at_475[1] = {};
	/**
	 * Minimum MAP to enable boost control. Use this to avoid solenoid noise at idle, and help spool in some cases.
	 * offset 476
	 */
	uint16_t boostControlMinMap;
	/**
	 * Wastegate control Solenoid, set to 'NONE' if you are using DC wastegate
	 * offset 478
	 */
	output_pin_e boostControlPin;
	/**
	 * offset 480
	 */
	pin_output_mode_e boostControlPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 481
	 */
	uint8_t alignmentFill_at_481[3] = {};
	/**
	 * Ignition advance angle used during engine cranking, 5-10 degrees will work as a base setting for most engines.
	 * There is tapering towards running timing advance
	 * set cranking_timing_angle X
	 * units: deg
	 * offset 484
	 */
	angle_t crankingTimingAngle;
	/**
	 * Single coil = distributor
	 * Individual coils = one coil per cylinder (COP, coil-near-plug), requires sequential mode
	 * Wasted spark = Fires pairs of cylinders together, either one coil per pair of cylinders or one coil per cylinder
	 * Two distributors = A pair of distributors, found on some BMW, Toyota and other engines
	 * set ignition_mode X
	 * offset 488
	 */
	ignition_mode_e ignitionMode;
	/**
	 * How many consecutive gap rations have to match expected ranges for sync to happen
	 * units: count
	 * offset 489
	 */
	int8_t gapTrackingLengthOverride;
	/**
	 * Above this speed, disable closed loop idle control. Set to 0 to disable (allow closed loop idle at any speed).
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 490
	 */
	uint8_t maxIdleVss;
	/**
	 * Allowed range around detection position
	 * offset 491
	 */
	uint8_t camDecoder2jzPrecision;
	/**
	 * Expected oil pressure after starting the engine. If oil pressure does not reach this level within 5 seconds of engine start, fuel will be cut. Set to 0 to disable and always allow starting.
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 492
	 */
	uint16_t minOilPressureAfterStart;
	/**
	 * Dynamic uses the timing map to decide the ignition timing
	 * Static timing fixes the timing to the value set below (only use for checking static timing with a timing light).
	 * offset 494
	 */
	timing_mode_e timingMode;
	/**
	 * offset 495
	 */
	can_nbc_e canNbcType;
	/**
	 * This value is the ignition timing used when in 'fixed timing' mode, i.e. constant timing
	 * This mode is useful when adjusting distributor location.
	 * units: RPM
	 * offset 496
	 */
	angle_t fixedModeTiming;
	/**
	 * Angle between Top Dead Center (TDC) and the first trigger event.
	 * Positive value in case of synchronization point before TDC and negative in case of synchronization point after TDC
	 * .Knowing this angle allows us to control timing and other angles in reference to TDC.
	 * HOWTO:
	 * 1: Switch to fixed timing mode on 'ignition setting' dialog
	 * 2: use an actual timing light to calibrate
	 * 3: add/subtract until timing light confirms desired fixed timing value!'
	 * units: deg btdc
	 * offset 500
	 */
	angle_t globalTriggerAngleOffset;
	/**
	 * Ratio/coefficient of input voltage dividers on your PCB. For example, use '2' if your board divides 5v into 2.5v. Use '1.66' if your board divides 5v into 3v.
	 * units: coef
	 * offset 504
	 */
	float analogInputDividerCoefficient;
	/**
	 * This is the ratio of the resistors for the battery voltage, measure the voltage at the battery and then adjust this number until the gauge matches the reading.
	 * units: coef
	 * offset 508
	 */
	float vbattDividerCoeff;
	/**
	 * offset 512
	 */
	output_pin_e fanPin;
	/**
	 * offset 514
	 */
	pin_output_mode_e fanPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 515
	 */
	uint8_t alignmentFill_at_515[1] = {};
	/**
	 * Cooling fan turn-on temperature threshold, in Celsius
	 * units: SPECIAL_CASE_TEMPERATURE
	 * offset 516
	 */
	int16_t fanOnTemperature;
	/**
	 * Cooling fan turn-off temperature threshold, in Celsius
	 * units: SPECIAL_CASE_TEMPERATURE
	 * offset 518
	 */
	int16_t fanOffTemperature;
	/**
	 * offset 520
	 */
	output_pin_e fan2Pin;
	/**
	 * offset 522
	 */
	pin_output_mode_e fan2PinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 523
	 */
	uint8_t alignmentFill_at_523[1] = {};
	/**
	 * Cooling fan turn-on temperature threshold, in Celsius
	 * units: SPECIAL_CASE_TEMPERATURE
	 * offset 524
	 */
	int16_t fan2OnTemperature;
	/**
	 * Cooling fan turn-off temperature threshold, in Celsius
	 * units: SPECIAL_CASE_TEMPERATURE
	 * offset 526
	 */
	int16_t fan2OffTemperature;
	/**
	 * offset 528
	 */
	int8_t disableFan1AtSpeed;
	/**
	 * offset 529
	 */
	int8_t disableFan2AtSpeed;
	/**
	 * Hysteresis below the disable-at-speed threshold before the fan is allowed back on. Prevents rapid on/off cycling at the threshold speed.
	 * units: mph
	 * offset 530
	 */
	int8_t disableFan1AtSpeedHysteresis;
	/**
	 * Hysteresis below the disable-at-speed threshold before the fan is allowed back on. Prevents rapid on/off cycling at the threshold speed.
	 * units: mph
	 * offset 531
	 */
	int8_t disableFan2AtSpeedHysteresis;
	/**
	 * Inhibit operation of this fan while the engine is not running.
	offset 532 bit 0 */
	bool disableFan1WhenStopped : 1 {};
	/**
	 * Inhibit operation of this fan while the engine is not running.
	offset 532 bit 1 */
	bool disableFan2WhenStopped : 1 {};
	/**
	 * Enable PWM mode for Fan 1. When enabled, the fan output is driven by the PWM curve instead of on/off relay logic.
	offset 532 bit 2 */
	bool fan1PwmEnabled : 1 {};
	/**
	 * Enable PWM mode for Fan 2. When enabled, the fan output is driven by the PWM curve instead of on/off relay logic.
	offset 532 bit 3 */
	bool fan2PwmEnabled : 1 {};
	/**
	offset 532 bit 4 */
	bool unusedBit_177_4 : 1 {};
	/**
	offset 532 bit 5 */
	bool unusedBit_177_5 : 1 {};
	/**
	offset 532 bit 6 */
	bool unusedBit_177_6 : 1 {};
	/**
	offset 532 bit 7 */
	bool unusedBit_177_7 : 1 {};
	/**
	offset 532 bit 8 */
	bool unusedBit_177_8 : 1 {};
	/**
	offset 532 bit 9 */
	bool unusedBit_177_9 : 1 {};
	/**
	offset 532 bit 10 */
	bool unusedBit_177_10 : 1 {};
	/**
	offset 532 bit 11 */
	bool unusedBit_177_11 : 1 {};
	/**
	offset 532 bit 12 */
	bool unusedBit_177_12 : 1 {};
	/**
	offset 532 bit 13 */
	bool unusedBit_177_13 : 1 {};
	/**
	offset 532 bit 14 */
	bool unusedBit_177_14 : 1 {};
	/**
	offset 532 bit 15 */
	bool unusedBit_177_15 : 1 {};
	/**
	offset 532 bit 16 */
	bool unusedBit_177_16 : 1 {};
	/**
	offset 532 bit 17 */
	bool unusedBit_177_17 : 1 {};
	/**
	offset 532 bit 18 */
	bool unusedBit_177_18 : 1 {};
	/**
	offset 532 bit 19 */
	bool unusedBit_177_19 : 1 {};
	/**
	offset 532 bit 20 */
	bool unusedBit_177_20 : 1 {};
	/**
	offset 532 bit 21 */
	bool unusedBit_177_21 : 1 {};
	/**
	offset 532 bit 22 */
	bool unusedBit_177_22 : 1 {};
	/**
	offset 532 bit 23 */
	bool unusedBit_177_23 : 1 {};
	/**
	offset 532 bit 24 */
	bool unusedBit_177_24 : 1 {};
	/**
	offset 532 bit 25 */
	bool unusedBit_177_25 : 1 {};
	/**
	offset 532 bit 26 */
	bool unusedBit_177_26 : 1 {};
	/**
	offset 532 bit 27 */
	bool unusedBit_177_27 : 1 {};
	/**
	offset 532 bit 28 */
	bool unusedBit_177_28 : 1 {};
	/**
	offset 532 bit 29 */
	bool unusedBit_177_29 : 1 {};
	/**
	offset 532 bit 30 */
	bool unusedBit_177_30 : 1 {};
	/**
	offset 532 bit 31 */
	bool unusedBit_177_31 : 1 {};
	/**
	 * Fan 1 PWM frequency
	 * units: Hz
	 * offset 536
	 */
	uint16_t fan1PwmFrequency;
	/**
	 * Fan 2 PWM frequency
	 * units: Hz
	 * offset 538
	 */
	uint16_t fan2PwmFrequency;
	/**
	 * Fan 1 PWM curve temperature bins
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 540
	 */
	int16_t fan1TempBins[FAN_PWM_CURVE_SIZE] = {};
	/**
	 * Fan 1 PWM curve output values
	 * units: %
	 * offset 556
	 */
	uint8_t fan1PwmValues[FAN_PWM_CURVE_SIZE] = {};
	/**
	 * Fan 2 PWM curve temperature bins
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 564
	 */
	int16_t fan2TempBins[FAN_PWM_CURVE_SIZE] = {};
	/**
	 * Fan 2 PWM curve output values
	 * units: %
	 * offset 580
	 */
	uint8_t fan2PwmValues[FAN_PWM_CURVE_SIZE] = {};
	/**
	 * Minimum PWM output clamp (Fan 1)
	 * units: %
	 * offset 588
	 */
	uint8_t fan1MinPwm;
	/**
	 * Maximum PWM output clamp (Fan 1)
	 * units: %
	 * offset 589
	 */
	uint8_t fan1MaxPwm;
	/**
	 * Minimum PWM output clamp (Fan 2)
	 * units: %
	 * offset 590
	 */
	uint8_t fan2MinPwm;
	/**
	 * Maximum PWM output clamp (Fan 2)
	 * units: %
	 * offset 591
	 */
	uint8_t fan2MaxPwm;
	/**
	 * PWM adder when AC compressor is active (Fan 1)
	 * units: %
	 * offset 592
	 */
	uint8_t fan1AcAdder;
	/**
	 * PWM adder when AC compressor is active (Fan 2)
	 * units: %
	 * offset 593
	 */
	uint8_t fan2AcAdder;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 594
	 */
	uint8_t alignmentFill_at_594[2] = {};
	/**
	 * Soft-start ramp time ? how long to ramp from 0 to target PWM (Fan 1)
	 * units: s
	 * offset 596
	 */
	float fan1SoftStartSec;
	/**
	 * Soft-start ramp time ? how long to ramp from 0 to target PWM (Fan 2)
	 * units: s
	 * offset 600
	 */
	float fan2SoftStartSec;
	/**
	 * offset 604
	 */
	int wizardPanelToShow;
	/**
	 * offset 608
	 */
	output_pin_e acrPin;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 610
	 */
	uint8_t alignmentFill_at_610[2] = {};
	/**
	 * Number of revolutions per kilometer for the wheels your vehicle speed sensor is connected to. Use an online calculator to determine this based on your tire size.
	 * units: revs/km
	 * offset 612
	 */
	float driveWheelRevPerKm;
	/**
	 * CANbus thread period in ms
	 * units: ms
	 * offset 616
	 */
	int canSleepPeriodMs;
	/**
	 * units: index
	 * offset 620
	 */
	int byFirmwareVersion;
	/**
	 * First analog throttle body, first sensor. See also pedalPositionAdcChannel
	 * Analog TPS inputs have 200Hz low-pass cutoff.
	 * offset 624
	 */
	adc_channel_e tps1_1AdcChannel;
	/**
	 * This is the processor input pin that the battery voltage circuit is connected to, if you are unsure of what pin to use, check the schematic that corresponds to your PCB.
	 * offset 625
	 */
	adc_channel_e vbattAdcChannel;
	/**
	 * This is the processor pin that your fuel level sensor in connected to. This is a non standard input so will need to be user defined.
	 * offset 626
	 */
	adc_channel_e fuelLevelSensor;
	/**
	 * Second throttle body position sensor, single channel so far
	 * offset 627
	 */
	adc_channel_e tps2_1AdcChannel;
	/**
	 * 0.1 is a good default value
	 * units: x
	 * offset 628
	 */
	float idle_derivativeFilterLoss;
	/**
	 * offset 632
	 */
	trigger_config_s trigger;
	/**
	 * Extra air taper amount
	 * units: %
	 * offset 644
	 */
	float airByRpmTaper;
	/**
	 * Duty cycle to use in case of a sensor failure. This duty cycle should produce the minimum possible amount of boost. This duty is also used in case any of the minimum RPM/TPS/MAP conditions are not met.
	 * units: %
	 * offset 648
	 */
	uint8_t boostControlSafeDutyCycle;
	/**
	 * offset 649
	 */
	adc_channel_e mafAdcChannel;
	/**
	 * offset 650
	 */
	uint8_t acrRevolutions;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 651
	 */
	uint8_t alignmentFill_at_651[1] = {};
	/**
	 * offset 652
	 */
	int calibrationBirthday;
	/**
	 * units: volts
	 * offset 656
	 */
	float adcVcc;
	/**
	 * Magic engine phase: we compare instant MAP at X to instant MAP at x+360 angle in one complete cycle
	 * units: Deg
	 * offset 660
	 */
	float mapCamDetectionAnglePosition;
	/**
	 * Camshaft input could be used either just for engine phase detection if your trigger shape does not include cam sensor as 'primary' channel, or it could be used for Variable Valve timing on one of the camshafts.
	 * offset 664
	 */
	brain_input_pin_e camInputs[CAM_INPUTS_COUNT] = {};
	/**
	 * offset 672
	 */
	afr_sensor_s afr;
	/**
	 * Electronic throttle pedal position first channel
	 * See throttlePedalPositionSecondAdcChannel for second channel
	 * See also tps1_1AdcChannel
	 * See throttlePedalUpVoltage and throttlePedalWOTVoltage
	 * offset 692
	 */
	adc_channel_e throttlePedalPositionAdcChannel;
	/**
	 * TPS/PPS error threshold
	 * units: %
	 * offset 693
	 */
	scaled_channel<uint8_t, 2, 1> etbSplit;
	/**
	 * offset 694
	 */
	Gpio tle6240_cs;
	/**
	 * offset 696
	 */
	pin_output_mode_e tle6240_csPinMode;
	/**
	 * offset 697
	 */
	pin_output_mode_e mc33810_csPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 698
	 */
	uint8_t alignmentFill_at_698[2] = {};
	/**
	 * @see hasBaroSensor
	 * offset 700
	 */
	air_pressure_sensor_config_s baroSensor;
	/**
	 * offset 712
	 */
	idle_hardware_s idle;
	/**
	 * Ignition timing to remove when a knock event occurs. Advice: 5% (mild), 10% (turbo/high comp.), 15% (high knock, e.g. GDI), 20% (spicy lump),
	 * units: %
	 * offset 724
	 */
	scaled_channel<uint8_t, 10, 1> knockRetardAggression;
	/**
	 * After a knock event, reapply timing at this rate.
	 * units: deg/s
	 * offset 725
	 */
	scaled_channel<uint8_t, 10, 1> knockRetardReapplyRate;
	/**
	 * Select which cam is used for engine sync. Other cams will be used only for VVT measurement, but not engine sync.
	 * offset 726
	 */
	engineSyncCam_e engineSyncCam;
	/**
	 * offset 727
	 */
	pin_output_mode_e sdCardCsPinMode;
	/**
	 * Number of turns of your vehicle speed sensor per turn of the wheels. For example if your sensor is on the transmission output, enter your axle/differential ratio. If you are using a hub-mounted sensor, enter a value of 1.0.
	 * units: ratio
	 * offset 728
	 */
	scaled_channel<uint16_t, 1000, 1> vssGearRatio;
	/**
	 * Set this so your vehicle speed signal is responsive, but not noisy. Larger value give smoother but slower response.
	 * offset 730
	 */
	uint8_t vssFilterReciprocal;
	/**
	 * Number of pulses output per revolution of the shaft where your VSS is mounted. For example, GM applications of the T56 output 17 pulses per revolution of the transmission output shaft.
	 * units: count
	 * offset 731
	 */
	uint8_t vssToothCount;
	/**
	 * Reject VSS pulses that imply a faster acceleration or deceleration than this, and dead-reckon speed from the last known rate of change instead. Helps reject a single noisy tooth. Set to 0 to disable.
	 * units: km/h/sec
	 * offset 732
	 */
	uint8_t vssMaxAcceleration;
	/**
	 * Allows you to change the default load axis used for the VE table, which is typically MAP (manifold absolute pressure).
	 * offset 733
	 */
	ve_override_e idleVeOverrideMode;
	/**
	 * offset 734
	 */
	Gpio l9779_cs;
	/**
	 * offset 736
	 */
	output_pin_e injectionPins[MAX_CYLINDER_COUNT] = {};
	/**
	 * offset 760
	 */
	output_pin_e ignitionPins[MAX_CYLINDER_COUNT] = {};
	/**
	 * offset 784
	 */
	pin_output_mode_e injectionPinMode;
	/**
	 * offset 785
	 */
	pin_output_mode_e ignitionPinMode;
	/**
	 * offset 786
	 */
	output_pin_e fuelPumpPin;
	/**
	 * offset 788
	 */
	pin_output_mode_e fuelPumpPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 789
	 */
	uint8_t alignmentFill_at_789[1] = {};
	/**
	 * Secondary fuel pump output pin (Dual mode only)
	 * offset 790
	 */
	output_pin_e fuelPump2Pin;
	/**
	 * Secondary fuel pump output mode
	 * offset 792
	 */
	pin_output_mode_e fuelPump2PinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 793
	 */
	uint8_t alignmentFill_at_793[3] = {};
	/**
	 * PID settings for closed-loop fuel pressure control (PWM mode, requires FP sensor)
	 * offset 796
	 */
	pid_s fuelPumpControl;
	/**
	 * How many consecutive VVT gap rations have to match expected ranges for sync to happen
	 * units: count
	 * offset 816
	 */
	int8_t gapVvtTrackingLengthOverride;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 817
	 */
	uint8_t alignmentFill_at_817[1] = {};
	/**
	 * Check engine light, also malfunction indicator light. Always blinks once on boot.
	 * offset 818
	 */
	output_pin_e malfunctionIndicatorPin;
	/**
	 * offset 820
	 */
	pin_output_mode_e malfunctionIndicatorPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 821
	 */
	uint8_t alignmentFill_at_821[1] = {};
	/**
	 * Some cars have a switch to indicate that clutch pedal is all the way down
	 * offset 822
	 */
	switch_input_pin_e clutchDownPin;
	/**
	 * offset 824
	 */
	output_pin_e alternatorControlPin;
	/**
	 * offset 826
	 */
	pin_output_mode_e alternatorControlPinMode;
	/**
	 * offset 827
	 */
	pin_input_mode_e clutchDownPinMode;
	/**
	 * offset 828
	 */
	Gpio digitalPotentiometerChipSelect[DIGIPOT_COUNT] = {};
	/**
	 * offset 836
	 */
	pin_output_mode_e electronicThrottlePin1Mode;
	/**
	 * offset 837
	 */
	spi_device_e max31855spiDevice;
	/**
	 * offset 838
	 */
	Gpio debugTriggerSync;
	/**
	 * offset 840
	 */
	Gpio debugTriggerState;
	/**
	 * Digital Potentiometer is used by stock ECU stimulation code
	 * offset 842
	 */
	spi_device_e digitalPotentiometerSpiDevice;
	/**
	 * offset 843
	 */
	pin_input_mode_e brakePedalPinMode;
	/**
	 * offset 844
	 */
	Gpio mc33972_cs;
	/**
	 * offset 846
	 */
	pin_output_mode_e mc33972_csPinMode;
	/**
	 * Useful in Research&Development phase
	 * offset 847
	 */
	adc_channel_e auxFastSensor1_adcChannel;
	/**
	 * First throttle body, second sensor.
	 * offset 848
	 */
	adc_channel_e tps1_2AdcChannel;
	/**
	 * Second throttle body, second sensor.
	 * offset 849
	 */
	adc_channel_e tps2_2AdcChannel;
	/**
	 * Electronic throttle pedal position input
	 * Second channel
	 * See also tps1_1AdcChannel
	 * See throttlePedalSecondaryUpVoltage and throttlePedalSecondaryWOTVoltage
	 * offset 850
	 */
	adc_channel_e throttlePedalPositionSecondAdcChannel;
	/**
	 * AFR, WBO, EGO - whatever you like to call it
	 * offset 851
	 */
	ego_sensor_e afr_type;
	/**
	 * offset 852
	 */
	Gpio mc33810_cs[C_MC33810_COUNT] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 854
	 */
	uint8_t alignmentFill_at_854[2] = {};
	/**
	 * 0.1 is a good default value
	 * units: x
	 * offset 856
	 */
	float idle_antiwindupFreq;
	/**
	 * offset 860
	 */
	brain_input_pin_e triggerInputPins[TRIGGER_INPUT_PIN_COUNT] = {};
	/**
	 * Minimum allowed time for the boost phase. If the boost target current is reached before this time elapses, it is assumed that the injector has failed short circuit.
	 * units: us
	 * offset 864
	 */
	uint16_t mc33_t_min_boost;
	/**
	 * Ratio between the wheels and your transmission output.
	 * units: ratio
	 * offset 866
	 */
	scaled_channel<uint16_t, 100, 1> finalGearRatio;
	/**
	 * offset 868
	 */
	brain_input_pin_e tcuInputSpeedSensorPin;
	/**
	 * offset 870
	 */
	uint8_t tcuInputSpeedSensorTeeth;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 871
	 */
	uint8_t alignmentFill_at_871[1] = {};
	/**
	 * Each rusEFI piece can provide synthetic trigger signal for external ECU. Sometimes these wires are routed back into trigger inputs of the same rusEFI board.
	 * See also directSelfStimulation which is different.
	 * offset 872
	 */
	Gpio triggerSimulatorPins[TRIGGER_SIMULATOR_PIN_COUNT] = {};
	/**
	 * units: g/s
	 * offset 876
	 */
	scaled_channel<uint16_t, 1000, 1> fordInjectorSmallPulseSlope;
	/**
	 * offset 878
	 */
	pin_output_mode_e triggerSimulatorPinModes[TRIGGER_SIMULATOR_PIN_COUNT] = {};
	/**
	 * offset 880
	 */
	adc_channel_e maf2AdcChannel;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 881
	 */
	uint8_t alignmentFill_at_881[1] = {};
	/**
	 * On-off O2 sensor heater control. 'ON' if engine is running, 'OFF' if stopped or cranking.
	 * offset 882
	 */
	output_pin_e o2heaterPin;
	/**
	 * offset 884
	 */
	pin_output_mode_e o2heaterPinModeTodO;
	/**
	 * units: RPM
	 * offset 885
	 */
	scaled_channel<uint8_t, 1, 100> lambdaProtectionMinRpm;
	/**
	 * units: %
	 * offset 886
	 */
	scaled_channel<uint8_t, 1, 10> lambdaProtectionMinLoad;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 887
	 */
	uint8_t alignmentFill_at_887[1] = {};
	/**
	offset 888 bit 0 */
	bool is_enabled_spi_1 : 1 {};
	/**
	offset 888 bit 1 */
	bool is_enabled_spi_2 : 1 {};
	/**
	offset 888 bit 2 */
	bool is_enabled_spi_3 : 1 {};
	/**
	offset 888 bit 3 */
	bool isSdCardEnabled : 1 {};
	/**
	 * Use 11 bit (standard) or 29 bit (extended) IDs for rusEFI verbose CAN format.
	offset 888 bit 4 */
	bool rusefiVerbose29b : 1 {};
	/**
	offset 888 bit 5 */
	bool rethrowHardFault : 1 {};
	/**
	offset 888 bit 6 */
	bool verboseQuad : 1 {};
	/**
	 * This setting should only be used if you have a stepper motor idle valve and a stepper motor driver installed.
	offset 888 bit 7 */
	bool useStepperIdle : 1 {};
	/**
	offset 888 bit 8 */
	bool lambdaProtectionEnable : 1 {};
	/**
	offset 888 bit 9 */
	bool verboseTLE8888 : 1 {};
	/**
	 * CAN broadcast using custom rusEFI protocol
	offset 888 bit 10 */
	bool enableVerboseCanTx : 1 {};
	/**
	offset 888 bit 11 */
	bool externalRusEfiGdiModule : 1 {};
	/**
	 * Useful for individual intakes
	offset 888 bit 12 */
	bool measureMapOnlyInOneCylinder : 1 {};
	/**
	offset 888 bit 13 */
	bool stepperForceParkingEveryRestart : 1 {};
	/**
	 * If enabled, RPM is estimated from ~90 degrees of rotation using tooth timestamps collected even before trigger sync, and fuel/ignition scheduling starts as soon as the trigger syncs (sequential ignition temporarily runs as wasted spark until full phase sync). As soon as the trigger syncs plus 90 degrees rotation, fuel and ignition events will occur. If disabled, worst case may require up to 4 full crank rotations before any events are scheduled.
	offset 888 bit 14 */
	bool isFasterEngineSpinUpEnabled : 1 {};
	/**
	 * This setting disables fuel injection while the engine is in overrun, this is useful as a fuel saving measure and to prevent back firing.
	offset 888 bit 15 */
	bool coastingFuelCutEnabled : 1 {};
	/**
	 * Override the IAC position during overrun conditions to help reduce engine breaking, this can be helpful for large engines in light weight cars or engines that have trouble returning to idle.
	offset 888 bit 16 */
	bool useIacTableForCoasting : 1 {};
	/**
	offset 888 bit 17 */
	bool useNoiselessTriggerDecoder : 1 {};
	/**
	offset 888 bit 18 */
	bool useIdleTimingPidControl : 1 {};
	/**
	 * Allows disabling the ETB when the engine is stopped. You may not like the power draw or PWM noise from the motor, so this lets you turn it off until it's necessary.
	offset 888 bit 19 */
	bool disableEtbWhenEngineStopped : 1 {};
	/**
	offset 888 bit 20 */
	bool is_enabled_spi_4 : 1 {};
	/**
	 * Disable the electronic throttle motor and DC idle motor for testing.
	 * This mode is for testing ETB/DC idle position sensors, etc without actually driving the throttle.
	offset 888 bit 21 */
	bool pauseEtbControl : 1 {};
	/**
	offset 888 bit 22 */
	bool verboseKLine : 1 {};
	/**
	offset 888 bit 23 */
	bool idleIncrementalPidCic : 1 {};
	/**
	 * AEM X-Series or rusEFI Wideband
	offset 888 bit 24 */
	bool enableAemXSeries : 1 {};
	/**
	offset 888 bit 25 */
	bool modeledFlowIdle : 1 {};
	/**
	offset 888 bit 26 */
	bool isTuningDetectorEnabled : 1 {};
	/**
	offset 888 bit 27 */
	bool useAbsolutePressureForLagTime : 1 {};
	/**
	 * Ramp the idle target down from the entry threshold over N seconds when returning to idle. Helps prevent overshooting (below) the idle target while returning to idle from coasting.
	offset 888 bit 28 */
	bool idleReturnTargetRamp : 1 {};
	/**
	offset 888 bit 29 */
	bool useInjectorFlowLinearizationTable : 1 {};
	/**
	 * If enabled we use two H-bridges to drive stepper idle air valve
	offset 888 bit 30 */
	bool useHbridgesToDriveIdleStepper : 1 {};
	/**
	offset 888 bit 31 */
	bool multisparkEnable : 1 {};
	/**
	 * Enables absolute ignition timing control during launch (sets timing to the "Absolute Timing at Launch" value).
	offset 892 bit 0 */
	bool enableLaunchRetard : 1 {};
	/**
	offset 892 bit 1 */
	bool canInputBCM : 1 {};
	/**
	 * This property is useful if using rusEFI as TCM or BCM only
	offset 892 bit 2 */
	bool consumeObdSensors : 1 {};
	/**
	 * Read VSS from OEM CAN bus according to selected CAN vehicle configuration.
	offset 892 bit 3 */
	bool enableCanVss : 1 {};
	/**
	 * If enabled, adjust at a constant rate instead of a rate proportional to the current lambda error. This mode may be easier to tune, and more tolerant of sensor noise.
	offset 892 bit 4 */
	bool stftIgnoreErrorMagnitude : 1 {};
	/**
	offset 892 bit 5 */
	bool vvtBooleanForVerySpecialCases : 1 {};
	/**
	offset 892 bit 6 */
	bool enableSoftwareKnock : 1 {};
	/**
	 * Verbose info in console below engineSnifferRpmThreshold
	offset 892 bit 7 */
	bool verboseVVTDecoding : 1 {};
	/**
	offset 892 bit 8 */
	bool invertCamVVTSignal : 1 {};
	/**
	 * When set to yes, it enables intake air temperature-based corrections for Alpha-N tuning strategies.
	offset 892 bit 9 */
	bool alphaNUseIat : 1 {};
	/**
	offset 892 bit 10 */
	bool knockBankCyl1 : 1 {};
	/**
	offset 892 bit 11 */
	bool knockBankCyl2 : 1 {};
	/**
	offset 892 bit 12 */
	bool knockBankCyl3 : 1 {};
	/**
	offset 892 bit 13 */
	bool knockBankCyl4 : 1 {};
	/**
	offset 892 bit 14 */
	bool knockBankCyl5 : 1 {};
	/**
	offset 892 bit 15 */
	bool knockBankCyl6 : 1 {};
	/**
	offset 892 bit 16 */
	bool knockBankCyl7 : 1 {};
	/**
	offset 892 bit 17 */
	bool knockBankCyl8 : 1 {};
	/**
	offset 892 bit 18 */
	bool knockBankCyl9 : 1 {};
	/**
	offset 892 bit 19 */
	bool knockBankCyl10 : 1 {};
	/**
	offset 892 bit 20 */
	bool knockBankCyl11 : 1 {};
	/**
	offset 892 bit 21 */
	bool knockBankCyl12 : 1 {};
	/**
	offset 892 bit 22 */
	bool tcuEnabled : 1 {};
	/**
	 * If enabled we use four Push-Pull outputs to directly drive stepper idle air valve coils
	offset 892 bit 23 */
	bool useRawOutputToDriveIdleStepper : 1 {};
	/**
	 * Print incoming and outgoing second bus CAN messages in rusEFI console
	offset 892 bit 24 */
	bool verboseCan2 : 1 {};
	/**
	offset 892 bit 25 */
	bool unusedBit_370_25 : 1 {};
	/**
	offset 892 bit 26 */
	bool unusedBit_370_26 : 1 {};
	/**
	offset 892 bit 27 */
	bool unusedBit_370_27 : 1 {};
	/**
	offset 892 bit 28 */
	bool unusedBit_370_28 : 1 {};
	/**
	offset 892 bit 29 */
	bool unusedBit_370_29 : 1 {};
	/**
	offset 892 bit 30 */
	bool unusedBit_370_30 : 1 {};
	/**
	offset 892 bit 31 */
	bool unusedBit_370_31 : 1 {};
	/**
	 * offset 896
	 */
	brain_input_pin_e logicAnalyzerPins[LOGIC_ANALYZER_CHANNEL_COUNT] = {};
	/**
	 * offset 904
	 */
	pin_output_mode_e mainRelayPinMode;
	/**
	 * Time after ignition turn-off before the main relay is disabled.
	 * units: s
	 * offset 905
	 */
	uint8_t mainRelayDisableTime;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 906
	 */
	uint8_t alignmentFill_at_906[2] = {};
	/**
	 * offset 908
	 */
	uint32_t verboseCanBaseAddress;
	/**
	 * Boost Voltage
	 * units: v
	 * offset 912
	 */
	uint8_t mc33_hvolt;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 913
	 */
	uint8_t alignmentFill_at_913[1] = {};
	/**
	 * Minimum MAP before closed loop boost is enabled. Use to prevent misbehavior upon entering boost.
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 914
	 */
	uint16_t minimumBoostClosedLoopMap;
	/**
	 * The percentage of ignition events to cut when entering the launch control window (e.g., at Launch RPM minus Launch Control Window).
	 * units: %
	 * offset 916
	 */
	int8_t initialIgnitionCutPercent;
	/**
	 * The percentage of ignition events to cut when the engine speed reaches the end of the corrections RPM (Launch RPM minus Launch Corrections End RPM). Between the start of the window and the end of corrections RPM, the cut percentage interpolates linearly from initial to final cut percentage.
	 * units: %
	 * offset 917
	 */
	int8_t finalIgnitionCutPercentBeforeLaunch;
	/**
	 * offset 918
	 */
	gppwm_channel_e boostOpenLoopYAxis;
	/**
	 * offset 919
	 */
	spi_device_e l9779spiDevice;
	/**
	 * offset 920
	 */
	imu_type_e imuType;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 921
	 */
	uint8_t alignmentFill_at_921[1] = {};
	/**
	 * How far above idle speed do we consider idling, i.e. coasting detection threshold.
	 * For example, if target = 800, this param = 200, then anything below 1000 RPM is considered idle.
	 * units: RPM
	 * offset 922
	 */
	int16_t idlePidRpmUpperLimit;
	/**
	 * Apply nonlinearity correction below a pulse of this duration. Pulses longer than this duration will receive no adjustment.
	 * units: ms
	 * offset 924
	 */
	scaled_channel<uint16_t, 1000, 1> applyNonlinearBelowPulse;
	/**
	 * offset 926
	 */
	Gpio lps25BaroSensorScl;
	/**
	 * offset 928
	 */
	Gpio lps25BaroSensorSda;
	/**
	 * offset 930
	 */
	brain_input_pin_e vehicleSpeedSensorInputPin;
	/**
	 * Some vehicles have a switch to indicate that clutch pedal is all the way up
	 * offset 932
	 */
	switch_input_pin_e clutchUpPin;
	/**
	 * offset 934
	 */
	InjectorNonlinearMode injectorNonlinearMode;
	/**
	 * offset 935
	 */
	pin_input_mode_e clutchUpPinMode;
	/**
	 * offset 936
	 */
	Gpio max31855_cs[EGT_CHANNEL_COUNT] = {};
	/**
	 * Continental/GM flex fuel sensor, 50-150hz type
	 * offset 952
	 */
	brain_input_pin_e flexSensorPin;
	/**
	 * Since torque reduction pin is usually shared with launch control, most people have an RPM where behavior under that is Launch Control, over that is Flat Shift/Torque Reduction
	 * units: rpm
	 * offset 954
	 */
	uint16_t torqueReductionArmingRpm;
	/**
	 * offset 956
	 */
	pin_output_mode_e stepperDirectionPinMode;
	/**
	 * offset 957
	 */
	spi_device_e mc33972spiDevice;
	/**
	 * Stoichiometric ratio for your secondary fuel. This value is used when the Flex Fuel sensor indicates E100, typically 9.0
	 * units: :1
	 * offset 958
	 */
	scaled_channel<uint8_t, 10, 1> stoichRatioSecondary;
	/**
	 * Maximum allowed ETB position. Some throttles go past fully open, so this allows you to limit it to fully open.
	 * units: %
	 * offset 959
	 */
	uint8_t etbMaximumPosition;
	/**
	 * Rate the ECU will log to the SD card, in hz (log lines per second).
	 * units: hz
	 * offset 960
	 */
	uint16_t sdCardLogFrequency;
	/**
	 * offset 962
	 */
	adc_channel_e idlePositionChannel;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 963
	 */
	uint8_t alignmentFill_at_963[1] = {};
	/**
	 * The RPM difference below the Launch RPM at which corrections (timing retard interpolation and/or ignition cut ramp) reach their final/maximum target. For example, if Launch RPM is 4000, and this is 50, corrections reach their final target at 3950 RPM.
	 * units: RPM
	 * offset 964
	 */
	uint16_t launchCorrectionsEndRpm;
	/**
	 * offset 966
	 */
	output_pin_e starterRelayDisablePin;
	/**
	 * On some vehicles we can disable starter once engine is already running
	 * offset 968
	 */
	pin_output_mode_e starterRelayDisablePinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 969
	 */
	uint8_t alignmentFill_at_969[1] = {};
	/**
	 * Some Subaru and some Mazda use double-solenoid idle air valve
	 * offset 970
	 */
	output_pin_e secondSolenoidPin;
	/**
	 * See also starterControlPin
	 * offset 972
	 */
	switch_input_pin_e startStopButtonPin;
	/**
	 * units: RPM
	 * offset 974
	 */
	scaled_channel<uint8_t, 1, 100> lambdaProtectionRestoreRpm;
	/**
	 * offset 975
	 */
	pin_output_mode_e acRelayPinMode;
	/**
	 * This many MAP samples are used to estimate the current MAP. This many samples are considered, and the minimum taken. Recommended value is 1 for single-throttle engines, and your number of cylinders for individual throttle bodies.
	 * units: count
	 * offset 976
	 */
	int mapMinBufferLength;
	/**
	 * Below this throttle position, the engine is considered idling. If you have an electronic throttle, this checks accelerator pedal position instead of throttle position, and should be set to 1-2%.
	 * units: %
	 * offset 980
	 */
	int16_t idlePidDeactivationTpsThreshold;
	/**
	 * units: %
	 * offset 982
	 */
	int16_t stepperParkingExtraSteps;
	/**
	 * Closed voltage for secondary throttle position sensor
	 * offset 984
	 */
	tps_limit_t tps1SecondaryMin;
	/**
	 * Fully opened voltage for secondary throttle position sensor
	 * offset 986
	 */
	tps_limit_t tps1SecondaryMax;
	/**
	 * Maximum time to crank starter when start/stop button is pressed
	 * units: Seconds
	 * offset 988
	 */
	uint16_t startCrankingDuration;
	/**
	 * This pin is used for debugging - snap a logic analyzer on it and see if it's ever high
	 * offset 990
	 */
	Gpio triggerErrorPin;
	/**
	 * offset 992
	 */
	pin_output_mode_e triggerErrorPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 993
	 */
	uint8_t alignmentFill_at_993[1] = {};
	/**
	 * offset 994
	 */
	output_pin_e acRelayPin;
	/**
	 * units: %
	 * offset 996
	 */
	uint8_t lambdaProtectionMinTps;
	/**
	 * Only respond once lambda is out of range for this period of time. Use to avoid transients triggering lambda protection when not needed
	 * units: s
	 * offset 997
	 */
	scaled_channel<uint8_t, 10, 1> lambdaProtectionTimeout;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 998
	 */
	uint8_t alignmentFill_at_998[2] = {};
	/**
	 * offset 1000
	 */
	script_setting_t scriptSetting[SCRIPT_SETTING_COUNT] = {};
	/**
	 * offset 1032
	 */
	Gpio spi1mosiPin;
	/**
	 * offset 1034
	 */
	Gpio spi1misoPin;
	/**
	 * offset 1036
	 */
	Gpio spi1sckPin;
	/**
	 * offset 1038
	 */
	Gpio spi2mosiPin;
	/**
	 * offset 1040
	 */
	Gpio spi2misoPin;
	/**
	 * offset 1042
	 */
	Gpio spi2sckPin;
	/**
	 * offset 1044
	 */
	Gpio spi3mosiPin;
	/**
	 * offset 1046
	 */
	Gpio spi3misoPin;
	/**
	 * offset 1048
	 */
	Gpio spi3sckPin;
	/**
	 * UNUSED
	 * Will remove in 2026 for sure
	 * Saab Combustion Detection Module knock signal input pin
	 * also known as Saab Ion Sensing Module
	 * offset 1050
	 */
	Gpio cdmInputPin;
	/**
	 * offset 1052
	 */
	uart_device_e consoleUartDevice;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1053
	 */
	uint8_t alignmentFill_at_1053[3] = {};
	/**
	offset 1056 bit 0 */
	bool unusedBit_offIdleEnabled : 1 {};
	/**
	offset 1056 bit 1 */
	bool canBroadcastUseChannelTwo : 1 {};
	/**
	 * When Launch Control is NOT activated by Clutch Up, use the Clutch Up switch to positively confirm the clutch has been released and disable launch.
	offset 1056 bit 2 */
	bool disableLaunchWithClutchUp : 1 {};
	/**
	offset 1056 bit 3 */
	bool unusedBit_446_3 : 1 {};
	/**
	offset 1056 bit 4 */
	bool unusedBit_446_4 : 1 {};
	/**
	offset 1056 bit 5 */
	bool unusedBit_446_5 : 1 {};
	/**
	offset 1056 bit 6 */
	bool unusedBit_446_6 : 1 {};
	/**
	offset 1056 bit 7 */
	bool unusedBit_446_7 : 1 {};
	/**
	offset 1056 bit 8 */
	bool unusedBit_446_8 : 1 {};
	/**
	offset 1056 bit 9 */
	bool unusedBit_446_9 : 1 {};
	/**
	offset 1056 bit 10 */
	bool unusedBit_446_10 : 1 {};
	/**
	offset 1056 bit 11 */
	bool unusedBit_446_11 : 1 {};
	/**
	offset 1056 bit 12 */
	bool unusedBit_446_12 : 1 {};
	/**
	offset 1056 bit 13 */
	bool unusedBit_446_13 : 1 {};
	/**
	offset 1056 bit 14 */
	bool unusedBit_446_14 : 1 {};
	/**
	offset 1056 bit 15 */
	bool unusedBit_446_15 : 1 {};
	/**
	offset 1056 bit 16 */
	bool unusedBit_446_16 : 1 {};
	/**
	offset 1056 bit 17 */
	bool unusedBit_446_17 : 1 {};
	/**
	offset 1056 bit 18 */
	bool unusedBit_446_18 : 1 {};
	/**
	offset 1056 bit 19 */
	bool unusedBit_446_19 : 1 {};
	/**
	offset 1056 bit 20 */
	bool unusedBit_446_20 : 1 {};
	/**
	offset 1056 bit 21 */
	bool unusedBit_446_21 : 1 {};
	/**
	offset 1056 bit 22 */
	bool unusedBit_446_22 : 1 {};
	/**
	offset 1056 bit 23 */
	bool unusedBit_446_23 : 1 {};
	/**
	offset 1056 bit 24 */
	bool unusedBit_446_24 : 1 {};
	/**
	offset 1056 bit 25 */
	bool unusedBit_446_25 : 1 {};
	/**
	offset 1056 bit 26 */
	bool unusedBit_446_26 : 1 {};
	/**
	offset 1056 bit 27 */
	bool unusedBit_446_27 : 1 {};
	/**
	offset 1056 bit 28 */
	bool unusedBit_446_28 : 1 {};
	/**
	offset 1056 bit 29 */
	bool unusedBit_446_29 : 1 {};
	/**
	offset 1056 bit 30 */
	bool unusedBit_446_30 : 1 {};
	/**
	offset 1056 bit 31 */
	bool unusedBit_446_31 : 1 {};
	/**
	 * offset 1060
	 */
	dc_io etbIo[ETB_COUNT] = {};
	/**
	 * offset 1076
	 */
	switch_input_pin_e ALSActivatePin;
	/**
	 * offset 1078
	 */
	switch_input_pin_e launchActivatePin;
	/**
	 * offset 1080
	 */
	pid_s boostPid;
	/**
	 * offset 1100
	 */
	boostType_e boostType;
	/**
	 * offset 1101
	 */
	pin_input_mode_e ignitionKeyDigitalPinMode;
	/**
	 * offset 1102
	 */
	Gpio ignitionKeyDigitalPin;
	/**
	 * units: Hz
	 * offset 1104
	 */
	int boostPwmFrequency;
	/**
	 * offset 1108
	 */
	launchActivationMode_e launchActivationMode;
	/**
	 * offset 1109
	 */
	antiLagActivationMode_e antiLagActivationMode;
	/**
	 * offset 1110
	 */
	cranking_condition_e crankingCondition;
	/**
	 * How long to look back for TPS-based acceleration enrichment. Increasing this time will trigger enrichment for longer when a throttle position change occurs.
	 * units: sec
	 * offset 1111
	 */
	scaled_channel<uint8_t, 20, 1> tpsAccelLookback;
	/**
	 * For decel we simply multiply delta of TPS and tFor decel we do not use table?!
	 * units: roc
	 * offset 1112
	 */
	float tpsDecelEnleanmentThreshold;
	/**
	 * Magic multiplier, we multiply delta of TPS and get fuel squirt duration
	 * units: coeff
	 * offset 1116
	 */
	float tpsDecelEnleanmentMultiplier;
	/**
	 * Selects the acceleration enrichment strategy.
	 * offset 1120
	 */
	accel_enrichment_mode_e accelEnrichmentMode;
	/**
	 * Pause closed loop fueling after deceleration fuel cut occurs. Set this to a little longer than however long is required for normal fueling behavior to resume after fuel cut.
	 * units: sec
	 * offset 1121
	 */
	scaled_channel<uint8_t, 10, 1> noFuelTrimAfterDfcoTime;
	/**
	 * Pause closed loop fueling after acceleration fuel occurs. Set this to a little longer than however long is required for normal fueling behavior to resume after fuel accel.
	 * units: sec
	 * offset 1122
	 */
	scaled_channel<uint8_t, 10, 1> noFuelTrimAfterAccelTime;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1123
	 */
	uint8_t alignmentFill_at_1123[1] = {};
	/**
	 * Launch disabled above this speed if setting is above zero
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 1124
	 */
	int launchSpeedThreshold;
	/**
	 * The RPM window before the Launch RPM where launch control strategies (like retard/cut) begin to activate. For example, if Launch RPM is 4000 and Window is 500, activation starts at 3500 RPM.
	 * units: RPM
	 * offset 1128
	 */
	int launchRpmWindow;
	/**
	 * units: ms
	 * offset 1132
	 */
	float triggerEventsTimeoutMs;
	/**
	 * A higher alpha (closer to 1) means the EMA reacts more quickly to changes in the data.
	 * '100%' means no filtering, 98% would be some filtering.
	 * units: percent
	 * offset 1136
	 */
	float ppsExpAverageAlpha;
	/**
	 * A higher alpha (closer to 1) means the EMA reacts more quickly to changes in the data.
	 * '1' means no filtering, 0.98 would be some filtering.
	 * offset 1140
	 */
	float mapExpAverageAlpha;
	/**
	 * offset 1144
	 */
	float magicNumberAvailableForDevTricks;
	/**
	 * offset 1148
	 */
	float turbochargerFilter;
	/**
	 * offset 1152
	 */
	int launchTpsThreshold;
	/**
	 * offset 1156
	 */
	float launchActivateDelay;
	/**
	 * offset 1160
	 */
	stft_s stft;
	/**
	 * offset 1188
	 */
	ltft_s ltft;
	/**
	 * offset 1204
	 */
	dc_io stepperDcIo[DC_PER_STEPPER] = {};
	/**
	 * For example, BMW, GM or Chevrolet
	 * REQUIRED for rusEFI Online
	 * offset 1220
	 */
	vehicle_info_t engineMake;
	/**
	 * For example, LS1 or NB2
	 * REQUIRED for rusEFI Online
	 * offset 1252
	 */
	vehicle_info_t engineCode;
	/**
	 * For example, Hunchback or Orange Miata
	 * Vehicle name has to be unique between your vehicles.
	 * REQUIRED for rusEFI Online
	 * offset 1284
	 */
	vehicle_info_t vehicleName;
	/**
	 * offset 1316
	 */
	output_pin_e tcu_solenoid[TCU_SOLENOID_COUNT] = {};
	/**
	 * offset 1328
	 */
	dc_function_e etbFunctions[ETB_COUNT] = {};
	/**
	 * offset 1330
	 */
	spi_device_e drv8860spiDevice;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1331
	 */
	uint8_t alignmentFill_at_1331[1] = {};
	/**
	 * offset 1332
	 */
	Gpio drv8860_cs;
	/**
	 * offset 1334
	 */
	pin_output_mode_e drv8860_csPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1335
	 */
	uint8_t alignmentFill_at_1335[1] = {};
	/**
	 * offset 1336
	 */
	Gpio drv8860_miso;
	/**
	 * offset 1338
	 */
	output_pin_e luaOutputPins[LUA_PWM_COUNT] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1354
	 */
	uint8_t alignmentFill_at_1354[2] = {};
	/**
	 * Angle between cam sensor and VVT zero position
	 * units: value
	 * offset 1356
	 */
	float vvtOffsets[CAM_INPUTS_COUNT] = {};
	/**
	 * offset 1372
	 */
	vr_threshold_s vrThreshold[VR_THRESHOLD_COUNT] = {};
	/**
	 * offset 1404
	 */
	gppwm_note_t gpPwmNote[GPPWM_CHANNELS] = {};
	/**
	 * Closed voltage for secondary throttle position sensor
	 * offset 1468
	 */
	tps_limit_t tps2SecondaryMin;
	/**
	 * Fully opened voltage for secondary throttle position sensor
	 * offset 1470
	 */
	tps_limit_t tps2SecondaryMax;
	/**
	 * Select which bus the wideband controller is attached to.
	offset 1472 bit 0 */
	bool widebandOnSecondBus : 1 {};
	/**
	 * Enables lambda sensor closed loop feedback for fuelling.
	offset 1472 bit 1 */
	bool fuelClosedLoopCorrectionEnabled : 1 {};
	/**
	 * Write SD card log even when powered by USB
	offset 1472 bit 2 */
	bool alwaysWriteSdCard : 1 {};
	/**
	 * Second harmonic (aka double) is usually quieter background noise
	offset 1472 bit 3 */
	bool knockDetectionUseDoubleFrequency : 1 {};
	/**
	 * Unlocking only via rusEFI console using 'unlock PICODEBUG' command. Use 'reset to default firmware' if pincode is lost.
	offset 1472 bit 4 */
	bool yesUnderstandLocking : 1 {};
	/**
	 * Sometimes we have a performance issue while printing error
	offset 1472 bit 5 */
	bool silentTriggerError : 1 {};
	/**
	offset 1472 bit 6 */
	bool useLinearCltSensor : 1 {};
	/**
	 * enable can_read/disable can_read
	offset 1472 bit 7 */
	bool canReadEnabled : 1 {};
	/**
	 * enable can_write/disable can_write. See also can1ListenMode
	offset 1472 bit 8 */
	bool canWriteEnabled : 1 {};
	/**
	offset 1472 bit 9 */
	bool useLinearIatSensor : 1 {};
	/**
	offset 1472 bit 10 */
	bool enableOilPressureProtect : 1 {};
	/**
	 * Treat milliseconds value as duty cycle value, i.e. 0.5ms would become 50%
	offset 1472 bit 11 */
	bool tachPulseDurationAsDutyCycle : 1 {};
	/**
	 * This enables smart alternator control and activates the extra alternator settings.
	offset 1472 bit 12 */
	bool isAlternatorControlEnabled : 1 {};
	/**
	 * Select base duty source: a 2D table (indexed by target voltage and RPM) or the legacy scalar offset in the PID settings.
	offset 1472 bit 13 */
	bool alternatorBaseDutyUseTable : 1 {};
	/**
	 * https://wiki.rusefi.com/Trigger-Configuration-Guide
	 * This setting flips the signal from the primary engine speed sensor.
	offset 1472 bit 14 */
	bool invertPrimaryTriggerSignal : 1 {};
	/**
	 * https://wiki.rusefi.com/Trigger-Configuration-Guide
	 * This setting flips the signal from the secondary engine speed sensor.
	offset 1472 bit 15 */
	bool invertSecondaryTriggerSignal : 1 {};
	/**
	 * When enabled, this option cuts the fuel supply when the RPM limit is reached. Cutting fuel provides a smoother limiting action; however, it may lead to slightly higher combustion chamber temperatures since unburned fuel is not present to cool the combustion process.
	offset 1472 bit 16 */
	bool cutFuelOnHardLimit : 1 {};
	/**
	 * When selected, this option cuts the spark to limit RPM. Cutting spark can produce flames from the exhaust due to unburned fuel igniting in the exhaust system. Additionally, this unburned fuel can help cool the combustion chamber, which may be beneficial in high-performance applications.
	 * Be careful enabling this: some engines are known to self-disassemble their valvetrain with a spark cut. Fuel cut is much safer.
	offset 1472 bit 17 */
	bool cutSparkOnHardLimit : 1 {};
	/**
	offset 1472 bit 18 */
	bool launchFuelCutEnable : 1 {};
	/**
	 * Enables or disables ignition/spark cut during launch control.
	offset 1472 bit 19 */
	bool launchSparkCutEnable : 1 {};
	/**
	offset 1472 bit 20 */
	bool torqueReductionEnabled : 1 {};
	/**
	 * When we sync cam sensor is that first or second full engine revolution of the four stroke cycle?
	offset 1472 bit 21 */
	bool camSyncOnSecondCrankRevolution : 1 {};
	/**
	offset 1472 bit 22 */
	bool limitTorqueReductionTime : 1 {};
	/**
	 * Are you a developer troubleshooting TS over CAN ISO/TP?
	offset 1472 bit 23 */
	bool verboseIsoTp : 1 {};
	/**
	 * In this mode only trigger events go into engine sniffer and not coils/injectors etc
	offset 1472 bit 24 */
	bool engineSnifferFocusOnInputs : 1 {};
	/**
	offset 1472 bit 25 */
	bool twoStroke : 1 {};
	/**
	 * Where is your primary skipped wheel located?
	offset 1472 bit 26 */
	bool skippedWheelOnCam : 1 {};
	/**
	offset 1472 bit 27 */
	bool unusedBit_550_27 : 1 {};
	/**
	offset 1472 bit 28 */
	bool unusedBit_550_28 : 1 {};
	/**
	offset 1472 bit 29 */
	bool unusedBit_550_29 : 1 {};
	/**
	offset 1472 bit 30 */
	bool unusedBit_550_30 : 1 {};
	/**
	offset 1472 bit 31 */
	bool unusedBit_550_31 : 1 {};
	/**
	 * A/C button input
	 * offset 1476
	 */
	switch_input_pin_e acSwitch;
	/**
	 * offset 1478
	 */
	adc_channel_e vRefAdcChannel;
	/**
	 * Expected neutral position
	 * units: %
	 * offset 1479
	 */
	uint8_t etbNeutralPosition;
	/**
	 * See also idleRpmPid
	 * offset 1480
	 */
	idle_mode_e idleMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1481
	 */
	uint8_t alignmentFill_at_1481[3] = {};
	/**
	offset 1484 bit 0 */
	bool isInjectionEnabled : 1 {};
	/**
	offset 1484 bit 1 */
	bool isIgnitionEnabled : 1 {};
	/**
	 * When enabled if TPS is held above 95% no fuel is injected while cranking to clear excess fuel from the cylinders.
	offset 1484 bit 2 */
	bool isCylinderCleanupEnabled : 1 {};
	/**
	 * Should we use tables to vary tau/beta based on CLT/MAP, or just with fixed values?
	offset 1484 bit 3 */
	bool complexWallModel : 1 {};
	/**
	offset 1484 bit 4 */
	bool unusedBit_564_4 : 1 {};
	/**
	offset 1484 bit 5 */
	bool unusedBit_564_5 : 1 {};
	/**
	offset 1484 bit 6 */
	bool unusedBit_564_6 : 1 {};
	/**
	offset 1484 bit 7 */
	bool unusedBit_564_7 : 1 {};
	/**
	offset 1484 bit 8 */
	bool unusedBit_564_8 : 1 {};
	/**
	offset 1484 bit 9 */
	bool unusedBit_564_9 : 1 {};
	/**
	offset 1484 bit 10 */
	bool unusedBit_564_10 : 1 {};
	/**
	offset 1484 bit 11 */
	bool unusedBit_564_11 : 1 {};
	/**
	offset 1484 bit 12 */
	bool unusedBit_564_12 : 1 {};
	/**
	offset 1484 bit 13 */
	bool unusedBit_564_13 : 1 {};
	/**
	offset 1484 bit 14 */
	bool unusedBit_564_14 : 1 {};
	/**
	offset 1484 bit 15 */
	bool unusedBit_564_15 : 1 {};
	/**
	offset 1484 bit 16 */
	bool unusedBit_564_16 : 1 {};
	/**
	offset 1484 bit 17 */
	bool unusedBit_564_17 : 1 {};
	/**
	offset 1484 bit 18 */
	bool unusedBit_564_18 : 1 {};
	/**
	offset 1484 bit 19 */
	bool unusedBit_564_19 : 1 {};
	/**
	offset 1484 bit 20 */
	bool unusedBit_564_20 : 1 {};
	/**
	offset 1484 bit 21 */
	bool unusedBit_564_21 : 1 {};
	/**
	offset 1484 bit 22 */
	bool unusedBit_564_22 : 1 {};
	/**
	offset 1484 bit 23 */
	bool unusedBit_564_23 : 1 {};
	/**
	offset 1484 bit 24 */
	bool unusedBit_564_24 : 1 {};
	/**
	offset 1484 bit 25 */
	bool unusedBit_564_25 : 1 {};
	/**
	offset 1484 bit 26 */
	bool unusedBit_564_26 : 1 {};
	/**
	offset 1484 bit 27 */
	bool unusedBit_564_27 : 1 {};
	/**
	offset 1484 bit 28 */
	bool unusedBit_564_28 : 1 {};
	/**
	offset 1484 bit 29 */
	bool unusedBit_564_29 : 1 {};
	/**
	offset 1484 bit 30 */
	bool unusedBit_564_30 : 1 {};
	/**
	offset 1484 bit 31 */
	bool unusedBit_564_31 : 1 {};
	/**
	 * Per-cycle uses one full engine cycle for RPM (smooth, slow). First Order extrapolates from the last two cycle-averaged RPMs for responsive, noise-free RPM. Instant uses the last 90 degrees (most responsive, has combustion noise).
	 * offset 1488
	 */
	rpmUpdateMode_e rpmUpdateMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1489
	 */
	uint8_t alignmentFill_at_1489[3] = {};
	/**
	offset 1492 bit 0 */
	bool isMapAveragingEnabled : 1 {};
	/**
	 * This activates a separate ignition timing table for idle conditions, this can help idle stability by using ignition retard and advance either side of the desired idle speed. Extra advance at low idle speeds will prevent stalling and extra retard at high idle speeds can help reduce engine power and slow the idle speed.
	offset 1492 bit 1 */
	bool useSeparateAdvanceForIdle : 1 {};
	/**
	offset 1492 bit 2 */
	bool isWaveAnalyzerEnabled : 1 {};
	/**
	 * This activates a separate fuel table for Idle, this allows fine tuning of the idle fuelling.
	offset 1492 bit 3 */
	bool useSeparateVeForIdle : 1 {};
	/**
	 * Verbose info in console below engineSnifferRpmThreshold
	offset 1492 bit 4 */
	bool verboseTriggerSynchDetails : 1 {};
	/**
	offset 1492 bit 5 */
	bool hondaK : 1 {};
	/**
	 * This is needed if your coils are individually wired (COP) and you wish to use batch ignition (Wasted Spark).
	offset 1492 bit 6 */
	bool twoWireBatchIgnition : 1 {};
	/**
	 * Read MAP sensor on ECU start-up to use as baro value.
	offset 1492 bit 7 */
	bool useFixedBaroCorrFromMap : 1 {};
	/**
	 * In Constant mode, timing is automatically tapered to running as RPM increases.
	 * In Table mode, the "Cranking ignition advance" table is used directly.
	offset 1492 bit 8 */
	bool useSeparateAdvanceForCranking : 1 {};
	/**
	 * This enables the various ignition corrections during cranking (IAT, CLT and PID idle).
	 * You probably don't need this.
	offset 1492 bit 9 */
	bool useAdvanceCorrectionsForCranking : 1 {};
	/**
	 * Enable flex-fuel compensation for engine start. When on (and a flex fuel sensor is present) the cranking coolant multiplier and the priming pulse mass each come from a 2D table over coolant and ethanol % (crankingFuelFlexTable / primeFlexTable, 4-row ethanol axis) instead of their 1D coolant curves. When off, the 1D curves (crankingFuelCoef / primeValues) are used.
	offset 1492 bit 10 */
	bool flexCranking : 1 {};
	/**
	 * Enable flex-fuel transient fueling compensation (acceleration enrichment and wall wetting tau/beta) based on ethanol content and coolant temperature.
	offset 1492 bit 11 */
	bool flexFuelTransientComp : 1 {};
	/**
	 * This flag allows to use a special 'PID Multiplier' table (0.0-1.0) to compensate for nonlinear nature of IAC-RPM controller
	offset 1492 bit 12 */
	bool useIacPidMultTable : 1 {};
	/**
	offset 1492 bit 13 */
	bool isBoostControlEnabled : 1 {};
	/**
	 * Gradually interpolates the ignition timing from the base timing table value down to the target "Absolute Timing at Launch" value, starting from the beginning of the launch window.
	offset 1492 bit 14 */
	bool launchSmoothRetard : 1 {};
	/**
	 * Some engines are OK running semi-random sequential while other engine require phase synchronization
	offset 1492 bit 15 */
	bool isPhaseSyncRequiredForIgnition : 1 {};
	/**
	 * If enabled, use a curve for RPM limit (based on coolant temperature) instead of a constant value.
	offset 1492 bit 16 */
	bool useCltBasedRpmLimit : 1 {};
	/**
	 * If enabled, don't wait for engine start to heat O2 sensors.
	 * WARNING: this will reduce the life of your sensor, as condensation in the exhaust from a cold start can crack the sensing element.
	offset 1492 bit 17 */
	bool forceO2Heating : 1 {};
	/**
	 * If increased VVT duty cycle increases the indicated VVT angle, set this to 'advance'. If it decreases, set this to 'retard'. Most intake cams use 'advance', and most exhaust cams use 'retard'.
	offset 1492 bit 18 */
	bool invertVvtControlIntake : 1 {};
	/**
	 * If increased VVT duty cycle increases the indicated VVT angle, set this to 'advance'. If it decreases, set this to 'retard'. Most intake cams use 'advance', and most exhaust cams use 'retard'.
	offset 1492 bit 19 */
	bool invertVvtControlExhaust : 1 {};
	/**
	offset 1492 bit 20 */
	bool useBiQuadOnAuxSpeedSensors : 1 {};
	/**
	offset 1492 bit 21 */
	bool stepper_dc_use_two_wires : 1 {};
	/**
	offset 1492 bit 22 */
	bool watchOutForLinearTime : 1 {};
	/**
	 * Only write the SD log while trigger conditions are met (start/stop). Off = always log, the current behavior.
	offset 1492 bit 23 */
	bool sdCardConditionalLogging : 1 {};
	/**
	 * Compensated MAP: in Speed Density mode, normalize MAP by barometric pressure before it is used as a table load axis.
	 * MAP_ref = MAP / (baro / 101.325 kPa) feeds the VE lookup and the fuel/spark load axes, so the same table cells are hit regardless of altitude (WOT reads ~100 kPa at any elevation).
	 * The physical air mass calculation still uses actual MAP. Requires a barometric pressure sensor; without a valid baro reading no compensation is applied.
	 * Works together with the Barometric pressure correction table, which serves a different goal: this setting keeps table lookups stable across altitude, while the baro table multiplies fueling for exhaust-side scavenging effects. Either or both can be used.
	offset 1492 bit 24 */
	bool useCompensatedMap : 1 {};
	/**
	offset 1492 bit 25 */
	bool unusedBit_619_25 : 1 {};
	/**
	offset 1492 bit 26 */
	bool unusedBit_619_26 : 1 {};
	/**
	offset 1492 bit 27 */
	bool unusedBit_619_27 : 1 {};
	/**
	offset 1492 bit 28 */
	bool unusedBit_619_28 : 1 {};
	/**
	offset 1492 bit 29 */
	bool unusedBit_619_29 : 1 {};
	/**
	offset 1492 bit 30 */
	bool unusedBit_619_30 : 1 {};
	/**
	offset 1492 bit 31 */
	bool unusedBit_619_31 : 1 {};
	/**
	 * Start logging at/above this RPM
	 * units: rpm
	 * offset 1496
	 */
	uint16_t sdLogStartRpm;
	/**
	 * Stop logging below this RPM. Set below 'start' for hysteresis
	 * units: rpm
	 * offset 1498
	 */
	uint16_t sdLogStopRpm;
	/**
	 * Keep logging this many seconds after RPM drops below the stop threshold
	 * units: sec
	 * offset 1500
	 */
	uint8_t sdLogStopDelay;
	/**
	 * Also require TPS at/above this to start logging (0 = ignore)
	 * units: %
	 * offset 1501
	 */
	uint8_t sdLogMinTps;
	/**
	 * Also require MAP at/above this to start logging (0 = ignore)
	 * units: kPa
	 * offset 1502
	 */
	uint16_t sdLogMinMap;
	/**
	 * Also require vehicle speed at/above this to start logging (0 = ignore)
	 * units: kph
	 * offset 1504
	 */
	uint8_t sdLogMinVss;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1505
	 */
	uint8_t alignmentFill_at_1505[1] = {};
	/**
	 * Optional toggle button to start/stop logging (press on, press off)
	 * offset 1506
	 */
	switch_input_pin_e sdLogTriggerPin;
	/**
	 * offset 1508
	 */
	pin_input_mode_e sdLogTriggerPinMode;
	/**
	 * Only used by First Order RPM mode. Smoothing applied to the RPM rate of change (the slope used to extrapolate RPM between cycles) each engine cycle. 0% = fully raw: the slope is replaced by the latest cycle-to-cycle measurement every cycle (most responsive, but cycle-to-cycle combustion/measurement noise passes straight through). Higher % blends in more of the previous slope, smoothing that noise out at the cost of slower response to genuine acceleration/deceleration. Capped at 95%: at 100% the slope would never incorporate a new measurement and would freeze permanently.
	 * units: %
	 * offset 1509
	 */
	uint8_t rpmRateSmoothingPct;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1510
	 */
	uint8_t alignmentFill_at_1510[2] = {};
	/**
	 * units: count
	 * offset 1512
	 */
	uint32_t engineChartSize;
	/**
	 * units: mult
	 * offset 1516
	 */
	float turboSpeedSensorMultiplier;
	/**
	 * RPM added on top of the normal CLT-based idle target while A/C is enabled. Some cars need the extra speed to keep the AC efficient while idling.
	 * units: RPM
	 * offset 1520
	 */
	int16_t acIdleRpmAdder;
	/**
	 * set warningPeriod X
	 * units: seconds
	 * offset 1522
	 */
	int16_t warningPeriod;
	/**
	 * units: angle
	 * offset 1524
	 */
	float knockDetectionWindowStart;
	/**
	 * units: ms
	 * offset 1528
	 */
	float idleStepperReactionTime;
	/**
	 * units: count
	 * offset 1532
	 */
	int idleStepperTotalSteps;
	/**
	 * Pedal position to realize that we need to reduce torque when the trigger pin is triggered
	 * offset 1536
	 */
	int torqueReductionArmingApp;
	/**
	 * Reference Torque value
	 * units: Nm
	 * offset 1540
	 */
	float referenceTorqueForGenerator;
	/**
	 * kPa/psi value at which Reference Torque is archived
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 1544
	 */
	float referenceMapForGenerator;
	/**
	 * offset 1548
	 */
	float referenceVeForGenerator;
	/**
	 * Duration in ms or duty cycle depending on selected mode
	 * offset 1552
	 */
	float tachPulseDuractionMs;
	/**
	 * Length of time the deposited wall fuel takes to dissipate after the start of acceleration.
	 * units: Seconds
	 * offset 1556
	 */
	float wwaeTau;
	/**
	 * offset 1560
	 */
	pid_s alternatorControl;
	/**
	 * offset 1580
	 */
	pid_s etb;
	/**
	 * RPM range above upper limit for extra air taper
	 * units: RPM
	 * offset 1600
	 */
	int16_t airTaperRpmRange;
	/**
	 * offset 1602
	 */
	brain_input_pin_e turboSpeedSensorInputPin;
	/**
	 * Closed voltage for primary throttle position sensor
	 * offset 1604
	 */
	tps_limit_t tps2Min;
	/**
	 * Fully opened voltage for primary throttle position sensor
	 * offset 1606
	 */
	tps_limit_t tps2Max;
	/**
	 * See also startStopButtonPin
	 * offset 1608
	 */
	output_pin_e starterControlPin;
	/**
	 * offset 1610
	 */
	pin_input_mode_e startStopButtonMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1611
	 */
	uint8_t alignmentFill_at_1611[1] = {};
	/**
	 * offset 1612
	 */
	Gpio mc33816_flag0;
	/**
	 * offset 1614
	 */
	scaled_channel<uint16_t, 1000, 1> tachPulsePerRev;
	/**
	 * kPa/psi value which is too low to be true
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 1616
	 */
	float mapErrorDetectionTooLow;
	/**
	 * kPa/psi value which is too high to be true
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 1620
	 */
	float mapErrorDetectionTooHigh;
	/**
	 * How long to wait for the spark to fire before recharging the coil for another spark.
	 * units: ms
	 * offset 1624
	 */
	scaled_channel<uint16_t, 1000, 1> multisparkSparkDuration;
	/**
	 * This sets the dwell time for subsequent sparks. The main spark's dwell is set by the dwell table.
	 * units: ms
	 * offset 1626
	 */
	scaled_channel<uint16_t, 1000, 1> multisparkDwell;
	/**
	 * See cltIdleRpmBins
	 * offset 1628
	 */
	pid_s idleRpmPid;
	/**
	 * 0 = No fuel settling on port walls 1 = All the fuel settling on port walls setting this to 0 disables the wall wetting enrichment.
	 * units: Fraction
	 * offset 1648
	 */
	float wwaeBeta;
	/**
	 * See also EFI_CONSOLE_RX_BRAIN_PIN
	 * offset 1652
	 */
	Gpio binarySerialTxPin;
	/**
	 * offset 1654
	 */
	Gpio binarySerialRxPin;
	/**
	 * offset 1656
	 */
	Gpio auxValves[AUX_DIGITAL_VALVE_COUNT] = {};
	/**
	 * offset 1660
	 */
	switch_input_pin_e tcuUpshiftButtonPin;
	/**
	 * offset 1662
	 */
	switch_input_pin_e tcuDownshiftButtonPin;
	/**
	 * units: volts
	 * offset 1664
	 */
	float throttlePedalUpVoltage;
	/**
	 * Pedal in the floor
	 * units: volts
	 * offset 1668
	 */
	float throttlePedalWOTVoltage;
	/**
	 * on IGN voltage detection turn fuel pump on to build fuel pressure
	 * units: seconds
	 * offset 1672
	 */
	int16_t startUpFuelPumpDuration;
	/**
	 * larger value = larger intake manifold volume
	 * offset 1674
	 */
	uint16_t mafFilterParameter;
	/**
	 * If the RPM closer to target than this value, disable closed loop idle correction to prevent oscillation
	 * units: RPM
	 * offset 1676
	 */
	int16_t idlePidRpmDeadZone;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1678
	 */
	uint8_t alignmentFill_at_1678[2] = {};
	/**
	 * See Over/Undervoltage Shutdown/Retry bit in documentation
	offset 1680 bit 0 */
	bool mc33810DisableRecoveryMode : 1 {};
	/**
	offset 1680 bit 1 */
	bool mc33810Gpgd0Mode : 1 {};
	/**
	offset 1680 bit 2 */
	bool mc33810Gpgd1Mode : 1 {};
	/**
	offset 1680 bit 3 */
	bool mc33810Gpgd2Mode : 1 {};
	/**
	offset 1680 bit 4 */
	bool mc33810Gpgd3Mode : 1 {};
	/**
	 * Send out board statistics
	offset 1680 bit 5 */
	bool enableExtendedCanBroadcast : 1 {};
	/**
	 * global_can_data performance hack
	offset 1680 bit 6 */
	bool luaCanRxWorkaround : 1 {};
	/**
	offset 1680 bit 7 */
	bool flexSensorInverted : 1 {};
	/**
	offset 1680 bit 8 */
	bool useHardSkipInTraction : 1 {};
	/**
	 * Use a Lua gauge as a traction control multiplier input
	offset 1680 bit 9 */
	bool tractionControlUseLuaGauge : 1 {};
	/**
	 * Use Aux Speed 1 as one of speeds for wheel slip ratio?
	offset 1680 bit 10 */
	bool useAuxSpeedForSlipRatio : 1 {};
	/**
	 * VSS and Aux Speed 1 or Aux Speed 1 with Aux Speed 2?
	offset 1680 bit 11 */
	bool useVssAsSecondWheelSpeed : 1 {};
	/**
	offset 1680 bit 12 */
	bool is_enabled_spi_5 : 1 {};
	/**
	offset 1680 bit 13 */
	bool is_enabled_spi_6 : 1 {};
	/**
	 * AEM X-Series EGT gauge kit or rusEFI EGT sensor from Wideband controller
	offset 1680 bit 14 */
	bool enableAemXSeriesEgt : 1 {};
	/**
	offset 1680 bit 15 */
	bool startRequestPinInverted : 1 {};
	/**
	offset 1680 bit 16 */
	bool tcu_rangeSensorPulldown : 1 {};
	/**
	offset 1680 bit 17 */
	bool devBit01 : 1 {};
	/**
	 * Input speed sensor is the same physical sensor as the main VSS
	offset 1680 bit 18 */
	bool tcuInputSpeedSensorSharedWithVss : 1 {};
	/**
	offset 1680 bit 19 */
	bool devBit1 : 1 {};
	/**
	offset 1680 bit 20 */
	bool devBit2 : 1 {};
	/**
	offset 1680 bit 21 */
	bool devBit3 : 1 {};
	/**
	offset 1680 bit 22 */
	bool devBit4 : 1 {};
	/**
	offset 1680 bit 23 */
	bool devBit5 : 1 {};
	/**
	offset 1680 bit 24 */
	bool devBit6 : 1 {};
	/**
	offset 1680 bit 25 */
	bool devBit7 : 1 {};
	/**
	offset 1680 bit 26 */
	bool invertExhaustCamVVTSignal : 1 {};
	/**
	 * "Available via TS Plugin see https://rusefi.com/s/knock"
	offset 1680 bit 27 */
	bool enableKnockSpectrogram : 1 {};
	/**
	offset 1680 bit 28 */
	bool enableKnockSpectrogramFilter : 1 {};
	/**
	offset 1680 bit 29 */
	bool unusedBit_707_29 : 1 {};
	/**
	offset 1680 bit 30 */
	bool unusedBit_707_30 : 1 {};
	/**
	offset 1680 bit 31 */
	bool unusedBit_707_31 : 1 {};
	/**
	 * This value is an added for base idle value. Idle Value added when coasting and transitioning into idle.
	 * units: percent
	 * offset 1684
	 */
	int16_t iacByTpsTaper;
	/**
	 * offset 1686
	 */
	Gpio accelerometerCsPin;
	/**
	 * Below this speed, disable DFCO. Use this to prevent jerkiness from fuel enable/disable in low gears.
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 1688
	 */
	uint8_t coastingFuelCutVssLow;
	/**
	 * Above this speed, allow DFCO. Use this to prevent jerkiness from fuel enable/disable in low gears.
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 1689
	 */
	uint8_t coastingFuelCutVssHigh;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1690
	 */
	uint8_t alignmentFill_at_1690[2] = {};
	/**
	 * Maximum change delta of TPS percentage over the 'length'. Actual TPS change has to be above this value in order for TPS/TPS acceleration to kick in.
	 * units: roc
	 * offset 1692
	 */
	float tpsAccelEnrichmentThreshold;
	/**
	 * offset 1696
	 */
	brain_input_pin_e auxSpeedSensorInputPin[AUX_SPEED_SENSOR_COUNT] = {};
	/**
	 * Number of forward gears, shared by GearDetector (any count) and the TCU's Automatic mode (currently hardcoded to a 4-gear GEAR_1..GEAR_4 state machine, 5-10 gear support planned for a future release). Configured together with the per-gear ratios in the Speed Sensor dialog.
	 * offset 1700
	 */
	uint8_t totalGearsCount;
	/**
	 * Defines when fuel is injected relative to the intake valve opening. Options include End of Injection or other timing references.
	 * offset 1701
	 */
	InjectionTimingMode injectionTimingMode;
	/**
	 * See http://rusefi.com/s/debugmode
	 * offset 1702
	 */
	debug_mode_e debugMode;
	/**
	 * Additional idle % when fan #1 is active
	 * units: %
	 * offset 1703
	 */
	uint8_t fan1ExtraIdle;
	/**
	 * Band rate for primary TTL
	 * units: BPs
	 * offset 1704
	 */
	uint32_t uartConsoleSerialSpeed;
	/**
	 * units: volts
	 * offset 1708
	 */
	float throttlePedalSecondaryUpVoltage;
	/**
	 * Pedal in the floor
	 * units: volts
	 * offset 1712
	 */
	float throttlePedalSecondaryWOTVoltage;
	/**
	 * offset 1716
	 */
	can_baudrate_e canBaudRate;
	/**
	 * Override the Y axis (load) value used for the VE table.
	 * Advanced users only: If you aren't sure you need this, you probably don't need this.
	 * offset 1717
	 */
	ve_override_e veOverrideMode;
	/**
	 * offset 1718
	 */
	can_baudrate_e can2BaudRate;
	/**
	 * Override the Y axis (load) value used for the AFR table.
	 * Advanced users only: If you aren't sure you need this, you probably don't need this.
	 * offset 1719
	 */
	load_override_e afrOverrideMode;
	/**
	 * units: A
	 * offset 1720
	 */
	scaled_channel<uint8_t, 10, 1> mc33_hpfp_i_peak;
	/**
	 * units: A
	 * offset 1721
	 */
	scaled_channel<uint8_t, 10, 1> mc33_hpfp_i_hold;
	/**
	 * How long to deactivate power when hold current is reached before applying power again
	 * units: us
	 * offset 1722
	 */
	uint8_t mc33_hpfp_i_hold_off;
	/**
	 * Maximum amount of time the solenoid can be active before assuming a programming error
	 * units: ms
	 * offset 1723
	 */
	uint8_t mc33_hpfp_max_hold;
	/**
	 * Enable if DC-motor driver (H-bridge) inverts the signals (eg. RZ7899 on Hellen boards)
	offset 1724 bit 0 */
	bool stepperDcInvertedPins : 1 {};
	/**
	 * Allow OpenBLT on Primary CAN
	offset 1724 bit 1 */
	bool canOpenBLT : 1 {};
	/**
	 * Allow OpenBLT on Secondary CAN
	offset 1724 bit 2 */
	bool can2OpenBLT : 1 {};
	/**
	 * Select whether to configure injector flow in volumetric flow (default, cc/min) or mass flow (g/s).
	offset 1724 bit 3 */
	bool injectorFlowAsMassFlow : 1 {};
	/**
	offset 1724 bit 4 */
	bool boardUseCanTerminator : 1 {};
	/**
	offset 1724 bit 5 */
	bool kLineDoHondaSend : 1 {};
	/**
	 * ListenMode is about acknowledging CAN traffic on the protocol level. Different from canWriteEnabled
	offset 1724 bit 6 */
	bool can1ListenMode : 1 {};
	/**
	offset 1724 bit 7 */
	bool can2ListenMode : 1 {};
	/**
	offset 1724 bit 8 */
	bool unusedBit_740_8 : 1 {};
	/**
	offset 1724 bit 9 */
	bool unusedBit_740_9 : 1 {};
	/**
	offset 1724 bit 10 */
	bool unusedBit_740_10 : 1 {};
	/**
	offset 1724 bit 11 */
	bool unusedBit_740_11 : 1 {};
	/**
	offset 1724 bit 12 */
	bool unusedBit_740_12 : 1 {};
	/**
	offset 1724 bit 13 */
	bool unusedBit_740_13 : 1 {};
	/**
	offset 1724 bit 14 */
	bool unusedBit_740_14 : 1 {};
	/**
	offset 1724 bit 15 */
	bool unusedBit_740_15 : 1 {};
	/**
	offset 1724 bit 16 */
	bool unusedBit_740_16 : 1 {};
	/**
	offset 1724 bit 17 */
	bool unusedBit_740_17 : 1 {};
	/**
	offset 1724 bit 18 */
	bool unusedBit_740_18 : 1 {};
	/**
	offset 1724 bit 19 */
	bool unusedBit_740_19 : 1 {};
	/**
	offset 1724 bit 20 */
	bool unusedBit_740_20 : 1 {};
	/**
	offset 1724 bit 21 */
	bool unusedBit_740_21 : 1 {};
	/**
	offset 1724 bit 22 */
	bool unusedBit_740_22 : 1 {};
	/**
	offset 1724 bit 23 */
	bool unusedBit_740_23 : 1 {};
	/**
	offset 1724 bit 24 */
	bool unusedBit_740_24 : 1 {};
	/**
	offset 1724 bit 25 */
	bool unusedBit_740_25 : 1 {};
	/**
	offset 1724 bit 26 */
	bool unusedBit_740_26 : 1 {};
	/**
	offset 1724 bit 27 */
	bool unusedBit_740_27 : 1 {};
	/**
	offset 1724 bit 28 */
	bool unusedBit_740_28 : 1 {};
	/**
	offset 1724 bit 29 */
	bool unusedBit_740_29 : 1 {};
	/**
	offset 1724 bit 30 */
	bool unusedBit_740_30 : 1 {};
	/**
	offset 1724 bit 31 */
	bool unusedBit_740_31 : 1 {};
	/**
	 * Angle of tooth detection within engine phase cycle
	 * units: angle
	 * offset 1728
	 */
	uint16_t camDecoder2jzPosition;
	/**
	 * offset 1730
	 */
	mc33810maxDwellTimer_e mc33810maxDwellTimer;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1731
	 */
	uint8_t alignmentFill_at_1731[1] = {};
	/**
	 * Duration of each test pulse
	 * units: ms
	 * offset 1732
	 */
	scaled_channel<uint16_t, 100, 1> benchTestOnTime;
	/**
	 * units: %
	 * offset 1734
	 */
	uint8_t lambdaProtectionRestoreTps;
	/**
	 * units: %
	 * offset 1735
	 */
	scaled_channel<uint8_t, 1, 10> lambdaProtectionRestoreLoad;
	/**
	 * offset 1736
	 */
	pin_input_mode_e launchActivatePinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1737
	 */
	uint8_t alignmentFill_at_1737[1] = {};
	/**
	 * offset 1738
	 */
	Gpio can2TxPin;
	/**
	 * offset 1740
	 */
	Gpio can2RxPin;
	/**
	 * offset 1742
	 */
	pin_output_mode_e starterControlPinMode;
	/**
	 * offset 1743
	 */
	adc_channel_e wastegatePositionSensor;
	/**
	 * Override the Y axis (load) value used for the ignition table.
	 * Advanced users only: If you aren't sure you need this, you probably don't need this.
	 * offset 1744
	 */
	load_override_e ignOverrideMode;
	/**
	 * Select which fuel pressure sensor measures the pressure of the fuel at your injectors.
	 * offset 1745
	 */
	injector_pressure_type_e injectorPressureType;
	/**
	 * offset 1746
	 */
	output_pin_e hpfpValvePin;
	/**
	 * offset 1748
	 */
	pin_output_mode_e hpfpValvePinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1749
	 */
	uint8_t alignmentFill_at_1749[3] = {};
	/**
	 * Specifies the boost pressure allowed before triggering a cut. Setting this to 0 will DISABLE overboost cut.
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 1752
	 */
	float boostCutPressure;
	/**
	 * units: kg/h
	 * offset 1756
	 */
	scaled_channel<uint8_t, 1, 5> tchargeBins[16] = {};
	/**
	 * units: ratio
	 * offset 1772
	 */
	scaled_channel<uint8_t, 100, 1> tchargeValues[16] = {};
	/**
	 * Fixed timing, useful for TDC testing
	 * units: deg
	 * offset 1788
	 */
	float fixedTiming;
	/**
	 * MAP voltage for low point
	 * units: v
	 * offset 1792
	 */
	float mapLowValueVoltage;
	/**
	 * MAP voltage for low point
	 * units: v
	 * offset 1796
	 */
	float mapHighValueVoltage;
	/**
	 * EGO value correction
	 * units: value
	 * offset 1800
	 */
	float egoValueShift;
	/**
	 * VVT output solenoid pin for this cam
	 * offset 1804
	 */
	output_pin_e vvtPins[CAM_INPUTS_COUNT] = {};
	/**
	 * offset 1812
	 */
	scaled_channel<uint8_t, 200, 1> tChargeMinRpmMinTps;
	/**
	 * offset 1813
	 */
	scaled_channel<uint8_t, 200, 1> tChargeMinRpmMaxTps;
	/**
	 * offset 1814
	 */
	scaled_channel<uint8_t, 200, 1> tChargeMaxRpmMinTps;
	/**
	 * offset 1815
	 */
	scaled_channel<uint8_t, 200, 1> tChargeMaxRpmMaxTps;
	/**
	 * offset 1816
	 */
	pwm_freq_t vvtOutputFrequency;
	/**
	 * Minimim timing advance allowed. No spark on any cylinder will ever fire after this angle BTDC. For example, setting -10 here means no spark ever fires later than 10 deg ATDC. Note that this only concerns the primary spark: any trailing sparks or multispark may violate this constraint.
	 * units: deg BTDC
	 * offset 1818
	 */
	int8_t minimumIgnitionTiming;
	/**
	 * Maximum timing advance allowed. No spark on any cylinder will ever fire before this angle BTDC. For example, setting 45 here means no spark ever fires earlier than 45 deg BTDC
	 * units: deg BTDC
	 * offset 1819
	 */
	int8_t maximumIgnitionTiming;
	/**
	 * units: Hz
	 * offset 1820
	 */
	int alternatorPwmFrequency;
	/**
	 * offset 1824
	 */
	vvt_mode_e vvtMode[CAMS_PER_BANK] = {};
	/**
	 * Additional idle % when fan #2 is active
	 * units: %
	 * offset 1826
	 */
	uint8_t fan2ExtraIdle;
	/**
	 * Delay to allow fuel pressure to build before firing the priming pulse.
	 * units: sec
	 * offset 1827
	 */
	scaled_channel<uint8_t, 100, 1> primingDelay;
	/**
	 * offset 1828
	 */
	adc_channel_e auxAnalogInputs[LUA_ANALOG_INPUT_COUNT] = {};
	/**
	 * offset 1836
	 */
	output_pin_e trailingCoilPins[MAX_CYLINDER_COUNT] = {};
	/**
	 * offset 1860
	 */
	tle8888_mode_e tle8888mode;
	/**
	 * offset 1861
	 */
	pin_output_mode_e accelerometerCsPinMode;
	/**
	 * None = I have a MAP-referenced fuel pressure regulator
	 * Fixed rail pressure = I have an atmosphere-referenced fuel pressure regulator (returnless, typically)
	 * Sensed rail pressure = I have a fuel pressure sensor
	 *  HPFP fuel mass compensation = manual mode for GDI engines
	 * Manual Pressure Correction = same fuel pressure sensor/reference pressure math as Sensed Rail Pressure (Fuel: corr reference pressure stays valid), but no automatic sqrt(pressure) flow compensation is applied - tune the separate Manual Pressure Correction table (multiplicative, kPa axis) by hand instead
	 * offset 1862
	 */
	injector_compensation_mode_e injectorCompensationMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1863
	 */
	uint8_t alignmentFill_at_1863[1] = {};
	/**
	 * This is the pressure at which your injector flow is known.
	 * For example if your injectors flow 400cc/min at 3.5 bar, enter 350kpa/50.7psi here.
	 * This is gauge pressure/in reference to atmospheric.
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 1864
	 */
	float fuelReferencePressure;
	/**
	 * offset 1868
	 */
	ThermistorConf auxTempSensor1;
	/**
	 * offset 1900
	 */
	ThermistorConf auxTempSensor2;
	/**
	 * units: Deg
	 * offset 1932
	 */
	int16_t knockSamplingDuration;
	/**
	 * units: Hz
	 * offset 1934
	 */
	int16_t etbFreq;
	/**
	 * offset 1936
	 */
	pid_s etbWastegatePid;
	/**
	 * For micro-stepping, make sure that PWM frequency (etbFreq) is high enough
	 * offset 1956
	 */
	stepper_num_micro_steps_e stepperNumMicroSteps;
	/**
	 * Use to limit the current when the stepper motor is idle, not moving (100% = no limit)
	 * units: %
	 * offset 1957
	 */
	uint8_t stepperMinDutyCycle;
	/**
	 * Use to limit the max.current through the stepper motor (100% = no limit)
	 * units: %
	 * offset 1958
	 */
	uint8_t stepperMaxDutyCycle;
	/**
	 * offset 1959
	 */
	spi_device_e sdCardSpiDevice;
	/**
	 * per-cylinder ignition and fueling timing correction for uneven engines
	 * units: deg
	 * offset 1960
	 */
	angle_t timing_offset_cylinder[MAX_CYLINDER_COUNT] = {};
	/**
	 * units: seconds
	 * offset 2008
	 */
	float idlePidActivationTime;
	/**
	 * Minimum coolant temperature to activate VVT
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 2012
	 */
	int16_t vvtControlMinClt;
	/**
	 * offset 2014
	 */
	pin_mode_e spi1SckMode;
	/**
	 * Modes count be used for 3v<>5v integration using pull-ups/pull-downs etc.
	 * offset 2015
	 */
	pin_mode_e spi1MosiMode;
	/**
	 * offset 2016
	 */
	pin_mode_e spi1MisoMode;
	/**
	 * offset 2017
	 */
	pin_mode_e spi2SckMode;
	/**
	 * offset 2018
	 */
	pin_mode_e spi2MosiMode;
	/**
	 * offset 2019
	 */
	pin_mode_e spi2MisoMode;
	/**
	 * offset 2020
	 */
	pin_mode_e spi3SckMode;
	/**
	 * offset 2021
	 */
	pin_mode_e spi3MosiMode;
	/**
	 * offset 2022
	 */
	pin_mode_e spi3MisoMode;
	/**
	 * offset 2023
	 */
	pin_output_mode_e stepperEnablePinMode;
	/**
	 * ResetB
	 * offset 2024
	 */
	Gpio mc33816_rstb;
	/**
	 * offset 2026
	 */
	Gpio mc33816_driven;
	/**
	 * Brake pedal switch
	 * offset 2028
	 */
	switch_input_pin_e brakePedalPin;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2030
	 */
	uint8_t alignmentFill_at_2030[2] = {};
	/**
	 * VVT output PID
	 * TODO: rename to vvtPid
	 * offset 2032
	 */
	pid_s auxPid[CAMS_PER_BANK] = {};
	/**
	 * VVT intake cam PID: iTerm min value
	 * offset 2072
	 */
	int16_t vvtIntake_iTermMin;
	/**
	 * VVT intake cam PID: iTerm max value
	 * offset 2074
	 */
	int16_t vvtIntake_iTermMax;
	/**
	 * VVT exhaust cam PID: iTerm min value
	 * offset 2076
	 */
	int16_t vvtExhaust_iTermMin;
	/**
	 * VVT exhaust cam PID: iTerm max value
	 * offset 2078
	 */
	int16_t vvtExhaust_iTermMax;
	/**
	 * offset 2080
	 */
	float injectorCorrectionPolynomial[8] = {};
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 2112
	 */
	scaled_channel<int16_t, 1, 1> primeBins[PRIME_CURVE_COUNT] = {};
	/**
	 * offset 2128
	 */
	linear_sensor_s oilPressure;
	/**
	 * offset 2148
	 */
	spi_device_e accelerometerSpiDevice;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2149
	 */
	uint8_t alignmentFill_at_2149[1] = {};
	/**
	 * offset 2150
	 */
	Gpio stepperEnablePin;
	/**
	 * offset 2152
	 */
	Gpio tle8888_cs;
	/**
	 * offset 2154
	 */
	pin_output_mode_e tle8888_csPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2155
	 */
	uint8_t alignmentFill_at_2155[1] = {};
	/**
	 * offset 2156
	 */
	Gpio mc33816_cs;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2158
	 */
	uint8_t alignmentFill_at_2158[2] = {};
	/**
	 * units: hz
	 * offset 2160
	 */
	float auxFrequencyFilter;
	/**
	 * offset 2164
	 */
	sent_input_pin_e sentInputPins[SENT_INPUT_COUNT] = {};
	/**
	 * This sets the RPM above which fuel cut is active.
	 * units: rpm
	 * offset 2166
	 */
	int16_t coastingFuelCutRpmHigh;
	/**
	 * This sets the RPM below which fuel cut is deactivated, this prevents jerking or issues transitioning to idle
	 * units: rpm
	 * offset 2168
	 */
	int16_t coastingFuelCutRpmLow;
	/**
	 * Throttle position below which fuel cut is active. With an electronic throttle enabled, this checks against pedal position.
	 * units: %
	 * offset 2170
	 */
	int16_t coastingFuelCutTps;
	/**
	 * Fuel cutoff is disabled when the engine is cold.
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 2172
	 */
	int16_t coastingFuelCutClt;
	/**
	 * Increases PID reaction for RPM<target by adding extra percent to PID-error
	 * units: %
	 * offset 2174
	 */
	int16_t pidExtraForLowRpm;
	/**
	 * MAP value above which fuel injection is re-enabled.
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 2176
	 */
	int16_t coastingFuelCutMap;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2178
	 */
	uint8_t alignmentFill_at_2178[2] = {};
	/**
	 * offset 2180
	 */
	linear_sensor_s highPressureFuel;
	/**
	 * offset 2200
	 */
	linear_sensor_s lowPressureFuel;
	/**
	 * offset 2220
	 */
	gppwm_note_t scriptCurveName[SCRIPT_CURVE_COUNT] = {};
	/**
	 * offset 2316
	 */
	gppwm_note_t scriptTableName[SCRIPT_TABLE_COUNT] = {};
	/**
	 * offset 2380
	 */
	gppwm_note_t scriptSettingName[SCRIPT_SETTING_COUNT] = {};
	/**
	 * Heat transfer coefficient at zero flow.
	 * 0 means the air charge is fully heated to the same temperature as CLT.
	 * 1 means the air charge gains no heat, and enters the cylinder at the temperature measured by IAT.
	 * offset 2508
	 */
	float tChargeAirCoefMin;
	/**
	 * Heat transfer coefficient at high flow, as defined by "max air flow".
	 * 0 means the air charge is fully heated to the same temperature as CLT.
	 * 1 means the air charge gains no heat, and enters the cylinder at the temperature measured by IAT.
	 * offset 2512
	 */
	float tChargeAirCoefMax;
	/**
	 * High flow point for heat transfer estimation.
	 * Set this to perhaps 50-75% of your maximum airflow at wide open throttle.
	 * units: kg/h
	 * offset 2516
	 */
	float tChargeAirFlowMax;
	/**
	 * Maximum allowed rate of increase allowed for the estimated charge temperature
	 * units: deg/sec
	 * offset 2520
	 */
	float tChargeAirIncrLimit;
	/**
	 * Maximum allowed rate of decrease allowed for the estimated charge temperature
	 * units: deg/sec
	 * offset 2524
	 */
	float tChargeAirDecrLimit;
	/**
	 * iTerm min value
	 * offset 2528
	 */
	int16_t etb_iTermMin;
	/**
	 * iTerm max value
	 * offset 2530
	 */
	int16_t etb_iTermMax;
	/**
	 * See useIdleTimingPidControl
	 * offset 2532
	 */
	pid_s idleTimingPid;
	/**
	 * When entering idle, and the PID settings are aggressive, it's good to make a soft entry upon entering closed loop
	 * offset 2552
	 */
	float idleTimingSoftEntryTime;
	/**
	 * offset 2556
	 */
	pin_input_mode_e torqueReductionTriggerPinMode;
	/**
	 * offset 2557
	 */
	torqueReductionActivationMode_e torqueReductionActivationMode;
	/**
	 * A delay in cycles between fuel-enrich. portions
	 * units: cycles
	 * offset 2558
	 */
	int16_t tpsAccelFractionPeriod;
	/**
	 * A fraction divisor: 1 or less = entire portion at once, or split into diminishing fractions
	 * units: coef
	 * offset 2560
	 */
	float tpsAccelFractionDivisor;
	/**
	 * offset 2564
	 */
	spi_device_e tle8888spiDevice;
	/**
	 * offset 2565
	 */
	spi_device_e mc33816spiDevice;
	/**
	 * iTerm min value
	 * offset 2566
	 */
	int16_t idlerpmpid_iTermMin;
	/**
	 * offset 2568
	 */
	spi_device_e tle6240spiDevice;
	/**
	 * Stoichiometric ratio for your primary fuel. When Flex Fuel is enabled, this value is used when the Flex Fuel sensor indicates E0.
	 * E0 = 14.7
	 * E10 = 14.1
	 * E85 = 9.9
	 * E100 = 9.0
	 * units: :1
	 * offset 2569
	 */
	scaled_channel<uint8_t, 10, 1> stoichRatioPrimary;
	/**
	 * iTerm max value
	 * offset 2570
	 */
	int16_t idlerpmpid_iTermMax;
	/**
	 * This sets the range of the idle control on the ETB. At 100% idle position, the value specified here sets the base ETB position. Can also be interpreted as the maximum allowed TPS% Opening for Idle Control.
	 * units: %
	 * offset 2572
	 */
	float etbIdleThrottleRange;
	/**
	 * Select which fuel correction bank this cylinder belongs to. Group cylinders that share the same O2 sensor
	 * offset 2576
	 */
	uint8_t cylinderBankSelect[MAX_CYLINDER_COUNT] = {};
	/**
	 * units: mg
	 * offset 2588
	 */
	scaled_channel<uint8_t, 1, 5> primeValues[PRIME_CURVE_COUNT] = {};
	/**
	 * Ethanol % axis (Y) for primeFlexTable.
	 * units: %
	 * offset 2596
	 */
	uint8_t primeFlexBins[PRIME_FLEX_SIZE] = {};
	/**
	 * Priming pulse fuel mass as a function of coolant (X axis, shared primeBins) and ethanol % (Y axis, primeFlexBins). Used instead of primeValues when flexCranking is enabled and a flex sensor is present.
	 * units: mg
	 * offset 2600
	 */
	scaled_channel<uint8_t, 1, 5> primeFlexTable[PRIME_FLEX_SIZE][PRIME_CURVE_COUNT] = {};
	/**
	 * Trigger comparator center point voltage
	 * units: V
	 * offset 2632
	 */
	scaled_channel<uint8_t, 50, 1> triggerCompCenterVolt;
	/**
	 * Trigger comparator hysteresis voltage (Min)
	 * units: V
	 * offset 2633
	 */
	scaled_channel<uint8_t, 50, 1> triggerCompHystMin;
	/**
	 * Trigger comparator hysteresis voltage (Max)
	 * units: V
	 * offset 2634
	 */
	scaled_channel<uint8_t, 50, 1> triggerCompHystMax;
	/**
	 * VR-sensor saturation RPM
	 * units: RPM
	 * offset 2635
	 */
	scaled_channel<uint8_t, 1, 50> triggerCompSensorSatRpm;
	/**
	 * units: ratio
	 * offset 2636
	 */
	scaled_channel<uint16_t, 100, 1> tractionControlSlipBins[TRACTION_CONTROL_ETB_DROP_SLIP_SIZE] = {};
	/**
	 * units: RPM
	 * offset 2648
	 */
	uint8_t tractionControlSpeedBins[TRACTION_CONTROL_ETB_DROP_SPEED_SIZE] = {};
	/**
	 * offset 2654
	 */
	can_vss_nbc_e canVssNbcType;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2655
	 */
	uint8_t alignmentFill_at_2655[1] = {};
	/**
	 * offset 2656
	 */
	gppwm_channel gppwm[GPPWM_CHANNELS] = {};
	/**
	 * Boost Current
	 * units: mA
	 * offset 3088
	 */
	uint16_t mc33_i_boost;
	/**
	 * Peak Current
	 * units: mA
	 * offset 3090
	 */
	uint16_t mc33_i_peak;
	/**
	 * Hold Current
	 * units: mA
	 * offset 3092
	 */
	uint16_t mc33_i_hold;
	/**
	 * Maximum allowed boost phase time. If the injector current doesn't reach the threshold before this time elapses, it is assumed that the injector is missing or has failed open circuit.
	 * units: us
	 * offset 3094
	 */
	uint16_t mc33_t_max_boost;
	/**
	 * units: us
	 * offset 3096
	 */
	uint16_t mc33_t_peak_off;
	/**
	 * Peak phase duration
	 * units: us
	 * offset 3098
	 */
	uint16_t mc33_t_peak_tot;
	/**
	 * units: us
	 * offset 3100
	 */
	uint16_t mc33_t_bypass;
	/**
	 * units: us
	 * offset 3102
	 */
	uint16_t mc33_t_hold_off;
	/**
	 * Hold phase duration
	 * units: us
	 * offset 3104
	 */
	uint16_t mc33_t_hold_tot;
	/**
	 * offset 3106
	 */
	pin_input_mode_e tcuUpshiftButtonPinMode;
	/**
	 * offset 3107
	 */
	pin_input_mode_e tcuDownshiftButtonPinMode;
	/**
	 * offset 3108
	 */
	pin_input_mode_e acSwitchMode;
	/**
	 * offset 3109
	 */
	pin_output_mode_e tcu_solenoid_mode[TCU_SOLENOID_COUNT] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3115
	 */
	uint8_t alignmentFill_at_3115[1] = {};
	/**
	 * units: ratio
	 * offset 3116
	 */
	float triggerGapOverrideFrom[GAP_TRACKING_LENGTH] = {};
	/**
	 * units: ratio
	 * offset 3188
	 */
	float triggerGapOverrideTo[GAP_TRACKING_LENGTH] = {};
	/**
	 * Below this RPM, use camshaft information to synchronize the crank's position for full sequential operation. Use this if your cam sensor does weird things at high RPM. Set to 0 to disable, and always use cam to help sync crank.
	 * units: rpm
	 * offset 3260
	 */
	scaled_channel<uint8_t, 1, 50> maxCamPhaseResolveRpm;
	/**
	 * Delay before cutting fuel. Set to 0 to cut immediately with no delay. May cause rumbles and pops out of your exhaust...
	 * units: sec
	 * offset 3261
	 */
	scaled_channel<uint8_t, 10, 1> dfcoDelay;
	/**
	 * Delay before engaging the AC compressor. Set to 0 to engage immediately with no delay. Use this to prevent bogging at idle when AC engages.
	 * units: sec
	 * offset 3262
	 */
	scaled_channel<uint8_t, 10, 1> acDelay;
	/**
	 * offset 3263
	 */
	tChargeMode_e tChargeMode;
	/**
	 * units: mg
	 * offset 3264
	 */
	scaled_channel<uint16_t, 1000, 1> fordInjectorSmallPulseBreakPoint;
	/**
	 * Threshold in ETB error (target vs. actual) above which the jam timer is started. If the timer reaches the time specified in the jam detection timeout period, the throttle is considered jammed, and engine operation limited.
	 * units: %
	 * offset 3266
	 */
	uint8_t etbJamDetectThreshold;
	/**
	 * units: lobes/cam
	 * offset 3267
	 */
	uint8_t hpfpCamLobes;
	/**
	 * offset 3268
	 */
	hpfp_cam_e hpfpCam;
	/**
	 * Low engine speed for A/C. Larger engines can survive lower values
	 * units: RPM
	 * offset 3269
	 */
	scaled_channel<int8_t, 1, 10> acLowRpmLimit;
	/**
	 * If the requested activation time is below this angle, don't bother running the pump
	 * units: deg
	 * offset 3270
	 */
	uint8_t hpfpMinAngle;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3271
	 */
	uint8_t alignmentFill_at_3271[1] = {};
	/**
	 * Size of the pump chamber in cc. Typical Bosch HDP5 has a 9.0mm diameter, typical BMW N* stroke is 4.4mm.
	 * units: cc
	 * offset 3272
	 */
	scaled_channel<uint16_t, 1000, 1> hpfpPumpVolume;
	/**
	 * How long to keep the valve activated (in order to allow the pump to build pressure and keep the valve open on its own)
	 * https://rusefi.com/forum/viewtopic.php?t=2192
	 * units: deg
	 * offset 3274
	 */
	uint8_t hpfpActivationAngle;
	/**
	 * offset 3275
	 */
	uint8_t issFilterReciprocal;
	/**
	 * units: %/kPa
	 * offset 3276
	 */
	scaled_channel<uint16_t, 1000, 1> hpfpPidP;
	/**
	 * units: %/kPa/lobe
	 * offset 3278
	 */
	scaled_channel<uint16_t, 100000, 1> hpfpPidI;
	/**
	 * iTerm min value
	 * offset 3280
	 */
	int16_t hpfpPid_iTermMin;
	/**
	 * iTerm max value
	 * offset 3282
	 */
	int16_t hpfpPid_iTermMax;
	/**
	 * The fastest rate the target pressure can be reduced by. This is because HPFP have no way to bleed off pressure other than injecting fuel.
	 * units: kPa/s
	 * offset 3284
	 */
	uint16_t hpfpTargetDecay;
	/**
	 * offset 3286
	 */
	output_pin_e stepper_raw_output[4] = {};
	/**
	 * units: ratio
	 * offset 3294
	 */
	scaled_channel<uint16_t, 100, 1> gearRatio[TCU_GEAR_COUNT] = {};
	/**
	 * We need to give engine time to build oil pressure without diverting it to VVT
	 * units: ms
	 * offset 3314
	 */
	uint16_t vvtActivationDelayMs;
	/**
	 * offset 3316
	 */
	GearControllerMode gearControllerMode;
	/**
	 * offset 3317
	 */
	TransmissionControllerMode transmissionControllerMode;
	/**
	 * During revolution where ACR should be disabled at what specific angle to disengage
	 * units: deg
	 * offset 3318
	 */
	uint16_t acrDisablePhase;
	/**
	 * offset 3320
	 */
	linear_sensor_s auxLinear1;
	/**
	 * offset 3340
	 */
	linear_sensor_s auxLinear2;
	/**
	 * offset 3360
	 */
	output_pin_e tcu_tcc_onoff_solenoid;
	/**
	 * offset 3362
	 */
	pin_output_mode_e tcu_tcc_onoff_solenoid_mode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3363
	 */
	uint8_t alignmentFill_at_3363[1] = {};
	/**
	 * offset 3364
	 */
	output_pin_e tcu_tcc_pwm_solenoid;
	/**
	 * offset 3366
	 */
	pin_output_mode_e tcu_tcc_pwm_solenoid_mode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3367
	 */
	uint8_t alignmentFill_at_3367[1] = {};
	/**
	 * offset 3368
	 */
	pwm_freq_t tcu_tcc_pwm_solenoid_freq;
	/**
	 * offset 3370
	 */
	output_pin_e tcu_pc_solenoid_pin;
	/**
	 * offset 3372
	 */
	pin_output_mode_e tcu_pc_solenoid_pin_mode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3373
	 */
	uint8_t alignmentFill_at_3373[1] = {};
	/**
	 * offset 3374
	 */
	pwm_freq_t tcu_pc_solenoid_freq;
	/**
	 * offset 3376
	 */
	output_pin_e tcu_32_solenoid_pin;
	/**
	 * offset 3378
	 */
	pin_output_mode_e tcu_32_solenoid_pin_mode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3379
	 */
	uint8_t alignmentFill_at_3379[1] = {};
	/**
	 * offset 3380
	 */
	pwm_freq_t tcu_32_solenoid_freq;
	/**
	 * offset 3382
	 */
	output_pin_e acrPin2;
	/**
	 * Set a minimum allowed target position to avoid slamming/driving against the hard mechanical stop in the throttle.
	 * units: %
	 * offset 3384
	 */
	scaled_channel<uint8_t, 10, 1> etbMinimumPosition;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3385
	 */
	uint8_t alignmentFill_at_3385[1] = {};
	/**
	 * offset 3386
	 */
	uint16_t tuneHidingKey;
	/**
	 * Individual characters are accessible using vin(index) Lua function
	 * offset 3388
	 */
	vin_number_t vinNumber;
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 3405
	 */
	int8_t torqueReductionActivationTemperature;
	/**
	 * Absolute: Sensor reads ~100 kPa (14.7 psi) with engine off and no fuel pressure.
	 * Gauge: Sensor reads 0 with engine off and no fuel pressure (most common standard 0-10 bar / 0-150 psi sensors).
	 * Differential: Sensor is connected to intake manifold vacuum and measures pressure difference directly.
	 * offset 3406
	 */
	fuel_pressure_sensor_mode_e fuelPressureSensorMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3407
	 */
	uint8_t alignmentFill_at_3407[1] = {};
	/**
	 * offset 3408
	 */
	switch_input_pin_e luaDigitalInputPins[LUA_DIGITAL_INPUT_COUNT] = {};
	/**
	 * units: rpm
	 * offset 3424
	 */
	int16_t ALSMinRPM;
	/**
	 * units: rpm
	 * offset 3426
	 */
	int16_t ALSMaxRPM;
	/**
	 * units: sec
	 * offset 3428
	 */
	int16_t ALSMaxDuration;
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 3430
	 */
	int8_t ALSMinCLT;
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 3431
	 */
	int8_t ALSMaxCLT;
	/**
	 * offset 3432
	 */
	uint8_t alsMinTimeBetween;
	/**
	 * offset 3433
	 */
	uint8_t alsEtbPosition;
	/**
	 * units: %
	 * offset 3434
	 */
	uint8_t acRelayAlternatorDutyAdder;
	/**
	 * If you have digital SENT TPS sensor please select type. For analog TPS leave None
	 * offset 3435
	 */
	SentEtbType sentEtbType;
	/**
	 * offset 3436
	 */
	uint16_t customSentTpsMin;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3438
	 */
	uint8_t alignmentFill_at_3438[2] = {};
	/**
	 * units: %
	 * offset 3440
	 */
	int ALSIdleAdd;
	/**
	 * units: %
	 * offset 3444
	 */
	int ALSEtbAdd;
	/**
	 * offset 3448
	 */
	float ALSSkipRatio;
	/**
	 * Hysterisis: if Pressure High Disable is 240kpa, and acPressureEnableHyst is 20, when the ECU sees 240kpa, A/C will be disabled, and stay disabled until 240-20=220kpa is reached
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 3452
	 */
	scaled_channel<uint8_t, 2, 1> acPressureEnableHyst;
	/**
	 * offset 3453
	 */
	pin_input_mode_e ALSActivatePinMode;
	/**
	 * For Ford TPS, use 53%. For Toyota ETCS-i, use ~65%
	 * units: %
	 * offset 3454
	 */
	scaled_channel<uint8_t, 2, 1> tpsSecondaryMaximum;
	/**
	 * For Toyota ETCS-i, use ~69%
	 * units: %
	 * offset 3455
	 */
	scaled_channel<uint8_t, 2, 1> ppsSecondaryMaximum;
	/**
	 * offset 3456
	 */
	pin_input_mode_e luaDigitalInputPinModes[LUA_DIGITAL_INPUT_COUNT] = {};
	/**
	 * offset 3464
	 */
	uint16_t customSentTpsMax;
	/**
	 * offset 3466
	 */
	uint16_t kLineBaudRate;
	/**
	 * offset 3468
	 */
	CanGpioType canGpioType;
	/**
	 * offset 3469
	 */
	UiMode uiMode;
	/**
	 * Crank angle ATDC of first lobe peak
	 * units: deg
	 * offset 3470
	 */
	int16_t hpfpPeakPos;
	/**
	 * units: us
	 * offset 3472
	 */
	int16_t kLinePeriodUs;
	/**
	 * Degrees of timing REMOVED from actual timing during soft RPM limit window
	 * units: deg
	 * offset 3474
	 */
	scaled_channel<uint8_t, 5, 1> rpmSoftLimitTimingRetard;
	/**
	 * % of fuel ADDED during window
	 * units: %
	 * offset 3475
	 */
	scaled_channel<uint8_t, 5, 1> rpmSoftLimitFuelAdded;
	/**
	 * Sets a buffer below the RPM hard limit, helping avoid rapid cycling of cut actions by defining a range within which RPM must drop before cut actions are re-enabled.
	 * Hysterisis: if the hard limit is 7200rpm and rpmHardLimitHyst is 200rpm, then when the ECU sees 7200rpm, fuel/ign will cut, and stay cut until 7000rpm (7200-200) is reached
	 * units: RPM
	 * offset 3476
	 */
	scaled_channel<uint8_t, 1, 10> rpmHardLimitHyst;
	/**
	 * Width of the RPM window below the RPM hard limit over which the Soft RPM Limit's timing retard and fuel added ramp from zero up to their full configured value at the hard limit. Independent of RPM limit hysteresis, which only controls when cut actions re-enable.
	 * units: RPM
	 * offset 3477
	 */
	scaled_channel<uint8_t, 1, 10> rpmSoftLimitRange;
	/**
	 * Time between bench test pulses
	 * units: ms
	 * offset 3478
	 */
	scaled_channel<uint16_t, 10, 1> benchTestOffTime;
	/**
	 * Defines a pressure range below the cut limit at which boost can resume, providing smoother control over boost cut actions.
	 * For example: if hard cut is 240kpa, and boost cut hysteresis is 20, when the ECU sees 240kpa, fuel/ign will cut, and stay cut until 240-20=220kpa is reached
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 3480
	 */
	scaled_channel<uint8_t, 2, 1> boostCutPressureHyst;
	/**
	 * Boost duty cycle modified by gear
	 * units: %
	 * offset 3481
	 */
	scaled_channel<int8_t, 2, 1> gearBasedOpenLoopBoostAdder[TCU_GEAR_COUNT] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3491
	 */
	uint8_t alignmentFill_at_3491[1] = {};
	/**
	 * How many test bench pulses do you want
	 * offset 3492
	 */
	uint32_t benchTestCount;
	/**
	 * How long initial idle adder is held before starting to decay.
	 * units: seconds
	 * offset 3496
	 */
	scaled_channel<uint8_t, 10, 1> iacByTpsHoldTime;
	/**
	 * How long it takes to remove initial IAC adder to return to normal idle.
	 * units: seconds
	 * offset 3497
	 */
	scaled_channel<uint8_t, 10, 1> iacByTpsDecayTime;
	/**
	 * offset 3498
	 */
	switch_input_pin_e tcu_rangeInput[RANGE_INPUT_COUNT] = {};
	/**
	 * offset 3510
	 */
	pin_input_mode_e tcu_rangeInputMode[RANGE_INPUT_COUNT] = {};
	/**
	 * Scale the reported vehicle speed value from CAN. Example: Parameter set to 1.1, CAN VSS reports 50kph, ECU will report 55kph instead.
	 * units: ratio
	 * offset 3516
	 */
	scaled_channel<uint16_t, 10000, 1> canVssScaling;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3518
	 */
	uint8_t alignmentFill_at_3518[2] = {};
	/**
	 * offset 3520
	 */
	ThermistorConf oilTempSensor;
	/**
	 * offset 3552
	 */
	ThermistorConf fuelTempSensor;
	/**
	 * offset 3584
	 */
	ThermistorConf ambientTempSensor;
	/**
	 * offset 3616
	 */
	ThermistorConf compressorDischargeTemperature;
	/**
	 * Cylinder head temperature (CHT) sensor
	 * offset 3648
	 */
	ThermistorConf chtSensor;
	/**
	 * EOT-from-CHT/IAT estimation: base delta. The head/block runs this many deg C hotter than the oil at zero IAT influence.
	 * units: deg C
	 * offset 3680
	 */
	float eotEstK0;
	/**
	 * EOT-from-CHT/IAT estimation: CHT coefficient. This fraction of the current CHT reading is added to the delta (hotter head = harder to reject heat into the oil).
	 * units: ratio
	 * offset 3684
	 */
	float eotEstK1;
	/**
	 * EOT-from-CHT/IAT estimation: IAT coefficient. Additional deg C of delta per deg C of intake air temperature (cooler incoming air increases the delta).
	 * units: ratio
	 * offset 3688
	 */
	float eotEstK2;
	/**
	 * EOT-from-CHT/IAT estimation: oil pressure coefficient. Additional deg C of delta per kPa of oil pressure. Higher pressure means more oil flow and better convective heat transfer, typically shrinking the delta (negative value).
	 * units: deg C/kPa
	 * offset 3692
	 */
	float eotEstK3;
	/**
	 * EOT-from-CHT/IAT estimation: heating time factor in seconds. Short blips barely register while sustained load integrates fully.
	 * units: s
	 * offset 3696
	 */
	float eotEstTauHeat;
	/**
	 * EOT-from-CHT/IAT estimation: cooling time factor in seconds. How long it takes for the delta to decay after load drops. Usually longer than heating because oil retains heat after load disappears.
	 * units: s
	 * offset 3700
	 */
	float eotEstTauCool;
	/**
	 * Fallback EOT value used when the CHT sensor or oil pressure reads invalid.
	 * units: deg C
	 * offset 3704
	 */
	int16_t eotEstFallbackEot;
	/**
	 * offset 3706
	 */
	int16_t pad_eot_reserved;
	/**
	 * Place the sensor before the throttle, but after any turbocharger/supercharger and intercoolers if fitted. Uses the same calibration as the MAP sensor.
	 * offset 3708
	 */
	adc_channel_e throttleInletPressureChannel;
	/**
	 * Place the sensor after the turbocharger/supercharger, but before any intercoolers if fitted. Uses the same calibration as the MAP sensor.
	 * offset 3709
	 */
	adc_channel_e compressorDischargePressureChannel;
	/**
	 * offset 3710
	 */
	Gpio dacOutputPins[DAC_OUTPUT_COUNT] = {};
	/**
	 * offset 3714
	 */
	output_pin_e speedometerOutputPin;
	/**
	 * Number of speedometer pulses per kilometer travelled.
	 * offset 3716
	 */
	uint16_t speedometerPulsePerKm;
	/**
	 * offset 3718
	 */
	uint8_t simulatorCamPosition[CAM_INPUTS_COUNT] = {};
	/**
	 * offset 3722
	 */
	adc_channel_e ignKeyAdcChannel;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3723
	 */
	uint8_t alignmentFill_at_3723[1] = {};
	/**
	 * offset 3724
	 */
	float ignKeyAdcDivider;
	/**
	 * offset 3728
	 */
	pin_mode_e spi6MisoMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3729
	 */
	uint8_t alignmentFill_at_3729[3] = {};
	/**
	 * units: ratio
	 * offset 3732
	 */
	float triggerVVTGapOverrideFrom[VVT_TRACKING_LENGTH] = {};
	/**
	 * units: ratio
	 * offset 3748
	 */
	float triggerVVTGapOverrideTo[VVT_TRACKING_LENGTH] = {};
	/**
	 * Traction control hold time. When traction control is active, the peak drop values are held for this duration.
	 * units: ms
	 * offset 3764
	 */
	uint16_t tractionControlHoldTime;
	/**
	 * Traction control decay time. After the hold time expires, values decay back to the current table value over this duration.
	 * units: ms
	 * offset 3766
	 */
	uint16_t tractionControlDecayTime;
	/**
	 * Selects the Y axis source for traction control tables. In RPM Accel mode the axis value is RPM/s divided by 100 (e.g. a bin value of 1.0 represents 100 RPM/s).
	 * offset 3768
	 */
	tc_y_axis_e tractionControlYAxisSource;
	/**
	 * units: %
	 * offset 3769
	 */
	int8_t tractionControlEtbDrop[TRACTION_CONTROL_ETB_DROP_SPEED_SIZE][TRACTION_CONTROL_ETB_DROP_SLIP_SIZE] = {};
	/**
	 * This sets an immediate limit on injector duty cycle. If this threshold is reached, the system will immediately cut the injectors.
	 * units: %
	 * offset 3805
	 */
	uint8_t maxInjectorDutyInstant;
	/**
	 * This limit allows injectors to operate up to the specified duty cycle percentage for a short period (as defined by the delay). After this delay, if the duty cycle remains above the limit, it will trigger a cut.
	 * units: %
	 * offset 3806
	 */
	uint8_t maxInjectorDutySustained;
	/**
	 * Timeout period for duty cycle over the sustained limit to trigger duty cycle protection.
	 * units: sec
	 * offset 3807
	 */
	scaled_channel<uint8_t, 10, 1> maxInjectorDutySustainedTimeout;
	/**
	 * offset 3808
	 */
	output_pin_e injectionPinsStage2[MAX_CYLINDER_COUNT] = {};
	/**
	 * units: Deg
	 * offset 3832
	 */
	int8_t tractionControlTimingDrop[TRACTION_CONTROL_ETB_DROP_SPEED_SIZE][TRACTION_CONTROL_ETB_DROP_SLIP_SIZE] = {};
	/**
	 * units: %
	 * offset 3868
	 */
	int8_t tractionControlIgnitionSkip[TRACTION_CONTROL_ETB_DROP_SPEED_SIZE][TRACTION_CONTROL_ETB_DROP_SLIP_SIZE] = {};
	/**
	 * units: x
	 * offset 3904
	 */
	float tractionControlLuaMultBins[8] = {};
	/**
	 * units: mult
	 * offset 3936
	 */
	float tractionControlLuaMultValues[8] = {};
	/**
	 * offset 3968
	 */
	float auxSpeed1Multiplier;
	/**
	 * offset 3972
	 */
	float brakeMeanEffectivePressureDifferential;
	/**
	 * offset 3976
	 */
	Gpio spi4mosiPin;
	/**
	 * offset 3978
	 */
	Gpio spi4misoPin;
	/**
	 * offset 3980
	 */
	Gpio spi4sckPin;
	/**
	 * offset 3982
	 */
	Gpio spi5mosiPin;
	/**
	 * offset 3984
	 */
	Gpio spi5misoPin;
	/**
	 * offset 3986
	 */
	Gpio spi5sckPin;
	/**
	 * offset 3988
	 */
	Gpio spi6mosiPin;
	/**
	 * offset 3990
	 */
	Gpio spi6misoPin;
	/**
	 * offset 3992
	 */
	Gpio spi6sckPin;
	/**
	 * offset 3994
	 */
	pin_mode_e spi4SckMode;
	/**
	 * offset 3995
	 */
	pin_mode_e spi4MosiMode;
	/**
	 * offset 3996
	 */
	pin_mode_e spi4MisoMode;
	/**
	 * offset 3997
	 */
	pin_mode_e spi5SckMode;
	/**
	 * offset 3998
	 */
	pin_mode_e spi5MosiMode;
	/**
	 * offset 3999
	 */
	pin_mode_e spi5MisoMode;
	/**
	 * offset 4000
	 */
	pin_mode_e spi6SckMode;
	/**
	 * offset 4001
	 */
	pin_mode_e spi6MosiMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4002
	 */
	uint8_t alignmentFill_at_4002[2] = {};
	/**
	 * Secondary TTL channel baud rate
	 * units: BPs
	 * offset 4004
	 */
	uint32_t tunerStudioSerialSpeed;
	/**
	 * offset 4008
	 */
	Gpio camSimulatorPin;
	/**
	 * offset 4010
	 */
	pin_output_mode_e camSimulatorPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4011
	 */
	uint8_t alignmentFill_at_4011[1] = {};
	/**
	 * offset 4012
	 */
	int anotherCiTest;
	/**
	 * offset 4016
	 */
	uint32_t device_uid[3] = {};
	/**
	 * offset 4028
	 */
	adc_channel_e tcu_rangeAnalogInput[RANGE_INPUT_COUNT] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4034
	 */
	uint8_t alignmentFill_at_4034[2] = {};
	/**
	 * units: Ohm
	 * offset 4036
	 */
	float tcu_rangeSensorBiasResistor;
	/**
	 * offset 4040
	 */
	MsIoBox_config_s msIoBox0;
	/**
	 * Nominal coil charge current, 0.25A step
	 * units: A
	 * offset 4044
	 */
	scaled_channel<uint8_t, 4, 1> mc33810Nomi;
	/**
	 * Maximum coil charge current, 1A step
	 * units: A
	 * offset 4045
	 */
	uint8_t mc33810Maxi;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4046
	 */
	uint8_t alignmentFill_at_4046[2] = {};
	/**
	 * offset 4048
	 */
	linear_sensor_s acPressure;
	/**
	 * value of A/C pressure in kPa/psi before that compressor is disengaged
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 4068
	 */
	uint16_t minAcPressure;
	/**
	 * value of A/C pressure in kPa/psi after that compressor is disengaged
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 4070
	 */
	uint16_t maxAcPressure;
	/**
	 * offset 4072
	 */
	linear_sensor_s clutchPressure;
	/**
	 * Delay before cutting fuel due to low oil pressure. Use this to ignore short pressure blips and sensor noise.
	 * units: sec
	 * offset 4092
	 */
	scaled_channel<uint8_t, 10, 1> minimumOilPressureTimeout;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4093
	 */
	uint8_t alignmentFill_at_4093[3] = {};
	/**
	 * offset 4096
	 */
	linear_sensor_s auxLinear3;
	/**
	 * offset 4116
	 */
	linear_sensor_s auxLinear4;
	/**
	 * offset 4136
	 */
	float engineShutDownPeriod;
	/**
	 * Below TPS value all knock suppression will be disabled.
	 * units: %
	 * offset 4140
	 */
	scaled_channel<uint8_t, 1, 1> knockSuppressMinTps;
	/**
	 * Fuel to odd when a knock event occurs. Advice: 5% (mild), 10% (turbo/high comp.), 15% (high knock, e.g. GDI), 20% (spicy lump),
	 * units: %
	 * offset 4141
	 */
	scaled_channel<uint8_t, 10, 1> knockFuelTrimAggression;
	/**
	 * After a knock event, reapply fuel at this rate.
	 * units: 1%/s
	 * offset 4142
	 */
	scaled_channel<uint8_t, 10, 1> knockFuelTrimReapplyRate;
	/**
	 * Maximum Amount of Fuel trim when knock
	 * units: %
	 * offset 4143
	 */
	scaled_channel<uint8_t, 1, 1> knockFuelTrim;
	/**
	 * units: sense
	 * offset 4144
	 */
	float knockSpectrumSensitivity;
	/**
	 * "Estimated knock frequency, ignore cylinderBore if this one > 0"
	 * units: Hz
	 * offset 4148
	 */
	float knockFrequency;
	/**
	 * None = I have a MAP-referenced fuel pressure regulator
	 * Fixed rail pressure = I have an atmosphere-referenced fuel pressure regulator (returnless, typically)
	 * Sensed rail pressure = I have a fuel pressure sensor
	 *  HPFP fuel mass compensation = manual mode for GDI engines
	 * Manual Pressure Correction = same fuel pressure sensor/reference pressure math as Sensed Rail Pressure, but no automatic sqrt(pressure) flow compensation is applied
	 * offset 4152
	 */
	injector_compensation_mode_e secondaryInjectorCompensationMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4153
	 */
	uint8_t alignmentFill_at_4153[3] = {};
	/**
	 * This is the pressure at which your injector flow is known.
	 * For example if your injectors flow 400cc/min at 3.5 bar, enter 350kpa here.
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 4156
	 */
	float secondaryInjectorFuelReferencePressure;
	/**
	 * SENT input connected to ETB
	 * offset 4160
	 */
	SentInput EtbSentInput;
	/**
	 * SENT input used for high pressure fuel sensor
	 * offset 4161
	 */
	SentInput FuelHighPressureSentInput;
	/**
	 * If you have SENT High Pressure Fuel Sensor please select type. For analog TPS leave None
	 * offset 4162
	 */
	SentFuelHighPressureType FuelHighPressureSentType;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4163
	 */
	uint8_t alignmentFill_at_4163[1] = {};
	/**
	offset 4164 bit 0 */
	bool nitrousControlEnabled : 1 {};
	/**
	offset 4164 bit 1 */
	bool vvlControlEnabled : 1 {};
	/**
	offset 4164 bit 2 */
	bool exhaustCutoutEnabled : 1 {};
	/**
	offset 4164 bit 3 */
	bool exhaustCutoutShowOpenState : 1 {};
	/**
	offset 4164 bit 4 */
	bool unusedBit_CutoutWasHBridge : 1 {};
	/**
	offset 4164 bit 5 */
	bool exhaustCutoutInvertedOutput : 1 {};
	/**
	offset 4164 bit 6 */
	bool exhaustCutoutKeyOnTestEnabled : 1 {};
	/**
	offset 4164 bit 7 */
	bool exhaustCutoutEngineOnTestEnabled : 1 {};
	/**
	offset 4164 bit 8 */
	bool unusedBit_CutoutWasPwm : 1 {};
	/**
	 * Centralized Engine State Machine. When enabled, state detection is driven by a single priority-ordered evaluator. When disabled, each controller manages its own state detection.
	offset 4164 bit 9 */
	bool useEngineStateMachine : 1 {};
	/**
	offset 4164 bit 10 */
	bool cdvControlEnabled : 1 {};
	/**
	 * Deactivate CDV solenoid when clutch pedal is released
	offset 4164 bit 11 */
	bool cdvUseClutchExit : 1 {};
	/**
	offset 4164 bit 12 */
	bool luaLimiterEnabled : 1 {};
	/**
	 * Clutch Delay Valve activation mode. Simple: activate on launch/pre-launch entry. Smart: also require clutch pressure to be inside the configured window (cdvSmartMinPressure/cdvSmartMaxPressure) before activating, and deactivate immediately on leaving the window.
	offset 4164 bit 13 */
	bool cdvSmartMode : 1 {};
	/**
	 * Uses Electronic Throttle Limiting (a PID) to try to hold engine RPM about 50rpm below the hard RPM limit, instead of (or in addition to) cutting fuel/spark at the limit. This is a real distinction from the hard RPM limit: the hard limit only cuts, while this tries to actively manage throttle to stay just under it.
	offset 4164 bit 14 */
	bool cutEtbOnRpmLimit : 1 {};
	/**
	 * When enabled, overrun fuel cut will not engage while the transmission is in neutral (DetectedGear sensor reads neutral).
	 * Requires the gear ratio table (Total Gear Count / gear ratios) to be configured: if it is not set up, the detected gear always reads neutral and fuel cut will never engage.
	offset 4164 bit 15 */
	bool coastingFuelCutRequiresGear : 1 {};
	/**
	 * Use CHT/IAT sensors to estimate oil temperature (EOT) via first-order thermal model. Disable if a real oil temp sensor is wired.
	offset 4164 bit 16 */
	bool eotFromIatCht : 1 {};
	/**
	 * When enabled, the priming pulse fires after 'primingTriggerTeeth' raw primary trigger teeth are seen since ignition-on, instead of after the fixed 'primingDelay'. Tooth counting does not require trigger sync, and (like the fixed-delay mode) the pulse still only fires once per key cycle.
	offset 4164 bit 17 */
	bool primeOnTriggerTeeth : 1 {};
	/**
	offset 4164 bit 18 */
	bool unusedBit_Fancy17 : 1 {};
	/**
	offset 4164 bit 19 */
	bool unusedBit_Fancy18 : 1 {};
	/**
	offset 4164 bit 20 */
	bool unusedBit_Fancy19 : 1 {};
	/**
	offset 4164 bit 21 */
	bool unusedBit_Fancy20 : 1 {};
	/**
	offset 4164 bit 22 */
	bool unusedBit_Fancy21 : 1 {};
	/**
	offset 4164 bit 23 */
	bool unusedBit_Fancy22 : 1 {};
	/**
	offset 4164 bit 24 */
	bool unusedBit_Fancy23 : 1 {};
	/**
	offset 4164 bit 25 */
	bool unusedBit_Fancy24 : 1 {};
	/**
	offset 4164 bit 26 */
	bool unusedBit_Fancy25 : 1 {};
	/**
	offset 4164 bit 27 */
	bool unusedBit_Fancy26 : 1 {};
	/**
	offset 4164 bit 28 */
	bool unusedBit_Fancy27 : 1 {};
	/**
	offset 4164 bit 29 */
	bool unusedBit_Fancy28 : 1 {};
	/**
	offset 4164 bit 30 */
	bool unusedBit_Fancy29 : 1 {};
	/**
	offset 4164 bit 31 */
	bool unusedBit_Fancy30 : 1 {};
	/**
	 * offset 4168
	 */
	nitrous_arming_method_e nitrousControlArmingMethod;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4169
	 */
	uint8_t alignmentFill_at_4169[1] = {};
	/**
	 * Pin that activates nitrous control
	 * offset 4170
	 */
	switch_input_pin_e nitrousControlTriggerPin;
	/**
	 * offset 4172
	 */
	pin_input_mode_e nitrousControlTriggerPinMode;
	/**
	 * offset 4173
	 */
	lua_gauge_e nitrousLuaGauge;
	/**
	 * Lua gauge index used as traction control multiplier input
	 * offset 4174
	 */
	lua_gauge_e tractionControlLuaGauge;
	/**
	 * offset 4175
	 */
	lua_gauge_meaning_e nitrousLuaGaugeMeaning;
	/**
	 * offset 4176
	 */
	float nitrousLuaGaugeArmingValue;
	/**
	 * offset 4180
	 */
	int nitrousMinimumTps;
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 4184
	 */
	int16_t nitrousMinimumClt;
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 4186
	 */
	int16_t nitrousMaximumMap;
	/**
	 * units: afr
	 * offset 4188
	 */
	scaled_channel<uint8_t, 10, 1> nitrousMaximumAfr;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4189
	 */
	uint8_t alignmentFill_at_4189[1] = {};
	/**
	 * units: rpm
	 * offset 4190
	 */
	uint16_t nitrousActivationRpm;
	/**
	 * units: rpm
	 * offset 4192
	 */
	uint16_t nitrousDeactivationRpm;
	/**
	 * units: rpm
	 * offset 4194
	 */
	uint16_t nitrousDeactivationRpmWindow;
	/**
	 * Retard timing by this amount during DFCO. Smooths the transition back from fuel cut. After fuel is restored, ramp timing back in over the period specified.
	 * units: deg
	 * offset 4196
	 */
	uint8_t dfcoRetardDeg;
	/**
	 * Smooths the transition back from fuel cut. After fuel is restored, ramp timing back in over the period specified.
	 * units: s
	 * offset 4197
	 */
	scaled_channel<uint8_t, 10, 1> dfcoRetardRampInTime;
	/**
	 * Selects when fuel cut is active. Overrun: TPS-closed + RPM above threshold (traditional DFCO). Decel: engine RPM is dropping faster than smDecelRateThreshold (works without VSS). Both: either condition triggers fuel cut.
	 * offset 4198
	 */
	dfco_fuel_cut_mode_e dfcoFuelCutMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4199
	 */
	uint8_t alignmentFill_at_4199[1] = {};
	/**
	 * offset 4200
	 */
	output_pin_e nitrousRelayPin;
	/**
	 * offset 4202
	 */
	pin_output_mode_e nitrousRelayPinMode;
	/**
	 * units: %
	 * offset 4203
	 */
	int8_t nitrousFuelAdderPercent;
	/**
	 * Retard timing to remove from actual final timing (after all corrections) due to additional air.
	 * units: deg
	 * offset 4204
	 */
	float nitrousIgnitionRetard;
	/**
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 4208
	 */
	uint16_t nitrousMinimumVehicleSpeed;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4210
	 */
	uint8_t alignmentFill_at_4210[2] = {};
	/**
	 * Exponential Average Alpha filtering parameter
	 * offset 4212
	 */
	float fuelLevelAveragingAlpha;
	/**
	 * How often do we update fuel level gauge
	 * units: seconds
	 * offset 4216
	 */
	float fuelLevelUpdatePeriodSec;
	/**
	 * Error below specified value
	 * units: v
	 * offset 4220
	 */
	float fuelLevelLowThresholdVoltage;
	/**
	 * Error above specified value
	 * units: v
	 * offset 4224
	 */
	float fuelLevelHighThresholdVoltage;
	/**
	 * A higher alpha (closer to 1) means the EMA reacts more quickly to changes in the data.
	 * '1' means no filtering, 0.98 would be some filtering.
	 * offset 4228
	 */
	float afrExpAverageAlpha;
	/**
	 * Compensates for trigger delay due to belt stretch, or other electromechanical issues. beware that raising this value advances ignition timing!
	 * units: uS
	 * offset 4232
	 */
	scaled_channel<uint8_t, 1, 1> sparkHardwareLatencyCorrection;
	/**
	 * Delay before cutting fuel due to extra high oil pressure. Use this to ignore short pressure blips and sensor noise.
	 * units: sec
	 * offset 4233
	 */
	scaled_channel<uint8_t, 10, 1> maxOilPressureTimeout;
	/**
	 * units: kg/h
	 * offset 4234
	 */
	scaled_channel<uint16_t, 100, 1> idleFlowEstimateFlow[8] = {};
	/**
	 * units: %
	 * offset 4250
	 */
	scaled_channel<uint8_t, 2, 1> idleFlowEstimatePosition[8] = {};
	/**
	 * units: mg
	 * offset 4258
	 */
	int8_t airmassToTimingBins[8] = {};
	/**
	 * units: deg
	 * offset 4266
	 */
	int8_t airmassToTimingValues[8] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4274
	 */
	uint8_t alignmentFill_at_4274[2] = {};
	/**
	 * Voltage when the wastegate is fully open
	 * units: v
	 * offset 4276
	 */
	float wastegatePositionOpenedVoltage;
	/**
	 * Voltage when the wastegate is closed
	 * units: v
	 * offset 4280
	 */
	float wastegatePositionClosedVoltage;
	/**
	 * offset 4284
	 */
	wbo_s canWbo[CAN_WBO_COUNT] = {};
	/**
	 * offset 4300
	 */
	output_pin_e vvlRelayPin;
	/**
	 * offset 4302
	 */
	pin_output_mode_e vvlRelayPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4303
	 */
	uint8_t alignmentFill_at_4303[1] = {};
	/**
	 * Output pin for clutch delay valve bypass solenoid
	 * offset 4304
	 */
	output_pin_e cdvSolenoidPin;
	/**
	 * offset 4306
	 */
	pin_output_mode_e cdvSolenoidPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4307
	 */
	uint8_t alignmentFill_at_4307[1] = {};
	/**
	 * offset 4308
	 */
	vvl_s vvlController;
	/**
	 * offset 4332
	 */
	rotational_idle_s rotationalIdleController;
	/**
	 * Launch RPM Threshold: when above 0, launch only engages if the activation switch (button/clutch) is pressed at or below this RPM, and stays latched while held - even past this RPM. This lets a standing launch (switch pressed low, revved up) coexist with flat shift / torque reduction (switch blipped high during an upshift). 0 disables the gate (legacy behavior).
	 * units: rpm
	 * offset 4368
	 */
	uint16_t launchRpmThreshold;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4370
	 */
	uint8_t alignmentFill_at_4370[2] = {};
	/**
	 * Enable pops and bangs mode. WARNING: will damage catalytic converters and reduce turbocharger life.
	offset 4372 bit 0 */
	bool popsAndBangsEnabled : 1 {};
	/**
	offset 4372 bit 1 */
	bool unusedBit_1180_1 : 1 {};
	/**
	offset 4372 bit 2 */
	bool unusedBit_1180_2 : 1 {};
	/**
	offset 4372 bit 3 */
	bool unusedBit_1180_3 : 1 {};
	/**
	offset 4372 bit 4 */
	bool unusedBit_1180_4 : 1 {};
	/**
	offset 4372 bit 5 */
	bool unusedBit_1180_5 : 1 {};
	/**
	offset 4372 bit 6 */
	bool unusedBit_1180_6 : 1 {};
	/**
	offset 4372 bit 7 */
	bool unusedBit_1180_7 : 1 {};
	/**
	offset 4372 bit 8 */
	bool unusedBit_1180_8 : 1 {};
	/**
	offset 4372 bit 9 */
	bool unusedBit_1180_9 : 1 {};
	/**
	offset 4372 bit 10 */
	bool unusedBit_1180_10 : 1 {};
	/**
	offset 4372 bit 11 */
	bool unusedBit_1180_11 : 1 {};
	/**
	offset 4372 bit 12 */
	bool unusedBit_1180_12 : 1 {};
	/**
	offset 4372 bit 13 */
	bool unusedBit_1180_13 : 1 {};
	/**
	offset 4372 bit 14 */
	bool unusedBit_1180_14 : 1 {};
	/**
	offset 4372 bit 15 */
	bool unusedBit_1180_15 : 1 {};
	/**
	offset 4372 bit 16 */
	bool unusedBit_1180_16 : 1 {};
	/**
	offset 4372 bit 17 */
	bool unusedBit_1180_17 : 1 {};
	/**
	offset 4372 bit 18 */
	bool unusedBit_1180_18 : 1 {};
	/**
	offset 4372 bit 19 */
	bool unusedBit_1180_19 : 1 {};
	/**
	offset 4372 bit 20 */
	bool unusedBit_1180_20 : 1 {};
	/**
	offset 4372 bit 21 */
	bool unusedBit_1180_21 : 1 {};
	/**
	offset 4372 bit 22 */
	bool unusedBit_1180_22 : 1 {};
	/**
	offset 4372 bit 23 */
	bool unusedBit_1180_23 : 1 {};
	/**
	offset 4372 bit 24 */
	bool unusedBit_1180_24 : 1 {};
	/**
	offset 4372 bit 25 */
	bool unusedBit_1180_25 : 1 {};
	/**
	offset 4372 bit 26 */
	bool unusedBit_1180_26 : 1 {};
	/**
	offset 4372 bit 27 */
	bool unusedBit_1180_27 : 1 {};
	/**
	offset 4372 bit 28 */
	bool unusedBit_1180_28 : 1 {};
	/**
	offset 4372 bit 29 */
	bool unusedBit_1180_29 : 1 {};
	/**
	offset 4372 bit 30 */
	bool unusedBit_1180_30 : 1 {};
	/**
	offset 4372 bit 31 */
	bool unusedBit_1180_31 : 1 {};
	/**
	 * Idle-up digital input pins
	 * offset 4376
	 */
	switch_input_pin_e idleUpSwitchPins[IDLE_UP_SWITCH_COUNT] = {};
	/**
	 * Idle-up input polarity/pull
	 * offset 4382
	 */
	pin_input_mode_e idleUpSwitchMode[IDLE_UP_SWITCH_COUNT] = {};
	/**
	 * Idle % to add when this switch is active
	 * units: %
	 * offset 4385
	 */
	uint8_t idleUpAdder[IDLE_UP_SWITCH_COUNT] = {};
	/**
	 * offset 4388
	 */
	i2c_config_s i2c[I2C_BUS_TOTAL_COUNT] = {};
	/**
	 * offset 4436
	 */
	can_sniffer_channel_s canSniffer[EFI_CAN_BUS_COUNT] = {};
	/**
	 * Bus for Tx
	 * offset 4444
	 */
	can_bus_channel_e canSnifferTxBus;
	/**
	 * 'Trigger' mode will write a high speed log of trigger events (warning: uses lots of space!). 'Full MLG' mode will write a standard MLG of sensors, engine function, etc. similar to the one captured in TunerStudio. 'DTC Freeze Frame' will write one frame MLG log and short CSV trigger log on every DTC
	 * offset 4445
	 */
	SDLoggerMode sdLoggerMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4446
	 */
	uint8_t alignmentFill_at_4446[2] = {};
	/**
	 * Dwell Duty Mode: when enabled, ignores the RPM/voltage dwell tables and computes dwell as a fixed percentage of the time between consecutive ignition pulses. Required for Ford TFI modules that expect a 50% duty cycle square wave.
	offset 4448 bit 0 */
	bool dwellDutyModeEnabled : 1 {};
	/**
	offset 4448 bit 1 */
	bool unusedBit_1220_1 : 1 {};
	/**
	offset 4448 bit 2 */
	bool unusedBit_1220_2 : 1 {};
	/**
	offset 4448 bit 3 */
	bool unusedBit_1220_3 : 1 {};
	/**
	offset 4448 bit 4 */
	bool unusedBit_1220_4 : 1 {};
	/**
	offset 4448 bit 5 */
	bool unusedBit_1220_5 : 1 {};
	/**
	offset 4448 bit 6 */
	bool unusedBit_1220_6 : 1 {};
	/**
	offset 4448 bit 7 */
	bool unusedBit_1220_7 : 1 {};
	/**
	offset 4448 bit 8 */
	bool unusedBit_1220_8 : 1 {};
	/**
	offset 4448 bit 9 */
	bool unusedBit_1220_9 : 1 {};
	/**
	offset 4448 bit 10 */
	bool unusedBit_1220_10 : 1 {};
	/**
	offset 4448 bit 11 */
	bool unusedBit_1220_11 : 1 {};
	/**
	offset 4448 bit 12 */
	bool unusedBit_1220_12 : 1 {};
	/**
	offset 4448 bit 13 */
	bool unusedBit_1220_13 : 1 {};
	/**
	offset 4448 bit 14 */
	bool unusedBit_1220_14 : 1 {};
	/**
	offset 4448 bit 15 */
	bool unusedBit_1220_15 : 1 {};
	/**
	offset 4448 bit 16 */
	bool unusedBit_1220_16 : 1 {};
	/**
	offset 4448 bit 17 */
	bool unusedBit_1220_17 : 1 {};
	/**
	offset 4448 bit 18 */
	bool unusedBit_1220_18 : 1 {};
	/**
	offset 4448 bit 19 */
	bool unusedBit_1220_19 : 1 {};
	/**
	offset 4448 bit 20 */
	bool unusedBit_1220_20 : 1 {};
	/**
	offset 4448 bit 21 */
	bool unusedBit_1220_21 : 1 {};
	/**
	offset 4448 bit 22 */
	bool unusedBit_1220_22 : 1 {};
	/**
	offset 4448 bit 23 */
	bool unusedBit_1220_23 : 1 {};
	/**
	offset 4448 bit 24 */
	bool unusedBit_1220_24 : 1 {};
	/**
	offset 4448 bit 25 */
	bool unusedBit_1220_25 : 1 {};
	/**
	offset 4448 bit 26 */
	bool unusedBit_1220_26 : 1 {};
	/**
	offset 4448 bit 27 */
	bool unusedBit_1220_27 : 1 {};
	/**
	offset 4448 bit 28 */
	bool unusedBit_1220_28 : 1 {};
	/**
	offset 4448 bit 29 */
	bool unusedBit_1220_29 : 1 {};
	/**
	offset 4448 bit 30 */
	bool unusedBit_1220_30 : 1 {};
	/**
	offset 4448 bit 31 */
	bool unusedBit_1220_31 : 1 {};
	/**
	 * Dwell Duty Mode: percentage of the inter-spark interval used as coil dwell time. 50 = half the interval between pulses (standard TFI target).
	 * units: %
	 * offset 4452
	 */
	uint8_t dwellDutyPercent;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4453
	 */
	uint8_t alignmentFill_at_4453[3] = {};
};
static_assert(sizeof(engine_configuration_s) == 4456);

// start of ign_cyl_trim_s
struct ign_cyl_trim_s {
	/**
	 * offset 0
	 */
	scaled_channel<int8_t, 5, 1> table[IGN_TRIM_SIZE][IGN_TRIM_SIZE] = {};
};
static_assert(sizeof(ign_cyl_trim_s) == 16);

// start of fuel_cyl_trim_s
struct fuel_cyl_trim_s {
	/**
	 * offset 0
	 */
	scaled_channel<int8_t, 5, 1> table[FUEL_TRIM_SIZE][FUEL_TRIM_SIZE] = {};
};
static_assert(sizeof(fuel_cyl_trim_s) == 16);

// start of blend_table_s
struct blend_table_s {
	/**
	 * offset 0
	 */
	scaled_channel<int16_t, 1, 1> table[BLEND_TABLE_COUNT][BLEND_TABLE_COUNT] = {};
	/**
	 * units: Load
	 * offset 128
	 */
	uint16_t loadBins[BLEND_TABLE_COUNT] = {};
	/**
	 * units: RPM
	 * offset 144
	 */
	uint16_t rpmBins[BLEND_TABLE_COUNT] = {};
	/**
	 * offset 160
	 */
	gppwm_channel_e blendParameter;
	/**
	 * offset 161
	 */
	gppwm_channel_e yAxisOverride;
	/**
	 * offset 162
	 */
	scaled_channel<int16_t, 10, 1> blendBins[BLEND_FACTOR_SIZE] = {};
	/**
	 * units: %
	 * offset 178
	 */
	scaled_channel<uint8_t, 2, 1> blendValues[BLEND_FACTOR_SIZE] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 186
	 */
	uint8_t alignmentFill_at_186[2] = {};
};
static_assert(sizeof(blend_table_s) == 188);

// start of blend_table_s_BLEND_PRECISION__1
struct blend_table_s_BLEND_PRECISION__1 {
	/**
	 * offset 0
	 */
	scaled_channel<int16_t, 10, 1> table[BLEND_TABLE_COUNT][BLEND_TABLE_COUNT] = {};
	/**
	 * units: Load
	 * offset 128
	 */
	uint16_t loadBins[BLEND_TABLE_COUNT] = {};
	/**
	 * units: RPM
	 * offset 144
	 */
	uint16_t rpmBins[BLEND_TABLE_COUNT] = {};
	/**
	 * offset 160
	 */
	gppwm_channel_e blendParameter;
	/**
	 * offset 161
	 */
	gppwm_channel_e yAxisOverride;
	/**
	 * offset 162
	 */
	scaled_channel<int16_t, 10, 1> blendBins[BLEND_FACTOR_SIZE] = {};
	/**
	 * units: %
	 * offset 178
	 */
	scaled_channel<uint8_t, 2, 1> blendValues[BLEND_FACTOR_SIZE] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 186
	 */
	uint8_t alignmentFill_at_186[2] = {};
};
static_assert(sizeof(blend_table_s_BLEND_PRECISION__1) == 188);

// start of blend_table_s_TARGET_AFR_BLEND_PRECISION__2
struct blend_table_s_TARGET_AFR_BLEND_PRECISION__2 {
	/**
	 * offset 0
	 */
	scaled_channel<int16_t, 100, 1> table[BLEND_TABLE_COUNT][BLEND_TABLE_COUNT] = {};
	/**
	 * units: Load
	 * offset 128
	 */
	uint16_t loadBins[BLEND_TABLE_COUNT] = {};
	/**
	 * units: RPM
	 * offset 144
	 */
	uint16_t rpmBins[BLEND_TABLE_COUNT] = {};
	/**
	 * offset 160
	 */
	gppwm_channel_e blendParameter;
	/**
	 * offset 161
	 */
	gppwm_channel_e yAxisOverride;
	/**
	 * offset 162
	 */
	scaled_channel<int16_t, 10, 1> blendBins[BLEND_FACTOR_SIZE] = {};
	/**
	 * units: %
	 * offset 178
	 */
	scaled_channel<uint8_t, 2, 1> blendValues[BLEND_FACTOR_SIZE] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 186
	 */
	uint8_t alignmentFill_at_186[2] = {};
};
static_assert(sizeof(blend_table_s_TARGET_AFR_BLEND_PRECISION__2) == 188);

// start of KnockGain
struct KnockGain {
	/**
	 * units: dB
	 * offset 0
	 */
	int8_t table[6][6] = {};
};
static_assert(sizeof(KnockGain) == 36);

// start of persistent_config_s
struct persistent_config_s {
	/**
	 * offset 0
	 */
	engine_configuration_s engineConfiguration;
	/**
	 * offset 4456
	 */
	float tmfTable[TMF_RATIO_SIZE][TMF_SIZE] = {};
	/**
	 * offset 4472
	 */
	float tmfRatioBins[TMF_RATIO_SIZE] = {};
	/**
	 * offset 4480
	 */
	float tmfOpeningBins[TMF_SIZE] = {};
	/**
	 * units: mult
	 * offset 4488
	 */
	float postCrankingFactor[CRANKING_ENRICH_CLT_COUNT][CRANKING_ENRICH_COUNT] = {};
	/**
	 * units: count
	 * offset 4632
	 */
	uint16_t postCrankingDurationBins[CRANKING_ENRICH_COUNT] = {};
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 4644
	 */
	int16_t postCrankingCLTBins[CRANKING_ENRICH_CLT_COUNT] = {};
	/**
	 * target TPS value, 0 to 100%
	 * TODO: use int8 data date once we template interpolation method
	 * units: target TPS position
	 * offset 4656
	 */
	float etbBiasBins[ETB_BIAS_CURVE_LENGTH] = {};
	/**
	 * PWM bias, open loop component of PID closed loop control
	 * units: ETB duty cycle bias
	 * offset 4688
	 */
	float etbBiasValues[ETB_BIAS_CURVE_LENGTH] = {};
	/**
	 * target Wastegate value, 0 to 100%
	 * units: target DC position
	 * offset 4720
	 */
	int8_t dcWastegateBiasBins[ETB_BIAS_CURVE_LENGTH] = {};
	/**
	 * PWM bias, open loop component of PID closed loop control
	 * units: DC wastegate duty cycle bias
	 * offset 4728
	 */
	scaled_channel<int16_t, 100, 1> dcWastegateBiasValues[ETB_BIAS_CURVE_LENGTH] = {};
	/**
	 * units: %
	 * offset 4744
	 */
	scaled_channel<uint8_t, 20, 1> iacPidMultTable[IAC_PID_MULT_SIZE][IAC_PID_MULT_SIZE] = {};
	/**
	 * units: Load
	 * offset 4808
	 */
	uint8_t iacPidMultLoadBins[IAC_PID_MULT_SIZE] = {};
	/**
	 * units: RPM
	 * offset 4816
	 */
	scaled_channel<uint8_t, 1, 10> iacPidMultRpmBins[IAC_PID_MULT_RPM_SIZE] = {};
	/**
	 * On Single Coil or Wasted Spark setups you have to lower dwell at high RPM
	 * units: RPM
	 * offset 4824
	 */
	uint16_t sparkDwellRpmBins[DWELL_CURVE_SIZE] = {};
	/**
	 * units: ms
	 * offset 4840
	 */
	scaled_channel<uint16_t, 100, 1> sparkDwellValues[DWELL_CURVE_SIZE] = {};
	/**
	 * CLT-based target RPM for automatic idle controller
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 4856
	 */
	scaled_channel<int16_t, 1, 1> cltIdleRpmBins[CLT_CURVE_SIZE] = {};
	/**
	 * See idleRpmPid
	 * units: RPM
	 * offset 4888
	 */
	scaled_channel<uint8_t, 1, 20> cltIdleRpm[CLT_CURVE_SIZE] = {};
	/**
	 * units: deg
	 * offset 4904
	 */
	scaled_channel<int16_t, 10, 1> ignitionCltCorrTable[CLT_TIMING_LOAD_AXIS_SIZE][CLT_TIMING_TEMP_AXIS_SIZE] = {};
	/**
	 * CLT-based timing correction
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 4954
	 */
	scaled_channel<int16_t, 1, 1> ignitionCltCorrTempBins[CLT_TIMING_TEMP_AXIS_SIZE] = {};
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 4964
	 */
	scaled_channel<uint8_t, 1, 5> ignitionCltCorrLoadBins[CLT_TIMING_LOAD_AXIS_SIZE] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 4969
	 */
	uint8_t alignmentFill_at_4969[3] = {};
	/**
	 * units: x
	 * offset 4972
	 */
	float scriptCurve1Bins[SCRIPT_CURVE_16] = {};
	/**
	 * units: y
	 * offset 5036
	 */
	float scriptCurve1[SCRIPT_CURVE_16] = {};
	/**
	 * units: x
	 * offset 5100
	 */
	float scriptCurve2Bins[SCRIPT_CURVE_16] = {};
	/**
	 * units: y
	 * offset 5164
	 */
	float scriptCurve2[SCRIPT_CURVE_16] = {};
	/**
	 * units: x
	 * offset 5228
	 */
	float scriptCurve3Bins[SCRIPT_CURVE_8] = {};
	/**
	 * units: y
	 * offset 5260
	 */
	float scriptCurve3[SCRIPT_CURVE_8] = {};
	/**
	 * units: x
	 * offset 5292
	 */
	float scriptCurve4Bins[SCRIPT_CURVE_8] = {};
	/**
	 * units: y
	 * offset 5324
	 */
	float scriptCurve4[SCRIPT_CURVE_8] = {};
	/**
	 * units: x
	 * offset 5356
	 */
	float scriptCurve5Bins[SCRIPT_CURVE_8] = {};
	/**
	 * units: y
	 * offset 5388
	 */
	float scriptCurve5[SCRIPT_CURVE_8] = {};
	/**
	 * units: x
	 * offset 5420
	 */
	float scriptCurve6Bins[SCRIPT_CURVE_8] = {};
	/**
	 * units: y
	 * offset 5452
	 */
	float scriptCurve6[SCRIPT_CURVE_8] = {};
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 5484
	 */
	float baroCorrPressureBins[BARO_CORR_SIZE] = {};
	/**
	 * units: RPM
	 * offset 5500
	 */
	float baroCorrRpmBins[BARO_CORR_SIZE] = {};
	/**
	 * units: ratio
	 * offset 5516
	 */
	float baroCorrTable[BARO_CORR_SIZE][BARO_CORR_SIZE] = {};
	/**
	 * Cranking fuel correction coefficient based on TPS
	 * units: Ratio
	 * offset 5580
	 */
	float crankingTpsCoef[CRANKING_CURVE_SIZE] = {};
	/**
	 * units: %
	 * offset 5612
	 */
	float crankingTpsBins[CRANKING_CURVE_SIZE] = {};
	/**
	 * Optional timing advance table for Cranking (see useSeparateAdvanceForCranking)
	 * units: RPM
	 * offset 5644
	 */
	uint16_t crankingAdvanceBins[CRANKING_ADVANCE_CURVE_SIZE] = {};
	/**
	 * Optional timing advance table for Cranking (see useSeparateAdvanceForCranking)
	 * units: deg
	 * offset 5652
	 */
	scaled_channel<int16_t, 100, 1> crankingAdvance[CRANKING_ADVANCE_CURVE_SIZE] = {};
	/**
	 * RPM-based idle position for coasting
	 * units: RPM
	 * offset 5660
	 */
	scaled_channel<uint8_t, 1, 100> iacCoastingRpmBins[CLT_CURVE_SIZE] = {};
	/**
	 * RPM-based idle position for coasting
	 * units: %
	 * offset 5676
	 */
	scaled_channel<uint8_t, 2, 1> iacCoasting[CLT_CURVE_SIZE] = {};
	/**
	 * A/C idle adder pressure axis
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 5692
	 */
	float acIdleAdderByPressureBins[AC_PRESSURE_CURVE_SIZE] = {};
	/**
	 * Idle open-loop % adder while A/C is active
	 * units: %
	 * offset 5724
	 */
	float acIdleAdderByPressure[AC_PRESSURE_CURVE_SIZE] = {};
	/**
	 * offset 5756
	 */
	scaled_channel<uint8_t, 2, 1> boostTableOpenLoop[BOOST_LOAD_COUNT][BOOST_RPM_COUNT] = {};
	/**
	 * units: RPM
	 * offset 5820
	 */
	scaled_channel<uint8_t, 1, 100> boostRpmBins[BOOST_RPM_COUNT] = {};
	/**
	 * offset 5828
	 */
	uint16_t boostOpenLoopLoadBins[BOOST_LOAD_COUNT] = {};
	/**
	 * offset 5844
	 */
	scaled_channel<uint8_t, 1, 2> boostTableClosedLoop[BOOST_LOAD_COUNT][BOOST_RPM_COUNT] = {};
	/**
	 * offset 5908
	 */
	uint16_t boostClosedLoopLoadBins[BOOST_LOAD_COUNT] = {};
	/**
	 * units: %
	 * offset 5924
	 */
	uint8_t pedalToTpsTable[PEDAL_TO_TPS_SIZE][PEDAL_TO_TPS_RPM_SIZE] = {};
	/**
	 * units: %
	 * offset 5988
	 */
	uint8_t pedalToTpsPedalBins[PEDAL_TO_TPS_SIZE] = {};
	/**
	 * units: RPM
	 * offset 5996
	 */
	scaled_channel<uint8_t, 1, 100> pedalToTpsRpmBins[PEDAL_TO_TPS_RPM_SIZE] = {};
	/**
	 * CLT-based cranking position %. The values in this curve represent a percentage of the ETB Maximum angle. e.g. If "ETB Idle Maximum Angle" is 10, a value of 70 means 7% ETB Position.
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 6004
	 */
	float cltCrankingCorrBins[CLT_CRANKING_CURVE_SIZE] = {};
	/**
	 * Cranking idle valve position by coolant temperature (Duty mode only). 0-100% open-loop valve duty during cranking.
	 * units: value
	 * offset 6036
	 */
	float cltCrankingCorr[CLT_CRANKING_CURVE_SIZE] = {};
	/**
	 * Cranking idle RPM adder by coolant temperature (RPM mode only). Added on top of the normal CLT-based idle RPM target during cranking, tapering to zero as the engine warms into idle.
	 * units: RPM
	 * offset 6068
	 */
	float cltCrankingRpmAdder[CLT_CRANKING_CURVE_SIZE] = {};
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 6100
	 */
	float afterCrankingIACtaperDurationBins[CLT_CRANKING_TAPER_CURVE_SIZE] = {};
	/**
	 * This is the duration in cycles that the IAC will take to reach its normal idle position, it can be used to hold the idle higher for a few seconds after cranking to improve startup.
	 * Should be 100 once tune is better
	 * units: cycles
	 * offset 6124
	 */
	uint16_t afterCrankingIACtaperDuration[CLT_CRANKING_TAPER_CURVE_SIZE] = {};
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 6136
	 */
	float afterCrankingIACtaperHoldDurationBins[CLT_CRANKING_TAPER_CURVE_SIZE] = {};
	/**
	 * This is the duration in cycles that the cranking air amount / idle RPM flare value is held locked at its initial cranking value before the Crank-to-Run taper begins counting down.
	 * units: cycles
	 * offset 6160
	 */
	uint16_t afterCrankingIACtaperHoldDuration[CLT_CRANKING_TAPER_CURVE_SIZE] = {};
	/**
	 * Optional timing advance table for Idle (see useSeparateAdvanceForIdle)
	 * units: RPM
	 * offset 6172
	 */
	scaled_channel<uint8_t, 1, 50> idleAdvanceBins[IDLE_ADVANCE_CURVE_SIZE] = {};
	/**
	 * Optional timing advance table for Idle (see useSeparateAdvanceForIdle)
	 * units: deg
	 * offset 6180
	 */
	float idleAdvance[IDLE_ADVANCE_CURVE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 6212
	 */
	scaled_channel<uint8_t, 1, 10> idleVeRpmBins[IDLE_VE_SIZE_RPM] = {};
	/**
	 * units: load
	 * offset 6216
	 */
	uint8_t idleVeLoadBins[IDLE_VE_SIZE] = {};
	/**
	 * units: %
	 * offset 6220
	 */
	scaled_channel<uint16_t, 10, 1> idleVeTable[IDLE_VE_SIZE][IDLE_VE_SIZE_RPM] = {};
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 6252
	 */
	float cltFuelCorrBins[CLT_FUEL_CURVE_SIZE] = {};
	/**
	 * units: ratio
	 * offset 6316
	 */
	float cltFuelCorr[CLT_FUEL_CURVE_SIZE] = {};
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 6380
	 */
	float iatFuelCorrBins[IAT_CURVE_SIZE] = {};
	/**
	 * units: ratio
	 * offset 6444
	 */
	float iatFuelCorr[IAT_CURVE_SIZE] = {};
	/**
	 * units: ratio
	 * offset 6508
	 */
	float crankingFuelCoef[CRANKING_CURVE_SIZE] = {};
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 6540
	 */
	float crankingFuelBins[CRANKING_CURVE_SIZE] = {};
	/**
	 * units: counter
	 * offset 6572
	 */
	float crankingCycleBins[CRANKING_CURVE_SIZE] = {};
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 6604
	 */
	int16_t crankingCycleFuelCltBins[CRANKING_CYCLE_CLT_SIZE] = {};
	/**
	 * Base mass of the per-cylinder fuel injected during cranking. This is then modified by the multipliers for IAT, TPS ect, to give the final cranking pulse width.
	 * A reasonable starting point is 60mg per liter per cylinder.
	 * ex: 2 liter 4 cyl = 500cc/cyl, so 30mg cranking fuel.
	 * units: mg
	 * offset 6612
	 */
	float crankingCycleBaseFuel[CRANKING_CYCLE_CLT_SIZE][CRANKING_CURVE_SIZE] = {};
	/**
	 * CLT-based idle position for simple manual idle controller
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 6740
	 */
	float cltIdleCorrBins[CLT_IDLE_TABLE_CLT_SIZE] = {};
	/**
	 * CLT-based idle position for simple manual idle controller
	 * units: %
	 * offset 6772
	 */
	float cltIdleCorrTable[CLT_IDLE_TABLE_RPM_SIZE][CLT_IDLE_TABLE_CLT_SIZE] = {};
	/**
	 * units: Target RPM
	 * offset 6836
	 */
	scaled_channel<uint8_t, 1, 100> rpmIdleCorrBins[CLT_IDLE_TABLE_RPM_SIZE] = {};
	/**
	 * Long Term Idle Trim (LTIT) multiplicativo para idle open loop
	 * units: %
	 * offset 6838
	 */
	scaled_channel<uint16_t, 10, 1> ltitTable[CLT_IDLE_TABLE_CLT_SIZE] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 6854
	 */
	uint8_t alignmentFill_at_6854[2] = {};
	/**
	 * Also known as MAF transfer function.
	 * kg/hour value.
	 * By the way 2.081989116 kg/h = 1 ft3/m
	 * units: kg/hour
	 * offset 6856
	 */
	float mafDecoding[MAF_DECODING_COUNT] = {};
	/**
	 * units: V
	 * offset 6984
	 */
	float mafDecodingBins[MAF_DECODING_COUNT] = {};
	/**
	 * units: deg
	 * offset 7112
	 */
	scaled_channel<int16_t, 10, 1> ignitionIatCorrTable[IAT_IGN_CORR_LOAD_COUNT][IAT_IGN_CORR_TEMP_COUNT] = {};
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 7240
	 */
	int8_t ignitionIatCorrTempBins[IAT_IGN_CORR_TEMP_COUNT] = {};
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 7248
	 */
	scaled_channel<uint8_t, 1, 5> ignitionIatCorrLoadBins[IAT_IGN_CORR_LOAD_COUNT] = {};
	/**
	 * units: deg
	 * offset 7256
	 */
	int16_t injectionPhase[INJ_PHASE_LOAD_COUNT][INJ_PHASE_RPM_COUNT] = {};
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 7328
	 */
	uint16_t injPhaseLoadBins[INJ_PHASE_LOAD_COUNT] = {};
	/**
	 * units: RPM
	 * offset 7340
	 */
	uint16_t injPhaseRpmBins[INJ_PHASE_RPM_COUNT] = {};
	/**
	 * units: onoff
	 * offset 7352
	 */
	uint8_t tcuSolenoidTable[TCU_SOLENOID_COUNT][TCU_GEAR_COUNT] = {};
	/**
	 * This table represents MAP at a given TPS vs RPM, which we use if our MAP sensor has failed, or if we are using MAP Prediciton. 
	 *  This table should be a direct representation of MAP, you can tune it manually by disconnecting MAP sensor, and filling out the table with values that match an external gauge that shows MAP.
	 * Additionally, you can also use MLV to get the map values and/or generate the table for you
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 7412
	 */
	scaled_channel<uint16_t, 100, 1> mapEstimateTable[MAP_EST_LOAD_COUNT][MAP_EST_RPM_COUNT] = {};
	/**
	 * units: % TPS
	 * offset 7484
	 */
	scaled_channel<uint16_t, 100, 1> mapEstimateTpsBins[MAP_EST_LOAD_COUNT] = {};
	/**
	 * units: RPM
	 * offset 7496
	 */
	uint16_t mapEstimateRpmBins[MAP_EST_RPM_COUNT] = {};
	/**
	 * units: value
	 * offset 7508
	 */
	int8_t vvtTable1[VVT_TABLE_SIZE][VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: L
	 * offset 7572
	 */
	uint16_t vvtTable1LoadBins[VVT_TABLE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 7588
	 */
	uint16_t vvtTable1RpmBins[VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: value
	 * offset 7604
	 */
	int8_t vvtTable2[VVT_TABLE_SIZE][VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: L
	 * offset 7668
	 */
	uint16_t vvtTable2LoadBins[VVT_TABLE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 7684
	 */
	uint16_t vvtTable2RpmBins[VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: %
	 * offset 7700
	 */
	int8_t vvtFuelIntakeCorrTable[VVT_TABLE_SIZE][VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: deg
	 * offset 7764
	 */
	float vvtFuelIntakeCorrVvtBins[VVT_TABLE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 7796
	 */
	uint16_t vvtFuelIntakeCorrRpmBins[VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: %
	 * offset 7812
	 */
	int8_t vvtFuelExhaustCorrTable[VVT_TABLE_SIZE][VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: deg
	 * offset 7876
	 */
	float vvtFuelExhaustCorrVvtBins[VVT_TABLE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 7908
	 */
	uint16_t vvtFuelExhaustCorrRpmBins[VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: deg
	 * offset 7924
	 */
	scaled_channel<int16_t, 10, 1> vvtIgnIntakeCorrTable[VVT_TABLE_SIZE][VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: deg
	 * offset 8052
	 */
	float vvtIgnIntakeCorrVvtBins[VVT_TABLE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 8084
	 */
	uint16_t vvtIgnIntakeCorrRpmBins[VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: deg
	 * offset 8100
	 */
	scaled_channel<int16_t, 10, 1> vvtIgnExhaustCorrTable[VVT_TABLE_SIZE][VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: deg
	 * offset 8228
	 */
	float vvtIgnExhaustCorrVvtBins[VVT_TABLE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 8260
	 */
	uint16_t vvtIgnExhaustCorrRpmBins[VVT_TABLE_RPM_SIZE] = {};
	/**
	 * units: deg
	 * offset 8276
	 */
	scaled_channel<int16_t, 10, 1> ignitionTable[IGN_LOAD_COUNT][IGN_RPM_COUNT] = {};
	/**
	 * units: {bitStringValue(ignLoadUnitLabels, ignLoadUnitIdxPcv)}
	 * offset 8788
	 */
	uint16_t ignitionLoadBins[IGN_LOAD_COUNT] = {};
	/**
	 * units: RPM
	 * offset 8820
	 */
	uint16_t ignitionRpmBins[IGN_RPM_COUNT] = {};
	/**
	 * units: %
	 * offset 8852
	 */
	scaled_channel<uint16_t, 10, 1> veTable[VE_LOAD_COUNT][VE_RPM_COUNT] = {};
	/**
	 * units: {bitStringValue(veLoadUnitLabels, veLoadUnitIdxPcv)}
	 * offset 9364
	 */
	uint16_t veLoadBins[VE_LOAD_COUNT] = {};
	/**
	 * units: RPM
	 * offset 9396
	 */
	uint16_t veRpmBins[VE_RPM_COUNT] = {};
	/**
	 * units: {useLambdaOnInterface ? "lambda" : "afr"}
	 * offset 9428
	 */
	scaled_channel<uint8_t, 147, 1> lambdaTable[FUEL_LOAD_COUNT][FUEL_RPM_COUNT] = {};
	/**
	 * offset 9684
	 */
	uint16_t lambdaLoadBins[FUEL_LOAD_COUNT] = {};
	/**
	 * units: RPM
	 * offset 9716
	 */
	uint16_t lambdaRpmBins[FUEL_RPM_COUNT] = {};
	/**
	 * units: value
	 * offset 9748
	 */
	float tpsTpsAccelTable[TPS_TPS_ACCEL_TABLE][TPS_TPS_ACCEL_TABLE] = {};
	/**
	 * units: %
	 * offset 10004
	 */
	float tpsTpsAccelFromRpmBins[TPS_TPS_ACCEL_TABLE] = {};
	/**
	 * units: %
	 * offset 10036
	 */
	float tpsTpsAccelToRpmBins[TPS_TPS_ACCEL_TABLE] = {};
	/**
	 * units: value
	 * offset 10068
	 */
	float scriptTable1[SCRIPT_TABLE_8][SCRIPT_TABLE_8] = {};
	/**
	 * units: L
	 * offset 10324
	 */
	int16_t scriptTable1LoadBins[SCRIPT_TABLE_8] = {};
	/**
	 * units: RPM
	 * offset 10340
	 */
	int16_t scriptTable1RpmBins[SCRIPT_TABLE_8] = {};
	/**
	 * units: value
	 * offset 10356
	 */
	float scriptTable2[TABLE_2_LOAD_SIZE][TABLE_2_RPM_SIZE] = {};
	/**
	 * units: L
	 * offset 10612
	 */
	int16_t scriptTable2LoadBins[TABLE_2_LOAD_SIZE] = {};
	/**
	 * units: RPM
	 * offset 10628
	 */
	int16_t scriptTable2RpmBins[TABLE_2_RPM_SIZE] = {};
	/**
	 * units: value
	 * offset 10644
	 */
	uint8_t scriptTable3[TABLE_3_LOAD_SIZE][TABLE_3_RPM_SIZE] = {};
	/**
	 * units: L
	 * offset 10708
	 */
	int16_t scriptTable3LoadBins[TABLE_3_LOAD_SIZE] = {};
	/**
	 * units: RPM
	 * offset 10724
	 */
	int16_t scriptTable3RpmBins[TABLE_3_RPM_SIZE] = {};
	/**
	 * units: value
	 * offset 10740
	 */
	uint8_t scriptTable4[TABLE_4_LOAD_SIZE][TABLE_4_RPM_SIZE] = {};
	/**
	 * units: L
	 * offset 10820
	 */
	int16_t scriptTable4LoadBins[TABLE_4_LOAD_SIZE] = {};
	/**
	 * units: RPM
	 * offset 10836
	 */
	int16_t scriptTable4RpmBins[TABLE_4_RPM_SIZE] = {};
	/**
	 * units: {bitStringValue(ignLoadUnitLabels, ignLoadUnitIdxPcv)}
	 * offset 10856
	 */
	uint16_t ignTrimLoadBins[IGN_TRIM_SIZE] = {};
	/**
	 * units: rpm
	 * offset 10864
	 */
	uint16_t ignTrimRpmBins[IGN_TRIM_SIZE] = {};
	/**
	 * offset 10872
	 */
	ign_cyl_trim_s ignTrims[MAX_CYLINDER_COUNT] = {};
	/**
	 * units: {bitStringValue(veLoadUnitLabels, veLoadUnitIdxPcv)}
	 * offset 11064
	 */
	uint16_t fuelTrimLoadBins[FUEL_TRIM_SIZE] = {};
	/**
	 * units: rpm
	 * offset 11072
	 */
	uint16_t fuelTrimRpmBins[FUEL_TRIM_SIZE] = {};
	/**
	 * offset 11080
	 */
	fuel_cyl_trim_s fuelTrims[MAX_CYLINDER_COUNT] = {};
	/**
	 * units: ratio
	 * offset 11272
	 */
	scaled_channel<uint16_t, 100, 1> unusedCrankingFuelCoefE100[CRANKING_CURVE_SIZE] = {};
	/**
	 * Ethanol % axis (Y) for crankingFuelFlexTable.
	 * units: %
	 * offset 11288
	 */
	uint8_t crankingFuelFlexBins[CRANKING_FLEX_SIZE] = {};
	/**
	 * Cranking coolant multiplier as a function of coolant (X axis, shared crankingFuelBins) and ethanol % (Y axis, crankingFuelFlexBins). Used instead of crankingFuelCoef when flexCranking is enabled and a flex sensor is present.
	 * units: mult
	 * offset 11292
	 */
	scaled_channel<uint8_t, 50, 1> crankingFuelFlexTable[CRANKING_FLEX_SIZE][CRANKING_CURVE_SIZE] = {};
	/**
	 * units: Airmass
	 * offset 11324
	 */
	scaled_channel<uint8_t, 1, 5> tcu_pcAirmassBins[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11332
	 */
	uint8_t tcu_pcValsR[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11340
	 */
	uint8_t tcu_pcValsN[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11348
	 */
	uint8_t tcu_pcVals1[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11356
	 */
	uint8_t tcu_pcVals2[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11364
	 */
	uint8_t tcu_pcVals3[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11372
	 */
	uint8_t tcu_pcVals4[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11380
	 */
	uint8_t tcu_pcVals12[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11388
	 */
	uint8_t tcu_pcVals23[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11396
	 */
	uint8_t tcu_pcVals34[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11404
	 */
	uint8_t tcu_pcVals21[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11412
	 */
	uint8_t tcu_pcVals32[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 11420
	 */
	uint8_t tcu_pcVals43[TCU_TABLE_WIDTH] = {};
	/**
	 * units: TPS
	 * offset 11428
	 */
	uint8_t tcu_tccTpsBins[8] = {};
	/**
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 11436
	 */
	uint8_t tcu_tccLockSpeed[8] = {};
	/**
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 11444
	 */
	uint8_t tcu_tccUnlockSpeed[8] = {};
	/**
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 11452
	 */
	uint8_t tcu_32SpeedBins[8] = {};
	/**
	 * units: %
	 * offset 11460
	 */
	uint8_t tcu_32Vals[8] = {};
	/**
	 * units: %
	 * offset 11468
	 */
	scaled_channel<int8_t, 10, 1> throttle2TrimTable[ETB2_TRIM_SIZE][ETB2_TRIM_RPM_SIZE] = {};
	/**
	 * units: %
	 * offset 11504
	 */
	uint8_t throttle2TrimTpsBins[ETB2_TRIM_SIZE] = {};
	/**
	 * units: RPM
	 * offset 11510
	 */
	scaled_channel<uint8_t, 1, 100> throttle2TrimRpmBins[ETB2_TRIM_RPM_SIZE] = {};
	/**
	 * units: deg
	 * offset 11516
	 */
	scaled_channel<uint8_t, 4, 1> maxKnockRetardTable[KNOCK_TABLE_SIZE][KNOCK_TABLE_RPM_SIZE] = {};
	/**
	 * units: %
	 * offset 11552
	 */
	uint8_t maxKnockRetardLoadBins[KNOCK_TABLE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 11558
	 */
	scaled_channel<uint8_t, 1, 100> maxKnockRetardRpmBins[KNOCK_TABLE_RPM_SIZE] = {};
	/**
	 * units: deg
	 * offset 11564
	 */
	scaled_channel<int16_t, 10, 1> ALSTimingRetardTable[ALS_SIZE][ALS_RPM_SIZE] = {};
	/**
	 * units: TPS
	 * offset 11596
	 */
	uint16_t alsIgnRetardLoadBins[ALS_SIZE] = {};
	/**
	 * units: RPM
	 * offset 11604
	 */
	uint16_t alsIgnRetardrpmBins[ALS_RPM_SIZE] = {};
	/**
	 * units: percent
	 * offset 11612
	 */
	scaled_channel<int16_t, 10, 1> ALSFuelAdjustment[ALS_SIZE][ALS_RPM_SIZE] = {};
	/**
	 * units: TPS
	 * offset 11644
	 */
	uint16_t alsFuelAdjustmentLoadBins[ALS_SIZE] = {};
	/**
	 * units: RPM
	 * offset 11652
	 */
	uint16_t alsFuelAdjustmentrpmBins[ALS_RPM_SIZE] = {};
	/**
	 * units: ratio
	 * offset 11660
	 */
	scaled_channel<int16_t, 1, 10> ALSIgnSkipTable[ALS_SIZE][ALS_RPM_SIZE] = {};
	/**
	 * units: TPS
	 * offset 11692
	 */
	uint16_t alsIgnSkipLoadBins[ALS_SIZE] = {};
	/**
	 * units: RPM
	 * offset 11700
	 */
	uint16_t alsIgnSkiprpmBins[ALS_RPM_SIZE] = {};
	/**
	 * offset 11708
	 */
	blend_table_s_BLEND_PRECISION__1 ignBlends[IGN_BLEND_COUNT] = {};
	/**
	 * offset 12460
	 */
	blend_table_s_BLEND_PRECISION__1 veBlends[VE_BLEND_COUNT] = {};
	/**
	 * units: %
	 * offset 13212
	 */
	scaled_channel<uint16_t, 10, 1> throttleEstimateEffectiveAreaBins[THR_EST_SIZE] = {};
	/**
	 * In units of g/s normalized to choked flow conditions
	 * units: g/s
	 * offset 13236
	 */
	scaled_channel<uint16_t, 10, 1> throttleEstimateEffectiveAreaValues[THR_EST_SIZE] = {};
	/**
	 * offset 13260
	 */
	blend_table_s_BLEND_PRECISION__1 boostOpenLoopBlends[BOOST_BLEND_COUNT] = {};
	/**
	 * offset 13636
	 */
	blend_table_s_BLEND_PRECISION__1 boostClosedLoopBlends[BOOST_BLEND_COUNT] = {};
	/**
	 * units: level
	 * offset 14012
	 */
	float tcu_rangeP[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 14036
	 */
	float tcu_rangeR[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 14060
	 */
	float tcu_rangeN[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 14084
	 */
	float tcu_rangeD[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 14108
	 */
	float tcu_rangeM[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 14132
	 */
	float tcu_rangeM3[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 14156
	 */
	float tcu_rangeM2[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 14180
	 */
	float tcu_rangeM1[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 14204
	 */
	float tcu_rangePlus[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 14228
	 */
	float tcu_rangeMinus[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 14252
	 */
	float tcu_rangeLow[RANGE_INPUT_COUNT] = {};
	/**
	 * units: lambda
	 * offset 14276
	 */
	scaled_channel<uint8_t, 100, 1> lambdaMaxDeviationTable[LAM_SIZE][LAM_RPM_SIZE] = {};
	/**
	 * offset 14292
	 */
	uint16_t lambdaMaxDeviationLoadBins[LAM_SIZE] = {};
	/**
	 * units: RPM
	 * offset 14300
	 */
	uint16_t lambdaMaxDeviationRpmBins[LAM_RPM_SIZE] = {};
	/**
	 * units: %
	 * offset 14308
	 */
	uint8_t injectorStagingTable[INJ_STAGING_COUNT][INJ_STAGING_RPM_SIZE] = {};
	/**
	 * offset 14344
	 */
	uint16_t injectorStagingLoadBins[INJ_STAGING_COUNT] = {};
	/**
	 * units: RPM
	 * offset 14356
	 */
	uint16_t injectorStagingRpmBins[INJ_STAGING_RPM_SIZE] = {};
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 14368
	 */
	int16_t wwCltBins[WWAE_TABLE_SIZE] = {};
	/**
	 * offset 14384
	 */
	scaled_channel<uint8_t, 100, 1> wwTauCltValues[WWAE_TABLE_SIZE] = {};
	/**
	 * offset 14392
	 */
	scaled_channel<uint8_t, 100, 1> wwBetaCltValues[WWAE_TABLE_SIZE] = {};
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 14400
	 */
	uint8_t wwMapBins[WWAE_TABLE_SIZE] = {};
	/**
	 * offset 14408
	 */
	scaled_channel<uint8_t, 100, 1> wwTauMapValues[WWAE_TABLE_SIZE] = {};
	/**
	 * offset 14416
	 */
	scaled_channel<uint8_t, 100, 1> wwBetaMapValues[WWAE_TABLE_SIZE] = {};
	/**
	 * units: %
	 * offset 14424
	 */
	scaled_channel<uint8_t, 2, 1> hpfpLobeProfileQuantityBins[HPFP_LOBE_PROFILE_SIZE] = {};
	/**
	 * units: deg
	 * offset 14440
	 */
	scaled_channel<uint8_t, 2, 1> hpfpLobeProfileAngle[HPFP_LOBE_PROFILE_SIZE] = {};
	/**
	 * units: volts
	 * offset 14456
	 */
	uint8_t hpfpDeadtimeVoltsBins[HPFP_DEADTIME_SIZE] = {};
	/**
	 * units: ms
	 * offset 14464
	 */
	scaled_channel<uint16_t, 1000, 1> hpfpDeadtimeMS[HPFP_DEADTIME_SIZE] = {};
	/**
	 * units: kPa
	 * offset 14480
	 */
	uint16_t hpfpTarget[HPFP_TARGET_SIZE][HPFP_TARGET_SIZE] = {};
	/**
	 * units: load
	 * offset 14680
	 */
	scaled_channel<uint16_t, 10, 1> hpfpTargetLoadBins[HPFP_TARGET_SIZE] = {};
	/**
	 * units: RPM
	 * offset 14700
	 */
	scaled_channel<uint8_t, 1, 50> hpfpTargetRpmBins[HPFP_TARGET_SIZE] = {};
	/**
	 * units: %
	 * offset 14710
	 */
	int8_t hpfpCompensation[HPFP_COMPENSATION_SIZE][HPFP_COMPENSATION_RPM_SIZE] = {};
	/**
	 * units: cc/lobe
	 * offset 14810
	 */
	scaled_channel<uint16_t, 1000, 1> hpfpCompensationLoadBins[HPFP_COMPENSATION_SIZE] = {};
	/**
	 * units: RPM
	 * offset 14830
	 */
	scaled_channel<uint8_t, 1, 50> hpfpCompensationRpmBins[HPFP_COMPENSATION_RPM_SIZE] = {};
	/**
	 * units: %
	 * offset 14840
	 */
	scaled_channel<uint16_t, 100, 1> hpfpFuelMassCompensation[HPFP_FUEL_MASS_COMPENSATION_SIZE][HPFP_FUEL_MASS_COMPENSATION_SIZE] = {};
	/**
	 * units: fuel mass/mg
	 * offset 14968
	 */
	scaled_channel<uint16_t, 100, 1> hpfpFuelMassCompensationFuelMass[HPFP_FUEL_MASS_COMPENSATION_SIZE] = {};
	/**
	 * units: bar
	 * offset 14984
	 */
	scaled_channel<uint16_t, 10, 1> hpfpFuelMassCompensationFuelPressure[HPFP_FUEL_MASS_COMPENSATION_SIZE] = {};
	/**
	 * units: ms
	 * offset 15000
	 */
	scaled_channel<uint16_t, 100, 1> injectorFlowLinearization[FLOW_LINEARIZATION_PRESSURE_SIZE][FLOW_LINEARIZATION_MASS_SIZE] = {};
	/**
	 * units: fuel mass/mg
	 * offset 15008
	 */
	scaled_channel<uint16_t, 100, 1> injectorFlowLinearizationFuelMassBins[FLOW_LINEARIZATION_MASS_SIZE] = {};
	/**
	 * units: bar
	 * offset 15012
	 */
	scaled_channel<uint16_t, 10, 1> injectorFlowLinearizationPressureBins[FLOW_LINEARIZATION_PRESSURE_SIZE] = {};
	/**
	 * Multiplicative correction applied on top of the theoretical (uncompensated) injection duration, eg. 1.20 = add 20% pulse width at this point. Only used in Manual Pressure Correction injector compensation mode.
	 * units: mult
	 * offset 15016
	 */
	scaled_channel<uint16_t, 1000, 1> manualPressureCorrection[MANUAL_PRESSURE_CORRECTION_PRESSURE_SIZE][MANUAL_PRESSURE_CORRECTION_MASS_SIZE] = {};
	/**
	 * units: fuel mass/mg
	 * offset 15024
	 */
	scaled_channel<uint16_t, 100, 1> manualPressureCorrectionFuelMassBins[MANUAL_PRESSURE_CORRECTION_MASS_SIZE] = {};
	/**
	 * units: kPa
	 * offset 15028
	 */
	scaled_channel<uint16_t, 1, 1> manualPressureCorrectionPressureBins[MANUAL_PRESSURE_CORRECTION_PRESSURE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 15032
	 */
	uint16_t knockNoiseRpmBins[ENGINE_NOISE_CURVE_SIZE] = {};
	/**
	 * Knock sensor output knock detection threshold depending on current RPM.
	 * units: dB
	 * offset 15064
	 */
	scaled_channel<int8_t, 2, 1> knockBaseNoise[ENGINE_NOISE_CURVE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 15080
	 */
	scaled_channel<uint8_t, 1, 50> tpsTspCorrValuesBins[TPS_TPS_ACCEL_CLT_CORR_TABLE] = {};
	/**
	 * units: multiplier
	 * offset 15084
	 */
	scaled_channel<uint8_t, 50, 1> tpsTspCorrValues[TPS_TPS_ACCEL_CLT_CORR_TABLE] = {};
	/**
	 * units: RPM
	 * offset 15088
	 */
	scaled_channel<uint8_t, 1, 50> predictiveMapBlendDurationBins[TPS_TPS_ACCEL_CLT_CORR_TABLE] = {};
	/**
	 * units: second
	 * offset 15092
	 */
	scaled_channel<uint8_t, 50, 1> predictiveMapBlendDurationValues[TPS_TPS_ACCEL_CLT_CORR_TABLE] = {};
	/**
	 * Coolant temperature axis for the flex-fuel transient compensation tables
	 * units: C
	 * offset 15096
	 */
	int16_t flexTransientCltBins[FLEX_TRANSIENT_CLT_SIZE] = {};
	/**
	 * Ethanol percentage axis for the flex-fuel transient compensation tables
	 * units: %
	 * offset 15112
	 */
	uint8_t flexTransientEthanolBins[FLEX_TRANSIENT_ETH_SIZE] = {};
	/**
	 * Acceleration enrichment multiplier as a function of CLT (X) and ethanol % (Y)
	 * units: mult
	 * offset 15120
	 */
	scaled_channel<uint8_t, 50, 1> flexAeMult[FLEX_TRANSIENT_ETH_SIZE][FLEX_TRANSIENT_CLT_SIZE] = {};
	/**
	 * Wall wetting tau multiplier as a function of CLT (X) and ethanol % (Y)
	 * units: mult
	 * offset 15184
	 */
	scaled_channel<uint8_t, 50, 1> flexWwTauMult[FLEX_TRANSIENT_ETH_SIZE][FLEX_TRANSIENT_CLT_SIZE] = {};
	/**
	 * Wall wetting beta multiplier as a function of CLT (X) and ethanol % (Y)
	 * units: mult
	 * offset 15248
	 */
	scaled_channel<uint8_t, 50, 1> flexWwBetaMult[FLEX_TRANSIENT_ETH_SIZE][FLEX_TRANSIENT_CLT_SIZE] = {};
	/**
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 15312
	 */
	scaled_channel<int16_t, 1, 1> cltRevLimitRpmBins[CLT_LIMITER_CURVE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 15320
	 */
	uint16_t cltRevLimitRpm[CLT_LIMITER_CURVE_SIZE] = {};
	/**
	 * units: volt
	 * offset 15328
	 */
	scaled_channel<uint16_t, 1000, 1> fuelLevelBins[FUEL_LEVEL_TABLE_COUNT] = {};
	/**
	 * units: %
	 * offset 15344
	 */
	uint8_t fuelLevelValues[FUEL_LEVEL_TABLE_COUNT] = {};
	/**
	 * units: volts
	 * offset 15352
	 */
	scaled_channel<uint8_t, 10, 1> dwellVoltageCorrVoltBins[DWELL_CURVE_SIZE] = {};
	/**
	 * units: multiplier
	 * offset 15360
	 */
	scaled_channel<uint8_t, 50, 1> dwellVoltageCorrValues[DWELL_CURVE_SIZE] = {};
	/**
	 * units: %
	 * offset 15368
	 */
	scaled_channel<uint8_t, 1, 1> tcu_shiftTpsBins[TCU_TABLE_WIDTH] = {};
	/**
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 15376
	 */
	uint8_t tcu_shiftSpeed12[TCU_TABLE_WIDTH] = {};
	/**
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 15384
	 */
	uint8_t tcu_shiftSpeed23[TCU_TABLE_WIDTH] = {};
	/**
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 15392
	 */
	uint8_t tcu_shiftSpeed34[TCU_TABLE_WIDTH] = {};
	/**
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 15400
	 */
	uint8_t tcu_shiftSpeed21[TCU_TABLE_WIDTH] = {};
	/**
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 15408
	 */
	uint8_t tcu_shiftSpeed32[TCU_TABLE_WIDTH] = {};
	/**
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 15416
	 */
	uint8_t tcu_shiftSpeed43[TCU_TABLE_WIDTH] = {};
	/**
	 * units: ms
	 * offset 15424
	 */
	float tcu_shiftTime;
	/**
	offset 15428 bit 0 */
	bool tcuIdleShiftToFirstEnabled : 1 {};
	/**
	offset 15428 bit 1 */
	bool unusedBit_249_1 : 1 {};
	/**
	offset 15428 bit 2 */
	bool unusedBit_249_2 : 1 {};
	/**
	offset 15428 bit 3 */
	bool unusedBit_249_3 : 1 {};
	/**
	offset 15428 bit 4 */
	bool unusedBit_249_4 : 1 {};
	/**
	offset 15428 bit 5 */
	bool unusedBit_249_5 : 1 {};
	/**
	offset 15428 bit 6 */
	bool unusedBit_249_6 : 1 {};
	/**
	offset 15428 bit 7 */
	bool unusedBit_249_7 : 1 {};
	/**
	offset 15428 bit 8 */
	bool unusedBit_249_8 : 1 {};
	/**
	offset 15428 bit 9 */
	bool unusedBit_249_9 : 1 {};
	/**
	offset 15428 bit 10 */
	bool unusedBit_249_10 : 1 {};
	/**
	offset 15428 bit 11 */
	bool unusedBit_249_11 : 1 {};
	/**
	offset 15428 bit 12 */
	bool unusedBit_249_12 : 1 {};
	/**
	offset 15428 bit 13 */
	bool unusedBit_249_13 : 1 {};
	/**
	offset 15428 bit 14 */
	bool unusedBit_249_14 : 1 {};
	/**
	offset 15428 bit 15 */
	bool unusedBit_249_15 : 1 {};
	/**
	offset 15428 bit 16 */
	bool unusedBit_249_16 : 1 {};
	/**
	offset 15428 bit 17 */
	bool unusedBit_249_17 : 1 {};
	/**
	offset 15428 bit 18 */
	bool unusedBit_249_18 : 1 {};
	/**
	offset 15428 bit 19 */
	bool unusedBit_249_19 : 1 {};
	/**
	offset 15428 bit 20 */
	bool unusedBit_249_20 : 1 {};
	/**
	offset 15428 bit 21 */
	bool unusedBit_249_21 : 1 {};
	/**
	offset 15428 bit 22 */
	bool unusedBit_249_22 : 1 {};
	/**
	offset 15428 bit 23 */
	bool unusedBit_249_23 : 1 {};
	/**
	offset 15428 bit 24 */
	bool unusedBit_249_24 : 1 {};
	/**
	offset 15428 bit 25 */
	bool unusedBit_249_25 : 1 {};
	/**
	offset 15428 bit 26 */
	bool unusedBit_249_26 : 1 {};
	/**
	offset 15428 bit 27 */
	bool unusedBit_249_27 : 1 {};
	/**
	offset 15428 bit 28 */
	bool unusedBit_249_28 : 1 {};
	/**
	offset 15428 bit 29 */
	bool unusedBit_249_29 : 1 {};
	/**
	offset 15428 bit 30 */
	bool unusedBit_249_30 : 1 {};
	/**
	offset 15428 bit 31 */
	bool unusedBit_249_31 : 1 {};
	/**
	 * Idle-shift VSS threshold. A value of 0 disables the speed check entirely, so only the idle RPM/TPS condition is required.
	 * units: km/h
	 * offset 15432
	 */
	uint8_t tcuIdleShiftToFirstMaxVss;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 15433
	 */
	uint8_t alignmentFill_at_15433[1] = {};
	/**
	 * units: volts
	 * offset 15434
	 */
	scaled_channel<int16_t, 10, 1> alternatorVoltageTargetTable[ALTERNATOR_VOLTAGE_TARGET_SIZE][ALTERNATOR_VOLTAGE_RPM_SIZE] = {};
	/**
	 * units: Load
	 * offset 15466
	 */
	uint16_t alternatorVoltageTargetLoadBins[ALTERNATOR_VOLTAGE_TARGET_SIZE] = {};
	/**
	 * units: RPM
	 * offset 15474
	 */
	uint16_t alternatorVoltageTargetRpmBins[ALTERNATOR_VOLTAGE_RPM_SIZE] = {};
	/**
	 * Base duty (open-loop feedforward) for alternator PWM, indexed by target voltage (Y) vs RPM (X).
	 * units: %
	 * offset 15482
	 */
	uint8_t alternatorBaseDutyTable[ALTERNATOR_VOLTAGE_TARGET_SIZE][ALTERNATOR_VOLTAGE_RPM_SIZE] = {};
	/**
	 * units: V
	 * offset 15498
	 */
	scaled_channel<uint8_t, 10, 1> alternatorBaseDutyVoltageBins[ALTERNATOR_VOLTAGE_TARGET_SIZE] = {};
	/**
	 * units: RPM
	 * offset 15502
	 */
	uint16_t alternatorBaseDutyRpmBins[ALTERNATOR_VOLTAGE_RPM_SIZE] = {};
	/**
	 * Target fuel pressure vs RPM and MAP load (PWM mode)
	 * units: kPa
	 * offset 15510
	 */
	scaled_channel<int16_t, 10, 1> fuelPressureTargetTable[FP_PRESSURE_TABLE_SIZE][FP_PRESSURE_RPM_SIZE] = {};
	/**
	 * MAP load bins for target FP pressure table
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 15542
	 */
	uint16_t fuelPressureTargetLoadBins[FP_PRESSURE_TABLE_SIZE] = {};
	/**
	 * RPM bins for target FP pressure table
	 * units: rpm
	 * offset 15550
	 */
	uint16_t fuelPressureTargetRpmBins[FP_PRESSURE_RPM_SIZE] = {};
	/**
	 * Feedforward base duty vs RPM and target FP pressure (PWM mode)
	 * units: pct
	 * offset 15558
	 */
	scaled_channel<int16_t, 10, 1> fuelPumpBaseDutyTable[FP_DUTY_TABLE_SIZE][FP_DUTY_RPM_SIZE] = {};
	/**
	 * Target FP bins for base duty table
	 * units: kPa
	 * offset 15590
	 */
	uint16_t fuelPumpBaseDutyFpBins[FP_DUTY_TABLE_SIZE] = {};
	/**
	 * RPM bins for base duty table
	 * units: rpm
	 * offset 15598
	 */
	uint16_t fuelPumpBaseDutyRpmBins[FP_DUTY_RPM_SIZE] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 15606
	 */
	uint8_t alignmentFill_at_15606[2] = {};
	/**
	 * units: C
	 * offset 15608
	 */
	float cltBoostCorrBins[BOOST_CURVE_SIZE] = {};
	/**
	 * units: ratio
	 * offset 15628
	 */
	float cltBoostCorr[BOOST_CURVE_SIZE] = {};
	/**
	 * units: C
	 * offset 15648
	 */
	float iatBoostCorrBins[BOOST_CURVE_SIZE] = {};
	/**
	 * units: ratio
	 * offset 15668
	 */
	float iatBoostCorr[BOOST_CURVE_SIZE] = {};
	/**
	 * units: C
	 * offset 15688
	 */
	float cltBoostAdderBins[BOOST_CURVE_SIZE] = {};
	/**
	 * offset 15708
	 */
	float cltBoostAdder[BOOST_CURVE_SIZE] = {};
	/**
	 * units: C
	 * offset 15728
	 */
	float iatBoostAdderBins[BOOST_CURVE_SIZE] = {};
	/**
	 * offset 15748
	 */
	float iatBoostAdder[BOOST_CURVE_SIZE] = {};
	/**
	 * "Minimum Battery Voltage"
	 * units: #
	 * offset 15768
	 */
	scaled_channel<uint8_t, 10, 1> cel_battery_min_v;
	/**
	 * "Maximum Battery Voltage"
	 * units: #
	 * offset 15769
	 */
	scaled_channel<uint8_t, 10, 1> cel_battery_max_v;
	/**
	 * "Minimum MAP V"
	 * units: V
	 * offset 15770
	 */
	scaled_channel<uint8_t, 50, 1> cel_map_min_v;
	/**
	 * "Maximum MAP V"
	 * units: V
	 * offset 15771
	 */
	scaled_channel<uint8_t, 50, 1> cel_map_max_v;
	/**
	 * "Minimum IAT V"
	 * units: V
	 * offset 15772
	 */
	scaled_channel<uint8_t, 50, 1> cel_iat_min_v;
	/**
	 * "Maximum IAT V"
	 * units: V
	 * offset 15773
	 */
	scaled_channel<uint8_t, 50, 1> cel_iat_max_v;
	/**
	 * "Minimum TPS V"
	 * units: V
	 * offset 15774
	 */
	scaled_channel<uint8_t, 50, 1> cel_tps_min_v;
	/**
	 * "Maximum TPS V"
	 * units: V
	 * offset 15775
	 */
	scaled_channel<uint8_t, 50, 1> cel_tps_max_v;
	/**
	 * units: RPM
	 * offset 15776
	 */
	scaled_channel<uint8_t, 1, 100> minimumOilPressureBins[8] = {};
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 15784
	 */
	scaled_channel<uint8_t, 1, 10> minimumOilPressureValues[8] = {};
	/**
	 * offset 15792
	 */
	blend_table_s_TARGET_AFR_BLEND_PRECISION__2 targetAfrBlends[TARGET_AFR_BLEND_COUNT] = {};
	/**
	 * @@DYNO_RPM_STEP_TOOLTIP@@
	 * units: Rpm
	 * offset 16168
	 */
	scaled_channel<uint8_t, 1, 1> dynoRpmStep;
	/**
	 * @@DYNO_SAE_TEMPERATURE_C_TOOLTIP@@
	 * units: C
	 * offset 16169
	 */
	scaled_channel<int8_t, 1, 1> dynoSaeTemperatureC;
	/**
	 * @@DYNO_SAE_RELATIVE_HUMIDITY_TOOLTIP@@
	 * units: %
	 * offset 16170
	 */
	scaled_channel<uint8_t, 1, 1> dynoSaeRelativeHumidity;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 16171
	 */
	uint8_t alignmentFill_at_16171[1] = {};
	/**
	 * @@DYNO_SAE_BARO_TOOLTIP@@
	 * units: KPa
	 * offset 16172
	 */
	scaled_channel<float, 1, 1> dynoSaeBaro;
	/**
	 * @@DYNO_CAR_WHEEL_DIA_INCH_TOOLTIP@@
	 * units: Inch
	 * offset 16176
	 */
	scaled_channel<int8_t, 1, 1> dynoCarWheelDiaInch;
	/**
	 * @@DYNO_CAR_WHEEL_ASPECT_RATIO_TOOLTIP@@
	 * units: Aspect Ratio (height)
	 * offset 16177
	 */
	scaled_channel<int8_t, 1, 1> dynoCarWheelAspectRatio;
	/**
	 * @@DYNO_CAR_WHEEL_TIRE_WIDTH_TOOLTIP@@
	 * units: Width mm
	 * offset 16178
	 */
	scaled_channel<int16_t, 1, 1> dynoCarWheelTireWidthMm;
	/**
	 * @@DYNO_CAR_GEAR_PRIMARY_REDUCTION_TOOLTIP@@
	 * units: Units
	 * offset 16180
	 */
	scaled_channel<float, 1, 1> dynoCarGearPrimaryReduction;
	/**
	 * @@DYNO_CAR_GEAR_RATIO_TOOLTIP@@
	 * units: Units
	 * offset 16184
	 */
	scaled_channel<float, 1, 1> dynoCarGearRatio;
	/**
	 * @@DYNO_CAR_GEAR_FINAL_DRIVE_TOOLTIP@@
	 * units: Units
	 * offset 16188
	 */
	scaled_channel<float, 1, 1> dynoCarGearFinalDrive;
	/**
	 * @@DYNO_CAR_CAR_MASS_TOOLTIP@@
	 * units: Kg
	 * offset 16192
	 */
	scaled_channel<int16_t, 1, 1> dynoCarCarMassKg;
	/**
	 * @@DYNO_CAR_CARGO_MASS_TOOLTIP@@
	 * units: Kg
	 * offset 16194
	 */
	scaled_channel<int16_t, 1, 1> dynoCarCargoMassKg;
	/**
	 * @@DYNO_CAR_COEFF_OF_DRAG_TOOLTIP@@
	 * units: Coeff
	 * offset 16196
	 */
	scaled_channel<float, 1, 1> dynoCarCoeffOfDrag;
	/**
	 * @@DYNO_CAR_FRONTAL_AREA_TOOLTIP@@
	 * units: m2
	 * offset 16200
	 */
	scaled_channel<float, 1, 1> dynoCarFrontalAreaM2;
	/**
	 * units: deg
	 * offset 16204
	 */
	scaled_channel<int8_t, 10, 1> trailingSparkTable[TRAILING_SPARK_SIZE][TRAILING_SPARK_RPM_SIZE] = {};
	/**
	 * units: rpm
	 * offset 16220
	 */
	scaled_channel<uint8_t, 1, 50> trailingSparkRpmBins[TRAILING_SPARK_RPM_SIZE] = {};
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 16224
	 */
	scaled_channel<uint8_t, 1, 5> trailingSparkLoadBins[TRAILING_SPARK_SIZE] = {};
	/**
	 * units: RPM
	 * offset 16228
	 */
	scaled_channel<uint8_t, 1, 100> maximumOilPressureBins[4] = {};
	/**
	 * units: {bitStringValue(pressureUnitsLabels, useMetricOnInterface)}
	 * offset 16232
	 */
	scaled_channel<uint8_t, 1, 10> maximumOilPressureValues[4] = {};
	/**
	 * Selects the X axis to use for the table.
	 * offset 16236
	 */
	gppwm_channel_e torqueReductionCutXaxis;
	/**
	 * How many % of ignition events will be cut
	 * units: %
	 * offset 16237
	 */
	int8_t torqueReductionIgnitionCutTable[TORQUE_TABLE_Y_SIZE][TORQUE_TABLE_X_SIZE] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 16249
	 */
	uint8_t alignmentFill_at_16249[1] = {};
	/**
	 * offset 16250
	 */
	int16_t torqueReductionCutXBins[TORQUE_TABLE_X_SIZE] = {};
	/**
	 * units: gear N°
	 * offset 16262
	 */
	int8_t torqueReductionCutGearBins[TORQUE_TABLE_Y_SIZE] = {};
	/**
	 * Selects the X axis to use for the table.
	 * offset 16264
	 */
	gppwm_channel_e torqueReductionTimeXaxis;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 16265
	 */
	uint8_t alignmentFill_at_16265[3] = {};
	/**
	 * For how long after the pin has been triggered will the cut/reduction stay active. After that, even if the pin is still triggered, torque is re-introduced
	 * units: ms
	 * offset 16268
	 */
	float torqueReductionTimeTable[TORQUE_TABLE_Y_SIZE][TORQUE_TABLE_X_SIZE] = {};
	/**
	 * offset 16316
	 */
	int16_t torqueReductionTimeXBins[TORQUE_TABLE_X_SIZE] = {};
	/**
	 * units: gear N°
	 * offset 16328
	 */
	int8_t torqueReductionTimeGearBins[TORQUE_TABLE_Y_SIZE] = {};
	/**
	 * Selects the X axis to use for the table.
	 * offset 16330
	 */
	gppwm_channel_e torqueReductionIgnitionRetardXaxis;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 16331
	 */
	uint8_t alignmentFill_at_16331[1] = {};
	/**
	 * How many degrees of timing advance will be reduced during the Torque Reduction Time
	 * units: deg
	 * offset 16332
	 */
	float torqueReductionIgnitionRetardTable[TORQUE_TABLE_Y_SIZE][TORQUE_TABLE_X_SIZE] = {};
	/**
	 * offset 16380
	 */
	int16_t torqueReductionIgnitionRetardXBins[TORQUE_TABLE_X_SIZE] = {};
	/**
	 * units: gear N°
	 * offset 16392
	 */
	int8_t torqueReductionIgnitionRetardGearBins[TORQUE_TABLE_Y_SIZE] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 16394
	 */
	uint8_t alignmentFill_at_16394[2] = {};
	/**
	offset 16396 bit 0 */
	bool wizardNumberOfCylinders : 1 {};
	/**
	offset 16396 bit 1 */
	bool wizardFiringOrder : 1 {};
	/**
	offset 16396 bit 2 */
	bool wizardMapSensorType : 1 {};
	/**
	offset 16396 bit 3 */
	bool wizardCrankTrigger : 1 {};
	/**
	offset 16396 bit 4 */
	bool wizardCamTrigger : 1 {};
	/**
	offset 16396 bit 5 */
	bool wizardInjectorFlow : 1 {};
	/**
	offset 16396 bit 6 */
	bool wizardDisplacement : 1 {};
	/**
	offset 16396 bit 7 */
	bool wizardCltSensor : 1 {};
	/**
	offset 16396 bit 8 */
	bool wizardTps : 1 {};
	/**
	offset 16396 bit 9 */
	bool wizardIgnitionOutputs : 1 {};
	/**
	offset 16396 bit 10 */
	bool wizardInjectorOutputs : 1 {};
	/**
	offset 16396 bit 11 */
	bool unusedBit_361_11 : 1 {};
	/**
	offset 16396 bit 12 */
	bool unusedBit_361_12 : 1 {};
	/**
	offset 16396 bit 13 */
	bool unusedBit_361_13 : 1 {};
	/**
	offset 16396 bit 14 */
	bool unusedBit_361_14 : 1 {};
	/**
	offset 16396 bit 15 */
	bool unusedBit_361_15 : 1 {};
	/**
	offset 16396 bit 16 */
	bool unusedBit_361_16 : 1 {};
	/**
	offset 16396 bit 17 */
	bool unusedBit_361_17 : 1 {};
	/**
	offset 16396 bit 18 */
	bool unusedBit_361_18 : 1 {};
	/**
	offset 16396 bit 19 */
	bool unusedBit_361_19 : 1 {};
	/**
	offset 16396 bit 20 */
	bool unusedBit_361_20 : 1 {};
	/**
	offset 16396 bit 21 */
	bool unusedBit_361_21 : 1 {};
	/**
	offset 16396 bit 22 */
	bool unusedBit_361_22 : 1 {};
	/**
	offset 16396 bit 23 */
	bool unusedBit_361_23 : 1 {};
	/**
	offset 16396 bit 24 */
	bool unusedBit_361_24 : 1 {};
	/**
	offset 16396 bit 25 */
	bool unusedBit_361_25 : 1 {};
	/**
	offset 16396 bit 26 */
	bool unusedBit_361_26 : 1 {};
	/**
	offset 16396 bit 27 */
	bool unusedBit_361_27 : 1 {};
	/**
	offset 16396 bit 28 */
	bool unusedBit_361_28 : 1 {};
	/**
	offset 16396 bit 29 */
	bool unusedBit_361_29 : 1 {};
	/**
	offset 16396 bit 30 */
	bool unusedBit_361_30 : 1 {};
	/**
	offset 16396 bit 31 */
	bool unusedBit_361_31 : 1 {};
	/**
	 * offset 16400
	 */
	Gpio communityCommsLedPin;
	/**
	 * offset 16402
	 */
	scaled_channel<uint8_t, 1, 10> knockGainLoadBins[6] = {};
	/**
	 * units: RPM
	 * offset 16408
	 */
	scaled_channel<uint8_t, 1, 100> knockGainRpmBins[6] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 16414
	 */
	uint8_t alignmentFill_at_16414[2] = {};
	/**
	 * offset 16416
	 */
	KnockGain knockGains[MAX_CYLINDER_COUNT] = {};
};
static_assert(sizeof(persistent_config_s) == 16848);

// end
// this section was generated automatically by rusEFI tool config_definition-all.jar based on (unknown script) integration/rusefi_config.txt
