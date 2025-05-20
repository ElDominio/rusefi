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
	int8_t maxAdd;
	/**
	 * Maximum % that the short term fuel trim can remove
	 * units: %
	 * offset 1
	 */
	int8_t maxRemove;
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
	 * When tuning by MAP the units are kPa, e.g. 30 would mean 30kPa. When tuning TPS, 30 would be 30%
	 * units: load
	 * offset 2
	 */
	uint16_t maxOverrunLoad;
	/**
	 * Above this engine load, the power region is active
	 * When tuning by MAP the units are kPa
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
	 * Below this temperature, correction is disabled.
	 * units: C
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
	 * Delay after starting the engine before beginning closed loop correction.
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
	 * kPa value at low volts
	 * units: kpa
	 * offset 0
	 */
	float lowValue;
	/**
	 * kPa value at high volts
	 * units: kpa
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
	 * offset 16
	 */
	float samplingAngle[MAP_ANGLE_SIZE] = {};
	/**
	 * offset 32
	 */
	float samplingWindowBins[MAP_WINDOW_SIZE] = {};
	/**
	 * MAP averaging angle crank degree duration
	 * units: deg
	 * offset 64
	 */
	float samplingWindow[MAP_WINDOW_SIZE] = {};
	/**
	 * offset 96
	 */
	air_pressure_sensor_config_s sensor;
};
static_assert(sizeof(MAP_sensor_config_s) == 108);

/**
 * @brief Thermistor known values

*/
// start of thermistor_conf_s
struct thermistor_conf_s {
	/**
	 * these values are in Celcius
	 * units: *C
	 * offset 0
	 */
	float tempC_1;
	/**
	 * units: *C
	 * offset 4
	 */
	float tempC_2;
	/**
	 * units: *C
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
	 * units: kPa
	 * offset 16
	 */
	scaled_channel<uint32_t, 10, 1> battLagCorrPressBins[VBAT_INJECTOR_CURVE_PRESSURE_SIZE] = {};
	/**
	 * ms delay between injector open and close dead times
	 * units: ms
	 * offset 24
	 */
	scaled_channel<int16_t, 100, 1> battLagCorrTable[VBAT_INJECTOR_CURVE_PRESSURE_SIZE][VBAT_INJECTOR_CURVE_SIZE] = {};
};
static_assert(sizeof(injector_s) == 48);

// start of trigger_config_s
struct trigger_config_s {
	/**
	 * https://github.com/rusefi/rusefi/wiki/All-Supported-Triggers
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
	 * A secondary Rev limit engaged by the driver to help launch the vehicle faster
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
	 * Disable multispark above this engine speed.
	 * units: rpm
	 * offset 10
	 */
	scaled_channel<uint8_t, 1, 50> multisparkMaxRpm;
	/**
	 * Above this RPM, disable AC. Set to 0 to disable check.
	 * units: rpm
	 * offset 11
	 */
	scaled_channel<uint8_t, 1, 50> maxAcRpm;
	/**
	 * Above this TPS, disable AC. Set to 0 to disable check.
	 * units: %
	 * offset 12
	 */
	uint8_t maxAcTps;
	/**
	 * Above this CLT, disable AC to prevent overheating the engine. Set to 0 to disable check.
	 * units: deg C
	 * offset 13
	 */
	uint8_t maxAcClt;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 14
	 */
	uint8_t alignmentFill_at_14[2] = {};
	/**
	 * Just for reference really, not taken into account by any logic at this point
	 * units: CR
	 * offset 16
	 */
	float compressionRatio;
	/**
	 * offset 20
	 */
	uint16_t unusedWastgate0;
	/**
	 * offset 22
	 */
	uint16_t unusedWastgate1;
	/**
	 * Voltage when the idle valve is closed.
	 * You probably don't have one of these!
	 * units: mv
	 * offset 24
	 */
	uint16_t idlePositionMin;
	/**
	 * Voltage when the idle valve is open.
	 * You probably don't have one of these!
	 * 1 volt = 1000 units
	 * units: mv
	 * offset 26
	 */
	uint16_t idlePositionMax;
	/**
	 * offset 28
	 */
	output_pin_e mainRelayPin;
	/**
	 * offset 30
	 */
	Gpio sdCardCsPin;
	/**
	 * offset 32
	 */
	Gpio canTxPin;
	/**
	 * offset 34
	 */
	Gpio canRxPin;
	/**
	 * Pin that activates the reduction/cut for shifting. Sometimes shared with the Launch Control pin
	 * offset 36
	 */
	switch_input_pin_e torqueReductionTriggerPin;
	/**
	 * units: %
	 * offset 38
	 */
	int8_t launchFuelAdderPercent;
	/**
	 * Time after which the throttle is considered jammed.
	 * units: sec
	 * offset 39
	 */
	scaled_channel<uint8_t, 50, 1> etbJamTimeout;
	/**
	 * offset 40
	 */
	output_pin_e tachOutputPin;
	/**
	 * offset 42
	 */
	pin_output_mode_e tachOutputPinMode;
	/**
	 * Additional idle % while A/C is active
	 * units: %
	 * offset 43
	 */
	uint8_t acIdleExtraOffset;
	/**
	 * This parameter sets the latest that the last multispark can occur after the main ignition event. For example, if the ignition timing is 30 degrees BTDC, and this parameter is set to 45, no multispark will ever be fired after 15 degrees ATDC.
	 * units: deg
	 * offset 44
	 */
	uint8_t multisparkMaxSparkingAngle;
	/**
	 * Configures the maximum number of extra sparks to fire (does not include main spark)
	 * units: count
	 * offset 45
	 */
	uint8_t multisparkMaxExtraSparkCount;
	/**
	 * units: RPM
	 * offset 46
	 */
	int16_t vvtControlMinRpm;
	/**
	 * offset 48
	 */
	injector_s injector;
	/**
	 * offset 96
	 */
	injector_s injectorSecondary;
	/**
	 * Does the vehicle have a turbo or supercharger?
	offset 144 bit 0 */
	bool isForcedInduction : 1 {};
	/**
	 * On some Ford and Toyota vehicles one of the throttle sensors is not linear on the full range, i.e. in the specific range of the positions we effectively have only one sensor.
	offset 144 bit 1 */
	bool useFordRedundantTps : 1 {};
	/**
	offset 144 bit 2 */
	bool enableKline : 1 {};
	/**
	offset 144 bit 3 */
	bool overrideTriggerGaps : 1 {};
	/**
	 * Turn on this fan when AC is on.
	offset 144 bit 4 */
	bool enableFan1WithAc : 1 {};
	/**
	 * Turn on this fan when AC is on.
	offset 144 bit 5 */
	bool enableFan2WithAc : 1 {};
	/**
	 * Inhibit operation of this fan while the engine is not running.
	offset 144 bit 6 */
	bool disableFan1WhenStopped : 1 {};
	/**
	 * Inhibit operation of this fan while the engine is not running.
	offset 144 bit 7 */
	bool disableFan2WhenStopped : 1 {};
	/**
	 * Enable secondary spark outputs that fire after the primary (rotaries, twin plug engines).
	offset 144 bit 8 */
	bool enableTrailingSparks : 1 {};
	/**
	 * TLE7209 and L6205 use two-wire mode. TLE9201 and VNH2SP30 do NOT use two wire mode.
	offset 144 bit 9 */
	bool etb_use_two_wires : 1 {};
	/**
	 * Subaru/BMW style where default valve position is somewhere in the middle. First solenoid opens it more while second can close it more than default position.
	offset 144 bit 10 */
	bool isDoubleSolenoidIdle : 1 {};
	/**
	offset 144 bit 11 */
	bool useEeprom : 1 {};
	/**
	 * Switch between Industrial and Cic PID implementation
	offset 144 bit 12 */
	bool useCicPidForIdle : 1 {};
	/**
	offset 144 bit 13 */
	bool useTLE8888_cranking_hack : 1 {};
	/**
	offset 144 bit 14 */
	bool kickStartCranking : 1 {};
	/**
	 * This uses separate ignition timing and VE tables not only for idle conditions, also during the postcranking-to-idle taper transition (See also afterCrankingIACtaperDuration).
	offset 144 bit 15 */
	bool useSeparateIdleTablesForCrankingTaper : 1 {};
	/**
	offset 144 bit 16 */
	bool launchControlEnabled : 1 {};
	/**
	offset 144 bit 17 */
	bool tempPumpLimitCheck : 1 {};
	/**
	offset 144 bit 18 */
	bool antiLagEnabled : 1 {};
	/**
	 * For cranking either use the specified fixed base fuel mass, or use the normal running math (VE table).
	offset 144 bit 19 */
	bool useRunningMathForCranking : 1 {};
	/**
	 * Shall we display real life signal or just the part consumed by trigger decoder.
	 * Applies to both trigger and cam/vvt input.
	offset 144 bit 20 */
	bool displayLogicLevelsInEngineSniffer : 1 {};
	/**
	offset 144 bit 21 */
	bool useTLE8888_stepper : 1 {};
	/**
	offset 144 bit 22 */
	bool usescriptTableForCanSniffingFiltering : 1 {};
	/**
	 * Print incoming and outgoing first bus CAN messages in rusEFI console
	offset 144 bit 23 */
	bool verboseCan : 1 {};
	/**
	 * Experimental setting that will cause a misfire
	 * DO NOT ENABLE.
	offset 144 bit 24 */
	bool artificialTestMisfire : 1 {};
	/**
	 * On some Ford and Toyota vehicles one of the pedal sensors is not linear on the full range, i.e. in the specific range of the positions we effectively have only one sensor.
	offset 144 bit 25 */
	bool useFordRedundantPps : 1 {};
	/**
	offset 144 bit 26 */
	bool cltSensorPulldown : 1 {};
	/**
	offset 144 bit 27 */
	bool iatSensorPulldown : 1 {};
	/**
	offset 144 bit 28 */
	bool allowIdenticalPps : 1 {};
	/**
	offset 144 bit 29 */
	bool overrideVvtTriggerGaps : 1 {};
	/**
	 * If enabled - use onboard SPI Accelerometer, otherwise listen for CAN messages
	offset 144 bit 30 */
	bool useSpiImu : 1 {};
	/**
	offset 144 bit 31 */
	bool enableStagedInjection : 1 {};
	/**
	 * Closed throttle, 1 volt = 200 units.
	 * See also tps1_1AdcChannel
	 * set tps_min X
	 * units: ADC
	 * offset 148
	 */
	int16_t tpsMin;
	/**
	 * Full throttle.
	 * See also tps1_1AdcChannel
	 * set tps_max X
	 * units: ADC
	 * offset 150
	 */
	int16_t tpsMax;
	/**
	 * TPS error detection: what throttle % is unrealistically low?
	 * Also used for accelerator pedal error detection if so equipped.
	 * units: %
	 * offset 152
	 */
	int16_t tpsErrorDetectionTooLow;
	/**
	 * TPS error detection: what throttle % is unrealistically high?
	 * Also used for accelerator pedal error detection if so equipped.
	 * units: %
	 * offset 154
	 */
	int16_t tpsErrorDetectionTooHigh;
	/**
	 * offset 156
	 */
	cranking_parameters_s cranking;
	/**
	 * Dwell duration while cranking
	 * units: ms
	 * offset 160
	 */
	float ignitionDwellForCrankingMs;
	/**
	 * Once engine speed passes this value, start reducing ETB angle.
	 * units: rpm
	 * offset 164
	 */
	uint16_t etbRevLimitStart;
	/**
	 * This far above 'Soft limiter start', fully close the throttle. At the bottom of the range, throttle control is normal. At the top of the range, the throttle is fully closed.
	 * units: rpm
	 * offset 166
	 */
	uint16_t etbRevLimitRange;
	/**
	 * @see isMapAveragingEnabled
	 * offset 168
	 */
	MAP_sensor_config_s map;
	/**
	 * todo: merge with channel settings, use full-scale Thermistor here!
	 * offset 276
	 */
	ThermistorConf clt;
	/**
	 * offset 308
	 */
	ThermistorConf iat;
	/**
	 * units: deg
	 * offset 340
	 */
	float launchTimingRetard;
	/**
	 * Maximum commanded airmass for the idle controller.
	 * units: mg
	 * offset 344
	 */
	scaled_channel<uint8_t, 1, 2> idleMaximumAirmass;
	/**
	 * Zero value means do not detect tuning
	 * units: seconds
	 * offset 345
	 */
	uint8_t tuningDetector;
	/**
	 * iTerm min value
	 * offset 346
	 */
	int16_t alternator_iTermMin;
	/**
	 * iTerm max value
	 * offset 348
	 */
	int16_t alternator_iTermMax;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 350
	 */
	uint8_t alignmentFill_at_350[2] = {};
	/**
	 * @@DISPLACEMENT_TOOLTIP@@
	 * units: L
	 * offset 352
	 */
	float displacement;
	/**
	 * units: RPM
	 * offset 356
	 */
	uint16_t triggerSimulatorRpm;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 358
	 */
	uint8_t alignmentFill_at_358[2] = {};
	/**
	 * Number of cylinder the engine has.
	 * offset 360
	 */
	uint32_t cylindersCount;
	/**
	 * offset 364
	 */
	firing_order_e firingOrder;
	/**
	 * offset 365
	 */
	uint8_t justATempTest;
	/**
	 * Delta kPa for MAP sync
	 * units: kPa
	 * offset 366
	 */
	uint8_t mapSyncThreshold;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 367
	 */
	uint8_t alignmentFill_at_367[1] = {};
	/**
	 * @@CYLINDER_BORE_TOOLTIP@@
	 * units: mm
	 * offset 368
	 */
	float cylinderBore;
	/**
	 * This setting controls which fuel quantity control algorithm is used.
	 * Alpha-N means drive by TPS commonly only used for NA engines
	 * Speed Density requires MAP sensor and is the default choice for may installs
	 * MAF air charge is a cylinder filling based method that uses a mass air flow sensor.
	 * offset 372
	 */
	engine_load_mode_e fuelAlgorithm;
	/**
	 * units: %
	 * offset 373
	 */
	uint8_t ALSMaxTPS;
	/**
	 * This is the injection strategy during engine start. See Fuel/Injection settings for more detail. It is suggested to use "Simultaneous".
	 * offset 374
	 */
	injection_mode_e crankingInjectionMode;
	/**
	 * This is where the fuel injection type is defined: "Simultaneous" means all injectors will fire together at once. "Sequential" fires the injectors on a per cylinder basis, which requires individually wired injectors. "Batched" will fire the injectors in groups.
	 * offset 375
	 */
	injection_mode_e injectionMode;
	/**
	 * Minimum RPM to enable boost control. Use this to avoid solenoid noise at idle, and help spool in some cases.
	 * offset 376
	 */
	uint16_t boostControlMinRpm;
	/**
	 * Minimum TPS to enable boost control. Use this to avoid solenoid noise at idle, and help spool in some cases.
	 * offset 378
	 */
	uint8_t boostControlMinTps;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 379
	 */
	uint8_t alignmentFill_at_379[1] = {};
	/**
	 * Minimum MAP to enable boost control. Use this to avoid solenoid noise at idle, and help spool in some cases.
	 * offset 380
	 */
	uint16_t boostControlMinMap;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 382
	 */
	uint8_t alignmentFill_at_382[2] = {};
	/**
	 * Ignition advance angle used during engine cranking, 5-10 degrees will work as a base setting for most engines.
	 * There is tapering towards running timing advance
	 * set cranking_timing_angle X
	 * units: deg
	 * offset 384
	 */
	angle_t crankingTimingAngle;
	/**
	 * Single coil = distributor
	 * Individual coils = one coil per cylinder (COP, coil-near-plug), requires sequential mode
	 * Wasted spark = Fires pairs of cylinders together, either one coil per pair of cylinders or one coil per cylinder
	 * Two distributors = A pair of distributors, found on some BMW, Toyota and other engines
	 * set ignition_mode X
	 * offset 388
	 */
	ignition_mode_e ignitionMode;
	/**
	 * How many consecutive gap rations have to match expected ranges for sync to happen
	 * units: count
	 * offset 389
	 */
	int8_t gapTrackingLengthOverride;
	/**
	 * Above this speed, disable closed loop idle control. Set to 0 to disable (allow closed loop idle at any speed).
	 * units: kph
	 * offset 390
	 */
	uint8_t maxIdleVss;
	/**
	 * Allowed range around detection position
	 * offset 391
	 */
	uint8_t camDecoder2jzPrecision;
	/**
	 * Expected oil pressure after starting the engine. If oil pressure does not reach this level within 5 seconds of engine start, fuel will be cut. Set to 0 to disable and always allow starting.
	 * units: kPa
	 * offset 392
	 */
	uint16_t minOilPressureAfterStart;
	/**
	 * Dynamic uses the timing map to decide the ignition timing
	 * Static timing fixes the timing to the value set below (only use for checking static timing with a timing light).
	 * offset 394
	 */
	timing_mode_e timingMode;
	/**
	 * offset 395
	 */
	can_nbc_e canNbcType;
	/**
	 * This value is the ignition timing used when in 'fixed timing' mode, i.e. constant timing
	 * This mode is useful when adjusting distributor location.
	 * units: RPM
	 * offset 396
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
	 * offset 400
	 */
	angle_t globalTriggerAngleOffset;
	/**
	 * Ratio/coefficient of input voltage dividers on your PCB. For example, use '2' if your board divides 5v into 2.5v. Use '1.66' if your board divides 5v into 3v.
	 * units: coef
	 * offset 404
	 */
	float analogInputDividerCoefficient;
	/**
	 * This is the ratio of the resistors for the battery voltage, measure the voltage at the battery and then adjust this number until the gauge matches the reading.
	 * units: coef
	 * offset 408
	 */
	float vbattDividerCoeff;
	/**
	 * Cooling fan turn-on temperature threshold, in Celsius
	 * units: deg C
	 * offset 412
	 */
	float fanOnTemperature;
	/**
	 * Cooling fan turn-off temperature threshold, in Celsius
	 * units: deg C
	 * offset 416
	 */
	float fanOffTemperature;
	/**
	 * offset 420
	 */
	output_pin_e acrPin;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 422
	 */
	uint8_t alignmentFill_at_422[2] = {};
	/**
	 * Number of revolutions per kilometer for the wheels your vehicle speed sensor is connected to. Use an online calculator to determine this based on your tire size.
	 * units: revs/km
	 * offset 424
	 */
	float driveWheelRevPerKm;
	/**
	 * CANbus thread period in ms
	 * units: ms
	 * offset 428
	 */
	int canSleepPeriodMs;
	/**
	 * units: index
	 * offset 432
	 */
	int byFirmwareVersion;
	/**
	 * First throttle body, first sensor. See also pedalPositionAdcChannel
	 * offset 436
	 */
	adc_channel_e tps1_1AdcChannel;
	/**
	 * This is the processor input pin that the battery voltage circuit is connected to, if you are unsure of what pin to use, check the schematic that corresponds to your PCB.
	 * offset 437
	 */
	adc_channel_e vbattAdcChannel;
	/**
	 * This is the processor pin that your fuel level sensor in connected to. This is a non standard input so will need to be user defined.
	 * offset 438
	 */
	adc_channel_e fuelLevelSensor;
	/**
	 * Second throttle body position sensor, single channel so far
	 * offset 439
	 */
	adc_channel_e tps2_1AdcChannel;
	/**
	 * 0.1 is a good default value
	 * units: x
	 * offset 440
	 */
	float idle_derivativeFilterLoss;
	/**
	 * offset 444
	 */
	trigger_config_s trigger;
	/**
	 * Extra air taper amount
	 * units: %
	 * offset 456
	 */
	float airByRpmTaper;
	/**
	 * Duty cycle to use in case of a sensor failure. This duty cycle should produce the minimum possible amount of boost. This duty is also used in case any of the minimum RPM/TPS/MAP conditions are not met.
	 * units: %
	 * offset 460
	 */
	uint8_t boostControlSafeDutyCycle;
	/**
	 * offset 461
	 */
	adc_channel_e mafAdcChannel;
	/**
	 * offset 462
	 */
	uint8_t acrRevolutions;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 463
	 */
	uint8_t alignmentFill_at_463[1] = {};
	/**
	 * offset 464
	 */
	int calibrationBirthday;
	/**
	 * units: volts
	 * offset 468
	 */
	float adcVcc;
	/**
	 * Magic engine phase: we compare instant MAP at X to instant MAP at x+360 angle in one complete cycle
	 * units: Deg
	 * offset 472
	 */
	float mapCamDetectionAnglePosition;
	/**
	 * Camshaft input could be used either just for engine phase detection if your trigger shape does not include cam sensor as 'primary' channel, or it could be used for Variable Valve timing on one of the camshafts.
	 * offset 476
	 */
	brain_input_pin_e camInputs[CAM_INPUTS_COUNT] = {};
	/**
	 * offset 484
	 */
	afr_sensor_s afr;
	/**
	 * Electronic throttle pedal position first channel
	 * See throttlePedalPositionSecondAdcChannel for second channel
	 * See also tps1_1AdcChannel
	 * See throttlePedalUpVoltage and throttlePedalWOTVoltage
	 * offset 504
	 */
	adc_channel_e throttlePedalPositionAdcChannel;
	/**
	 * TPS/PPS error threshold
	 * units: %
	 * offset 505
	 */
	scaled_channel<uint8_t, 10, 1> etbSplit;
	/**
	 * offset 506
	 */
	Gpio tle6240_cs;
	/**
	 * offset 508
	 */
	pin_output_mode_e tle6240_csPinMode;
	/**
	 * offset 509
	 */
	pin_output_mode_e mc33810_csPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 510
	 */
	uint8_t alignmentFill_at_510[2] = {};
	/**
	 * @see hasBaroSensor
	 * offset 512
	 */
	air_pressure_sensor_config_s baroSensor;
	/**
	 * offset 524
	 */
	idle_hardware_s idle;
	/**
	 * Ignition timing to remove when a knock event occurs. Advice: 5% (mild), 10% (turbo/high comp.), 15% (high knock, e.g. GDI), 20% (spicy lump),
	 * units: %
	 * offset 536
	 */
	scaled_channel<uint8_t, 10, 1> knockRetardAggression;
	/**
	 * After a knock event, reapply timing at this rate.
	 * units: deg/s
	 * offset 537
	 */
	scaled_channel<uint8_t, 10, 1> knockRetardReapplyRate;
	/**
	 * Select which cam is used for engine sync. Other cams will be used only for VVT measurement, but not engine sync.
	 * offset 538
	 */
	engineSyncCam_e engineSyncCam;
	/**
	 * offset 539
	 */
	pin_output_mode_e sdCardCsPinMode;
	/**
	 * Number of turns of your vehicle speed sensor per turn of the wheels. For example if your sensor is on the transmission output, enter your axle/differential ratio. If you are using a hub-mounted sensor, enter a value of 1.0.
	 * units: ratio
	 * offset 540
	 */
	scaled_channel<uint16_t, 1000, 1> vssGearRatio;
	/**
	 * Set this so your vehicle speed signal is responsive, but not noisy. Larger value give smoother but slower response.
	 * offset 542
	 */
	uint8_t vssFilterReciprocal;
	/**
	 * Number of pulses output per revolution of the shaft where your VSS is mounted. For example, GM applications of the T56 output 17 pulses per revolution of the transmission output shaft.
	 * units: count
	 * offset 543
	 */
	uint8_t vssToothCount;
	/**
	 * Override the Y axis (load) value used for only the Idle VE table.
	 * Advanced users only: If you aren't sure you need this, you probably don't need this.
	 * offset 544
	 */
	ve_override_e idleVeOverrideMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 545
	 */
	uint8_t alignmentFill_at_545[1] = {};
	/**
	 * offset 546
	 */
	Gpio l9779_cs;
	/**
	 * offset 548
	 */
	output_pin_e injectionPins[MAX_CYLINDER_COUNT] = {};
	/**
	 * offset 572
	 */
	output_pin_e ignitionPins[MAX_CYLINDER_COUNT] = {};
	/**
	 * offset 596
	 */
	pin_output_mode_e injectionPinMode;
	/**
	 * offset 597
	 */
	pin_output_mode_e ignitionPinMode;
	/**
	 * offset 598
	 */
	output_pin_e fuelPumpPin;
	/**
	 * offset 600
	 */
	pin_output_mode_e fuelPumpPinMode;
	/**
	 * How many consecutive VVT gap rations have to match expected ranges for sync to happen
	 * units: count
	 * offset 601
	 */
	int8_t gapVvtTrackingLengthOverride;
	/**
	 * Check engine light, also malfunction indicator light. Always blinks once on boot.
	 * offset 602
	 */
	output_pin_e malfunctionIndicatorPin;
	/**
	 * offset 604
	 */
	pin_output_mode_e malfunctionIndicatorPinMode;
	/**
	 * offset 605
	 */
	pin_output_mode_e fanPinMode;
	/**
	 * offset 606
	 */
	output_pin_e fanPin;
	/**
	 * Some cars have a switch to indicate that clutch pedal is all the way down
	 * offset 608
	 */
	switch_input_pin_e clutchDownPin;
	/**
	 * offset 610
	 */
	output_pin_e alternatorControlPin;
	/**
	 * offset 612
	 */
	pin_output_mode_e alternatorControlPinMode;
	/**
	 * offset 613
	 */
	pin_input_mode_e clutchDownPinMode;
	/**
	 * offset 614
	 */
	Gpio digitalPotentiometerChipSelect[DIGIPOT_COUNT] = {};
	/**
	 * offset 622
	 */
	pin_output_mode_e electronicThrottlePin1Mode;
	/**
	 * offset 623
	 */
	spi_device_e max31855spiDevice;
	/**
	 * offset 624
	 */
	Gpio debugTriggerSync;
	/**
	 * Digital Potentiometer is used by stock ECU stimulation code
	 * offset 626
	 */
	spi_device_e digitalPotentiometerSpiDevice;
	/**
	 * offset 627
	 */
	pin_input_mode_e brakePedalPinMode;
	/**
	 * offset 628
	 */
	Gpio mc33972_cs;
	/**
	 * offset 630
	 */
	pin_output_mode_e mc33972_csPinMode;
	/**
	 * Useful in Research&Development phase
	 * offset 631
	 */
	adc_channel_e auxFastSensor1_adcChannel;
	/**
	 * First throttle body, second sensor.
	 * offset 632
	 */
	adc_channel_e tps1_2AdcChannel;
	/**
	 * Second throttle body, second sensor.
	 * offset 633
	 */
	adc_channel_e tps2_2AdcChannel;
	/**
	 * Electronic throttle pedal position input
	 * Second channel
	 * See also tps1_1AdcChannel
	 * See throttlePedalSecondaryUpVoltage and throttlePedalSecondaryWOTVoltage
	 * offset 634
	 */
	adc_channel_e throttlePedalPositionSecondAdcChannel;
	/**
	 * AFR, WBO, EGO - whatever you like to call it
	 * offset 635
	 */
	ego_sensor_e afr_type;
	/**
	 * offset 636
	 */
	Gpio mc33810_cs[C_MC33810_COUNT] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 638
	 */
	uint8_t alignmentFill_at_638[2] = {};
	/**
	 * 0.1 is a good default value
	 * units: x
	 * offset 640
	 */
	float idle_antiwindupFreq;
	/**
	 * offset 644
	 */
	brain_input_pin_e triggerInputPins[TRIGGER_INPUT_PIN_COUNT] = {};
	/**
	 * Minimum allowed time for the boost phase. If the boost target current is reached before this time elapses, it is assumed that the injector has failed short circuit.
	 * units: us
	 * offset 648
	 */
	uint16_t mc33_t_min_boost;
	/**
	 * Ratio between the wheels and your transmission output.
	 * units: ratio
	 * offset 650
	 */
	scaled_channel<uint16_t, 100, 1> finalGearRatio;
	/**
	 * offset 652
	 */
	brain_input_pin_e tcuInputSpeedSensorPin;
	/**
	 * offset 654
	 */
	uint8_t tcuInputSpeedSensorTeeth;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 655
	 */
	uint8_t alignmentFill_at_655[1] = {};
	/**
	 * Each rusEFI piece can provide synthetic trigger signal for external ECU. Sometimes these wires are routed back into trigger inputs of the same rusEFI board.
	 * See also directSelfStimulation which is different.
	 * offset 656
	 */
	Gpio triggerSimulatorPins[TRIGGER_SIMULATOR_PIN_COUNT] = {};
	/**
	 * units: g/s
	 * offset 660
	 */
	scaled_channel<uint16_t, 1000, 1> fordInjectorSmallPulseSlope;
	/**
	 * offset 662
	 */
	pin_output_mode_e triggerSimulatorPinModes[TRIGGER_SIMULATOR_PIN_COUNT] = {};
	/**
	 * offset 664
	 */
	adc_channel_e maf2AdcChannel;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 665
	 */
	uint8_t alignmentFill_at_665[1] = {};
	/**
	 * On-off O2 sensor heater control. 'ON' if engine is running, 'OFF' if stopped or cranking.
	 * offset 666
	 */
	output_pin_e o2heaterPin;
	/**
	 * offset 668
	 */
	pin_output_mode_e o2heaterPinModeTodO;
	/**
	 * units: RPM
	 * offset 669
	 */
	scaled_channel<uint8_t, 1, 100> lambdaProtectionMinRpm;
	/**
	 * units: %
	 * offset 670
	 */
	scaled_channel<uint8_t, 1, 10> lambdaProtectionMinLoad;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 671
	 */
	uint8_t alignmentFill_at_671[1] = {};
	/**
	offset 672 bit 0 */
	bool is_enabled_spi_1 : 1 {};
	/**
	offset 672 bit 1 */
	bool is_enabled_spi_2 : 1 {};
	/**
	offset 672 bit 2 */
	bool is_enabled_spi_3 : 1 {};
	/**
	 * enable sd/disable sd
	offset 672 bit 3 */
	bool isSdCardEnabled : 1 {};
	/**
	 * Use 11 bit (standard) or 29 bit (extended) IDs for rusEFI verbose CAN format.
	offset 672 bit 4 */
	bool rusefiVerbose29b : 1 {};
	/**
	offset 672 bit 5 */
	bool rethrowHardFault : 1 {};
	/**
	offset 672 bit 6 */
	bool requireFootOnBrakeToCrank : 1 {};
	/**
	offset 672 bit 7 */
	bool verboseQuad : 1 {};
	/**
	 * This setting should only be used if you have a stepper motor idle valve and a stepper motor driver installed.
	offset 672 bit 8 */
	bool useStepperIdle : 1 {};
	/**
	offset 672 bit 9 */
	bool enabledStep1Limiter : 1 {};
	/**
	offset 672 bit 10 */
	bool lambdaProtectionEnable : 1 {};
	/**
	offset 672 bit 11 */
	bool verboseTLE8888 : 1 {};
	/**
	 * CAN broadcast using custom rusEFI protocol
	offset 672 bit 12 */
	bool enableVerboseCanTx : 1 {};
	/**
	offset 672 bit 13 */
	bool externalRusEfiGdiModule : 1 {};
	/**
	offset 672 bit 14 */
	bool flipWboChannels : 1 {};
	/**
	 * Useful for individual intakes
	offset 672 bit 15 */
	bool measureMapOnlyInOneCylinder : 1 {};
	/**
	offset 672 bit 16 */
	bool stepperForceParkingEveryRestart : 1 {};
	/**
	 * If enabled, try to fire the engine before a full engine cycle has been completed using RPM estimated from the last 90 degrees of engine rotation. As soon as the trigger syncs plus 90 degrees rotation, fuel and ignition events will occur. If disabled, worst case may require up to 4 full crank rotations before any events are scheduled.
	offset 672 bit 17 */
	bool isFasterEngineSpinUpEnabled : 1 {};
	/**
	 * This setting disables fuel injection while the engine is in overrun, this is useful as a fuel saving measure and to prevent back firing.
	offset 672 bit 18 */
	bool coastingFuelCutEnabled : 1 {};
	/**
	 * Override the IAC position during overrun conditions to help reduce engine breaking, this can be helpful for large engines in light weight cars or engines that have trouble returning to idle.
	offset 672 bit 19 */
	bool useIacTableForCoasting : 1 {};
	/**
	offset 672 bit 20 */
	bool useNoiselessTriggerDecoder : 1 {};
	/**
	offset 672 bit 21 */
	bool useIdleTimingPidControl : 1 {};
	/**
	 * Allows disabling the ETB when the engine is stopped. You may not like the power draw or PWM noise from the motor, so this lets you turn it off until it's necessary.
	offset 672 bit 22 */
	bool disableEtbWhenEngineStopped : 1 {};
	/**
	offset 672 bit 23 */
	bool is_enabled_spi_4 : 1 {};
	/**
	 * Disable the electronic throttle motor and DC idle motor for testing.
	 * This mode is for testing ETB/DC idle position sensors, etc without actually driving the throttle.
	offset 672 bit 24 */
	bool pauseEtbControl : 1 {};
	/**
	offset 672 bit 25 */
	bool tpsTpsPercentMode : 1 {};
	/**
	offset 672 bit 26 */
	bool verboseKLine : 1 {};
	/**
	offset 672 bit 27 */
	bool idleIncrementalPidCic : 1 {};
	/**
	 * AEM X-Series or rusEFI Wideband
	offset 672 bit 28 */
	bool enableAemXSeries : 1 {};
	/**
	offset 672 bit 29 */
	bool modeledFlowIdle : 1 {};
	/**
	offset 672 bit 30 */
	bool unusedBit_227_30 : 1 {};
	/**
	offset 672 bit 31 */
	bool unusedBit_227_31 : 1 {};
	/**
	 * offset 676
	 */
	brain_input_pin_e logicAnalyzerPins[LOGIC_ANALYZER_CHANNEL_COUNT] = {};
	/**
	 * offset 684
	 */
	pin_output_mode_e mainRelayPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 685
	 */
	uint8_t alignmentFill_at_685[3] = {};
	/**
	 * offset 688
	 */
	uint32_t verboseCanBaseAddress;
	/**
	 * Boost Voltage
	 * units: v
	 * offset 692
	 */
	uint8_t mc33_hvolt;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 693
	 */
	uint8_t alignmentFill_at_693[1] = {};
	/**
	 * Minimum MAP before closed loop boost is enabled. Use to prevent misbehavior upon entering boost.
	 * units: kPa
	 * offset 694
	 */
	uint16_t minimumBoostClosedLoopMap;
	/**
	 * units: %
	 * offset 696
	 */
	int8_t initialIgnitionCutPercent;
	/**
	 * units: %
	 * offset 697
	 */
	int8_t finalIgnitionCutPercentBeforeLaunch;
	/**
	 * offset 698
	 */
	gppwm_channel_e boostOpenLoopYAxis;
	/**
	 * offset 699
	 */
	spi_device_e l9779spiDevice;
	/**
	 * offset 700
	 */
	imu_type_e imuType;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 701
	 */
	uint8_t alignmentFill_at_701[1] = {};
	/**
	 * How far above idle speed do we consider idling, i.e. coasting detection threshold.
	 * For example, if target = 800, this param = 200, then anything below 1000 RPM is considered idle.
	 * units: RPM
	 * offset 702
	 */
	int16_t idlePidRpmUpperLimit;
	/**
	 * Apply nonlinearity correction below a pulse of this duration. Pulses longer than this duration will receive no adjustment.
	 * units: ms
	 * offset 704
	 */
	scaled_channel<uint16_t, 1000, 1> applyNonlinearBelowPulse;
	/**
	 * offset 706
	 */
	Gpio lps25BaroSensorScl;
	/**
	 * offset 708
	 */
	Gpio lps25BaroSensorSda;
	/**
	 * offset 710
	 */
	brain_input_pin_e vehicleSpeedSensorInputPin;
	/**
	 * Some vehicles have a switch to indicate that clutch pedal is all the way up
	 * offset 712
	 */
	switch_input_pin_e clutchUpPin;
	/**
	 * offset 714
	 */
	InjectorNonlinearMode injectorNonlinearMode;
	/**
	 * offset 715
	 */
	pin_input_mode_e clutchUpPinMode;
	/**
	 * offset 716
	 */
	Gpio max31855_cs[EGT_CHANNEL_COUNT] = {};
	/**
	 * Continental/GM flex fuel sensor, 50-150hz type
	 * offset 732
	 */
	brain_input_pin_e flexSensorPin;
	/**
	 * Since torque reduction pin is usually shared with launch control, most people have an RPM where behavior under that is Launch Control, over that is Flat Shift/Torque Reduction
	 * units: rpm
	 * offset 734
	 */
	uint16_t torqueReductionArmingRpm;
	/**
	 * offset 736
	 */
	pin_output_mode_e stepperDirectionPinMode;
	/**
	 * offset 737
	 */
	spi_device_e mc33972spiDevice;
	/**
	 * Stoichiometric ratio for your secondary fuel. This value is used when the Flex Fuel sensor indicates E100, typically 9.0
	 * units: :1
	 * offset 738
	 */
	scaled_channel<uint8_t, 10, 1> stoichRatioSecondary;
	/**
	 * Maximum allowed ETB position. Some throttles go past fully open, so this allows you to limit it to fully open.
	 * units: %
	 * offset 739
	 */
	uint8_t etbMaximumPosition;
	/**
	 * Rate the ECU will log to the SD card, in hz (log lines per second).
	 * units: hz
	 * offset 740
	 */
	uint16_t sdCardLogFrequency;
	/**
	 * offset 742
	 */
	adc_channel_e idlePositionChannel;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 743
	 */
	uint8_t alignmentFill_at_743[1] = {};
	/**
	 * offset 744
	 */
	uint16_t launchCorrectionsEndRpm;
	/**
	 * offset 746
	 */
	output_pin_e starterRelayDisablePin;
	/**
	 * On some vehicles we can disable starter once engine is already running
	 * offset 748
	 */
	pin_output_mode_e starterRelayDisablePinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 749
	 */
	uint8_t alignmentFill_at_749[1] = {};
	/**
	 * Some Subaru and some Mazda use double-solenoid idle air valve
	 * offset 750
	 */
	output_pin_e secondSolenoidPin;
	/**
	 * See also starterControlPin
	 * offset 752
	 */
	switch_input_pin_e startStopButtonPin;
	/**
	 * units: RPM
	 * offset 754
	 */
	scaled_channel<uint8_t, 1, 100> lambdaProtectionRestoreRpm;
	/**
	 * offset 755
	 */
	pin_output_mode_e acRelayPinMode;
	/**
	 * This many MAP samples are used to estimate the current MAP. This many samples are considered, and the minimum taken. Recommended value is 1 for single-throttle engines, and your number of cylinders for individual throttle bodies.
	 * units: count
	 * offset 756
	 */
	int mapMinBufferLength;
	/**
	 * Below this throttle position, the engine is considered idling. If you have an electronic throttle, this checks accelerator pedal position instead of throttle position, and should be set to 1-2%.
	 * units: %
	 * offset 760
	 */
	int16_t idlePidDeactivationTpsThreshold;
	/**
	 * units: %
	 * offset 762
	 */
	int16_t stepperParkingExtraSteps;
	/**
	 * units: ADC
	 * offset 764
	 */
	uint16_t tps1SecondaryMin;
	/**
	 * units: ADC
	 * offset 766
	 */
	uint16_t tps1SecondaryMax;
	/**
	 * units: rpm
	 * offset 768
	 */
	int16_t antiLagRpmTreshold;
	/**
	 * Maximum time to crank starter when start/stop button is pressed
	 * units: Seconds
	 * offset 770
	 */
	uint16_t startCrankingDuration;
	/**
	 * This pin is used for debugging - snap a logic analyzer on it and see if it's ever high
	 * offset 772
	 */
	Gpio triggerErrorPin;
	/**
	 * offset 774
	 */
	pin_output_mode_e triggerErrorPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 775
	 */
	uint8_t alignmentFill_at_775[1] = {};
	/**
	 * offset 776
	 */
	output_pin_e acRelayPin;
	/**
	 * units: %
	 * offset 778
	 */
	uint8_t lambdaProtectionMinTps;
	/**
	 * Only respond once lambda is out of range for this period of time. Use to avoid transients triggering lambda protection when not needed
	 * units: s
	 * offset 779
	 */
	scaled_channel<uint8_t, 10, 1> lambdaProtectionTimeout;
	/**
	 * offset 780
	 */
	script_setting_t scriptSetting[SCRIPT_SETTING_COUNT] = {};
	/**
	 * offset 812
	 */
	Gpio spi1mosiPin;
	/**
	 * offset 814
	 */
	Gpio spi1misoPin;
	/**
	 * offset 816
	 */
	Gpio spi1sckPin;
	/**
	 * offset 818
	 */
	Gpio spi2mosiPin;
	/**
	 * offset 820
	 */
	Gpio spi2misoPin;
	/**
	 * offset 822
	 */
	Gpio spi2sckPin;
	/**
	 * offset 824
	 */
	Gpio spi3mosiPin;
	/**
	 * offset 826
	 */
	Gpio spi3misoPin;
	/**
	 * offset 828
	 */
	Gpio spi3sckPin;
	/**
	 * Saab Combustion Detection Module knock signal input pin
	 * also known as Saab Ion Sensing Module
	 * offset 830
	 */
	Gpio cdmInputPin;
	/**
	 * offset 832
	 */
	uart_device_e consoleUartDevice;
	/**
	 * offset 833
	 */
	maf_sensor_type_e mafSensorType;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 834
	 */
	uint8_t alignmentFill_at_834[2] = {};
	/**
	 * Ramp the idle target down from the entry threshold over N seconds when returning to idle. Helps prevent overshooting (below) the idle target while returning to idle from coasting.
	offset 836 bit 0 */
	bool idleReturnTargetRamp : 1 {};
	/**
	offset 836 bit 1 */
	bool useInjectorFlowLinearizationTable : 1 {};
	/**
	 * If enabled we use two H-bridges to drive stepper idle air valve
	offset 836 bit 2 */
	bool useHbridgesToDriveIdleStepper : 1 {};
	/**
	offset 836 bit 3 */
	bool multisparkEnable : 1 {};
	/**
	offset 836 bit 4 */
	bool enableLaunchRetard : 1 {};
	/**
	offset 836 bit 5 */
	bool canInputBCM : 1 {};
	/**
	 * This property is useful if using rusEFI as TCM or BCM only
	offset 836 bit 6 */
	bool consumeObdSensors : 1 {};
	/**
	 * Read VSS from OEM CAN bus according to selected CAN vehicle configuration.
	offset 836 bit 7 */
	bool enableCanVss : 1 {};
	/**
	offset 836 bit 8 */
	bool suppressWboWorkaround7048 : 1 {};
	/**
	 * If enabled, adjust at a constant rate instead of a rate proportional to the current lambda error. This mode may be easier to tune, and more tolerant of sensor noise.
	offset 836 bit 9 */
	bool stftIgnoreErrorMagnitude : 1 {};
	/**
	offset 836 bit 10 */
	bool vvtBooleanForVerySpecialCases : 1 {};
	/**
	offset 836 bit 11 */
	bool enableSoftwareKnock : 1 {};
	/**
	 * Verbose info in console below engineSnifferRpmThreshold
	offset 836 bit 12 */
	bool verboseVVTDecoding : 1 {};
	/**
	offset 836 bit 13 */
	bool invertCamVVTSignal : 1 {};
	/**
	 * In Alpha-N mode, compensate for air temperature.
	offset 836 bit 14 */
	bool alphaNUseIat : 1 {};
	/**
	offset 836 bit 15 */
	bool knockBankCyl1 : 1 {};
	/**
	offset 836 bit 16 */
	bool knockBankCyl2 : 1 {};
	/**
	offset 836 bit 17 */
	bool knockBankCyl3 : 1 {};
	/**
	offset 836 bit 18 */
	bool knockBankCyl4 : 1 {};
	/**
	offset 836 bit 19 */
	bool knockBankCyl5 : 1 {};
	/**
	offset 836 bit 20 */
	bool knockBankCyl6 : 1 {};
	/**
	offset 836 bit 21 */
	bool knockBankCyl7 : 1 {};
	/**
	offset 836 bit 22 */
	bool knockBankCyl8 : 1 {};
	/**
	offset 836 bit 23 */
	bool knockBankCyl9 : 1 {};
	/**
	offset 836 bit 24 */
	bool knockBankCyl10 : 1 {};
	/**
	offset 836 bit 25 */
	bool knockBankCyl11 : 1 {};
	/**
	offset 836 bit 26 */
	bool knockBankCyl12 : 1 {};
	/**
	offset 836 bit 27 */
	bool tcuEnabled : 1 {};
	/**
	offset 836 bit 28 */
	bool canBroadcastUseChannelTwo : 1 {};
	/**
	 * If enabled we use four Push-Pull outputs to directly drive stepper idle air valve coils
	offset 836 bit 29 */
	bool useRawOutputToDriveIdleStepper : 1 {};
	/**
	 * Print incoming and outgoing second bus CAN messages in rusEFI console
	offset 836 bit 30 */
	bool verboseCan2 : 1 {};
	/**
	offset 836 bit 31 */
	bool unusedBit_326_31 : 1 {};
	/**
	 * offset 840
	 */
	dc_io etbIo[ETB_COUNT] = {};
	/**
	 * Wastegate control Solenoid, set to 'NONE' if you are using DC wastegate
	 * offset 856
	 */
	output_pin_e boostControlPin;
	/**
	 * offset 858
	 */
	pin_output_mode_e boostControlPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 859
	 */
	uint8_t alignmentFill_at_859[1] = {};
	/**
	 * offset 860
	 */
	switch_input_pin_e ALSActivatePin;
	/**
	 * offset 862
	 */
	switch_input_pin_e launchActivatePin;
	/**
	 * offset 864
	 */
	pid_s boostPid;
	/**
	 * offset 884
	 */
	boostType_e boostType;
	/**
	 * offset 885
	 */
	pin_input_mode_e ignitionKeyDigitalPinMode;
	/**
	 * offset 886
	 */
	Gpio ignitionKeyDigitalPin;
	/**
	 * units: Hz
	 * offset 888
	 */
	int boostPwmFrequency;
	/**
	 * offset 892
	 */
	launchActivationMode_e launchActivationMode;
	/**
	 * offset 893
	 */
	antiLagActivationMode_e antiLagActivationMode;
	/**
	 * How long to look back for TPS-based acceleration enrichment. Increasing this time will trigger enrichment for longer when a throttle position change occurs.
	 * units: sec
	 * offset 894
	 */
	scaled_channel<uint8_t, 20, 1> tpsAccelLookback;
	/**
	 * Pause closed loop fueling after deceleration fuel cut occurs. Set this to a little longer than however long is required for normal fueling behavior to resume after fuel cut.
	 * units: sec
	 * offset 895
	 */
	scaled_channel<uint8_t, 10, 1> noFuelTrimAfterDfcoTime;
	/**
	 * Launch disabled above this speed if setting is above zero
	 * units: Kph
	 * offset 896
	 */
	int launchSpeedThreshold;
	/**
	 * Starting Launch RPM window to activate (subtracts from Launch RPM)
	 * units: RPM
	 * offset 900
	 */
	int launchRpmWindow;
	/**
	 * units: ms
	 * offset 904
	 */
	float triggerEventsTimeoutMs;
	/**
	 * offset 908
	 */
	float mapExpAverageAlpha;
	/**
	 * offset 912
	 */
	float magicNumberAvailableForDevTricks;
	/**
	 * offset 916
	 */
	float turbochargerFilter;
	/**
	 * offset 920
	 */
	int launchTpsThreshold;
	/**
	 * offset 924
	 */
	float launchActivateDelay;
	/**
	 * offset 928
	 */
	stft_s stft;
	/**
	 * offset 956
	 */
	dc_io stepperDcIo[DC_PER_STEPPER] = {};
	/**
	 * For example, BMW, GM or Chevrolet
	 * REQUIRED for rusEFI Online
	 * offset 972
	 */
	vehicle_info_t engineMake;
	/**
	 * For example, LS1 or NB2
	 * REQUIRED for rusEFI Online
	 * offset 974
	 */
	vehicle_info_t engineCode;
	/**
	 * For example, Hunchback or Orange Miata
	 * Vehicle name has to be unique between your vehicles.
	 * REQUIRED for rusEFI Online
	 * offset 976
	 */
	vehicle_info_t vehicleName;
	/**
	 * offset 978
	 */
	output_pin_e tcu_solenoid[TCU_SOLENOID_COUNT] = {};
	/**
	 * offset 982
	 */
	dc_function_e etbFunctions[ETB_COUNT] = {};
	/**
	 * offset 984
	 */
	spi_device_e drv8860spiDevice;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 985
	 */
	uint8_t alignmentFill_at_985[1] = {};
	/**
	 * offset 986
	 */
	Gpio drv8860_cs;
	/**
	 * offset 988
	 */
	pin_output_mode_e drv8860_csPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 989
	 */
	uint8_t alignmentFill_at_989[1] = {};
	/**
	 * offset 990
	 */
	Gpio drv8860_miso;
	/**
	 * offset 992
	 */
	output_pin_e luaOutputPins[LUA_PWM_COUNT] = {};
	/**
	 * Angle between cam sensor and VVT zero position
	 * units: value
	 * offset 1008
	 */
	float vvtOffsets[CAM_INPUTS_COUNT] = {};
	/**
	 * offset 1024
	 */
	vr_threshold_s vrThreshold[VR_THRESHOLD_COUNT] = {};
	/**
	 * offset 1056
	 */
	gppwm_note_t gpPwmNote[GPPWM_CHANNELS] = {};
	/**
	 * units: ADC
	 * offset 1120
	 */
	uint16_t tps2SecondaryMin;
	/**
	 * units: ADC
	 * offset 1122
	 */
	uint16_t tps2SecondaryMax;
	/**
	 * Select which bus the wideband controller is attached to.
	offset 1124 bit 0 */
	bool widebandOnSecondBus : 1 {};
	/**
	 * Enables lambda sensor closed loop feedback for fuelling.
	offset 1124 bit 1 */
	bool fuelClosedLoopCorrectionEnabled : 1 {};
	/**
	 * Write SD card log even when powered by USB
	offset 1124 bit 2 */
	bool alwaysWriteSdCard : 1 {};
	/**
	 * Second harmonic (aka double) is usually quieter background noise
	offset 1124 bit 3 */
	bool knockDetectionUseDoubleFrequency : 1 {};
	/**
	offset 1124 bit 4 */
	bool yesUnderstandLocking : 1 {};
	/**
	 * Sometimes we have a performance issue while printing error
	offset 1124 bit 5 */
	bool silentTriggerError : 1 {};
	/**
	offset 1124 bit 6 */
	bool useLinearCltSensor : 1 {};
	/**
	 * enable can_read/disable can_read
	offset 1124 bit 7 */
	bool canReadEnabled : 1 {};
	/**
	 * enable can_write/disable can_write. See also can1ListenMode
	offset 1124 bit 8 */
	bool canWriteEnabled : 1 {};
	/**
	offset 1124 bit 9 */
	bool useLinearIatSensor : 1 {};
	/**
	offset 1124 bit 10 */
	bool enableOilPressureProtect : 1 {};
	/**
	 * Treat milliseconds value as duty cycle value, i.e. 0.5ms would become 50%
	offset 1124 bit 11 */
	bool tachPulseDurationAsDutyCycle : 1 {};
	/**
	 * This enables smart alternator control and activates the extra alternator settings.
	offset 1124 bit 12 */
	bool isAlternatorControlEnabled : 1 {};
	/**
	 * https://wiki.rusefi.com/Trigger-Configuration-Guide
	 * This setting flips the signal from the primary engine speed sensor.
	offset 1124 bit 13 */
	bool invertPrimaryTriggerSignal : 1 {};
	/**
	 * https://wiki.rusefi.com/Trigger-Configuration-Guide
	 * This setting flips the signal from the secondary engine speed sensor.
	offset 1124 bit 14 */
	bool invertSecondaryTriggerSignal : 1 {};
	/**
	offset 1124 bit 15 */
	bool cutFuelOnHardLimit : 1 {};
	/**
	 * Be careful enabling this: some engines are known to self-disassemble their valvetrain with a spark cut. Fuel cut is much safer.
	offset 1124 bit 16 */
	bool cutSparkOnHardLimit : 1 {};
	/**
	offset 1124 bit 17 */
	bool launchFuelCutEnable : 1 {};
	/**
	 * This is the Cut Mode normally used
	offset 1124 bit 18 */
	bool launchSparkCutEnable : 1 {};
	/**
	offset 1124 bit 19 */
	bool torqueReductionEnabled : 1 {};
	/**
	offset 1124 bit 20 */
	bool camSyncOnSecondCrankRevolution : 1 {};
	/**
	offset 1124 bit 21 */
	bool limitTorqueReductionTime : 1 {};
	/**
	 * Are you a developer troubleshooting TS over CAN ISO/TP?
	offset 1124 bit 22 */
	bool verboseIsoTp : 1 {};
	/**
	 * In this mode only trigger events go into engine sniffer and not coils/injectors etc
	offset 1124 bit 23 */
	bool engineSnifferFocusOnInputs : 1 {};
	/**
	offset 1124 bit 24 */
	bool unused1308_24 : 1 {};
	/**
	offset 1124 bit 25 */
	bool twoStroke : 1 {};
	/**
	 * Where is your primary skipped wheel located?
	offset 1124 bit 26 */
	bool skippedWheelOnCam : 1 {};
	/**
	offset 1124 bit 27 */
	bool unusedBit_396_27 : 1 {};
	/**
	offset 1124 bit 28 */
	bool unusedBit_396_28 : 1 {};
	/**
	offset 1124 bit 29 */
	bool unusedBit_396_29 : 1 {};
	/**
	offset 1124 bit 30 */
	bool unusedBit_396_30 : 1 {};
	/**
	offset 1124 bit 31 */
	bool unusedBit_396_31 : 1 {};
	/**
	 * A/C button input
	 * offset 1128
	 */
	switch_input_pin_e acSwitch;
	/**
	 * offset 1130
	 */
	adc_channel_e vRefAdcChannel;
	/**
	 * Expected neutral position
	 * units: %
	 * offset 1131
	 */
	uint8_t etbNeutralPosition;
	/**
	 * See also idleRpmPid
	 * offset 1132
	 */
	idle_mode_e idleMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1133
	 */
	uint8_t alignmentFill_at_1133[3] = {};
	/**
	offset 1136 bit 0 */
	bool isInjectionEnabled : 1 {};
	/**
	offset 1136 bit 1 */
	bool isIgnitionEnabled : 1 {};
	/**
	 * When enabled if TPS is held above 95% no fuel is injected while cranking to clear excess fuel from the cylinders.
	offset 1136 bit 2 */
	bool isCylinderCleanupEnabled : 1 {};
	/**
	 * Should we use tables to vary tau/beta based on CLT/MAP, or just with fixed values?
	offset 1136 bit 3 */
	bool complexWallModel : 1 {};
	/**
	 * RPM is measured based on last 720 degrees while instant RPM is measured based on the last 90 degrees of crank revolution
	offset 1136 bit 4 */
	bool alwaysInstantRpm : 1 {};
	/**
	offset 1136 bit 5 */
	bool isMapAveragingEnabled : 1 {};
	/**
	 * If enabled, use separate temperature multiplier table for cranking idle position.
	 * If disabled, use normal running multiplier table applied to the cranking base position.
	offset 1136 bit 6 */
	bool unusedOverrideCrankingIacSetting : 1 {};
	/**
	 * This activates a separate ignition timing table for idle conditions, this can help idle stability by using ignition retard and advance either side of the desired idle speed. Extra advance at low idle speeds will prevent stalling and extra retard at high idle speeds can help reduce engine power and slow the idle speed.
	offset 1136 bit 7 */
	bool useSeparateAdvanceForIdle : 1 {};
	/**
	offset 1136 bit 8 */
	bool isWaveAnalyzerEnabled : 1 {};
	/**
	 * This activates a separate fuel table for Idle, this allows fine tuning of the idle fuelling.
	offset 1136 bit 9 */
	bool useSeparateVeForIdle : 1 {};
	/**
	 * Verbose info in console below engineSnifferRpmThreshold
	offset 1136 bit 10 */
	bool verboseTriggerSynchDetails : 1 {};
	/**
	offset 1136 bit 11 */
	bool unusedBitsd23413421 : 1 {};
	/**
	offset 1136 bit 12 */
	bool hondaK : 1 {};
	/**
	 * This is needed if your coils are individually wired (COP) and you wish to use batch ignition (Wasted Spark).
	offset 1136 bit 13 */
	bool twoWireBatchIgnition : 1 {};
	/**
	 * Read MAP sensor on ECU start-up to use as baro value.
	offset 1136 bit 14 */
	bool useFixedBaroCorrFromMap : 1 {};
	/**
	 * In Constant mode, timing is automatically tapered to running as RPM increases.
	 * In Table mode, the "Cranking ignition advance" table is used directly.
	offset 1136 bit 15 */
	bool useSeparateAdvanceForCranking : 1 {};
	/**
	 * This enables the various ignition corrections during cranking (IAT, CLT and PID idle).
	 * You probably don't need this.
	offset 1136 bit 16 */
	bool useAdvanceCorrectionsForCranking : 1 {};
	/**
	 * Enable a second cranking table to use for E100 flex fuel, interpolating between the two based on flex fuel sensor.
	offset 1136 bit 17 */
	bool flexCranking : 1 {};
	/**
	 * This flag allows to use a special 'PID Multiplier' table (0.0-1.0) to compensate for nonlinear nature of IAC-RPM controller
	offset 1136 bit 18 */
	bool useIacPidMultTable : 1 {};
	/**
	offset 1136 bit 19 */
	bool isBoostControlEnabled : 1 {};
	/**
	 * Interpolates the Ignition Retard from 0 to 100% within the RPM Range
	offset 1136 bit 20 */
	bool launchSmoothRetard : 1 {};
	/**
	 * Some engines are OK running semi-random sequential while other engine require phase synchronization
	offset 1136 bit 21 */
	bool isPhaseSyncRequiredForIgnition : 1 {};
	/**
	 * If enabled, use a curve for RPM limit (based on coolant temperature) instead of a constant value.
	offset 1136 bit 22 */
	bool useCltBasedRpmLimit : 1 {};
	/**
	 * If enabled, don't wait for engine start to heat O2 sensors.
	 * WARNING: this will reduce the life of your sensor, as condensation in the exhaust from a cold start can crack the sensing element.
	offset 1136 bit 23 */
	bool forceO2Heating : 1 {};
	/**
	 * If increased VVT duty cycle increases the indicated VVT angle, set this to 'advance'. If it decreases, set this to 'retard'. Most intake cams use 'advance', and most exhaust cams use 'retard'.
	offset 1136 bit 24 */
	bool invertVvtControlIntake : 1 {};
	/**
	 * If increased VVT duty cycle increases the indicated VVT angle, set this to 'advance'. If it decreases, set this to 'retard'. Most intake cams use 'advance', and most exhaust cams use 'retard'.
	offset 1136 bit 25 */
	bool invertVvtControlExhaust : 1 {};
	/**
	offset 1136 bit 26 */
	bool useBiQuadOnAuxSpeedSensors : 1 {};
	/**
	 * 'Trigger' mode will write a high speed log of trigger events (warning: uses lots of space!). 'Normal' mode will write a standard MLG of sensors, engine function, etc. similar to the one captured in TunerStudio.
	offset 1136 bit 27 */
	bool sdTriggerLog : 1 {};
	/**
	offset 1136 bit 28 */
	bool unused1320_28 : 1 {};
	/**
	offset 1136 bit 29 */
	bool stepper_dc_use_two_wires : 1 {};
	/**
	offset 1136 bit 30 */
	bool watchOutForLinearTime : 1 {};
	/**
	offset 1136 bit 31 */
	bool unusedBit_437_31 : 1 {};
	/**
	 * units: count
	 * offset 1140
	 */
	uint32_t engineChartSize;
	/**
	 * units: mult
	 * offset 1144
	 */
	float turboSpeedSensorMultiplier;
	/**
	 * offset 1148
	 */
	Gpio camInputsDebug[CAM_INPUTS_COUNT] = {};
	/**
	 * Idle target speed when A/C is enabled. Some cars need the extra speed to keep the AC efficient while idling.
	 * units: RPM
	 * offset 1156
	 */
	int16_t acIdleRpmTarget;
	/**
	 * set warningPeriod X
	 * units: seconds
	 * offset 1158
	 */
	int16_t warningPeriod;
	/**
	 * units: angle
	 * offset 1160
	 */
	float knockDetectionWindowStart;
	/**
	 * units: ms
	 * offset 1164
	 */
	float idleStepperReactionTime;
	/**
	 * units: count
	 * offset 1168
	 */
	int idleStepperTotalSteps;
	/**
	 * Pedal position to realize that we need to reduce torque when the trigger pin is uuuh triggered
	 * offset 1172
	 */
	int torqueReductionArmingApp;
	/**
	 * Duration in ms or duty cycle depending on selected mode
	 * offset 1176
	 */
	float tachPulseDuractionMs;
	/**
	 * Length of time the deposited wall fuel takes to dissipate after the start of acceleration.
	 * units: Seconds
	 * offset 1180
	 */
	float wwaeTau;
	/**
	 * offset 1184
	 */
	pid_s alternatorControl;
	/**
	 * offset 1204
	 */
	pid_s etb;
	/**
	 * offset 1224
	 */
	Gpio triggerInputDebugPins[TRIGGER_INPUT_PIN_COUNT] = {};
	/**
	 * RPM range above upper limit for extra air taper
	 * units: RPM
	 * offset 1228
	 */
	int16_t airTaperRpmRange;
	/**
	 * offset 1230
	 */
	brain_input_pin_e turboSpeedSensorInputPin;
	/**
	 * Closed throttle#2. todo: extract these two fields into a structure
	 * See also tps2_1AdcChannel
	 * units: ADC
	 * offset 1232
	 */
	int16_t tps2Min;
	/**
	 * Full throttle#2. tpsMax value as 10 bit ADC value. Not Voltage!
	 * See also tps1_1AdcChannel
	 * units: ADC
	 * offset 1234
	 */
	int16_t tps2Max;
	/**
	 * See also startStopButtonPin
	 * offset 1236
	 */
	output_pin_e starterControlPin;
	/**
	 * offset 1238
	 */
	pin_input_mode_e startStopButtonMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1239
	 */
	uint8_t alignmentFill_at_1239[1] = {};
	/**
	 * offset 1240
	 */
	Gpio mc33816_flag0;
	/**
	 * offset 1242
	 */
	scaled_channel<uint16_t, 1000, 1> tachPulsePerRev;
	/**
	 * kPa value which is too low to be true
	 * units: kPa
	 * offset 1244
	 */
	float mapErrorDetectionTooLow;
	/**
	 * kPa value which is too high to be true
	 * units: kPa
	 * offset 1248
	 */
	float mapErrorDetectionTooHigh;
	/**
	 * How long to wait for the spark to fire before recharging the coil for another spark.
	 * units: ms
	 * offset 1252
	 */
	scaled_channel<uint16_t, 1000, 1> multisparkSparkDuration;
	/**
	 * This sets the dwell time for subsequent sparks. The main spark's dwell is set by the dwell table.
	 * units: ms
	 * offset 1254
	 */
	scaled_channel<uint16_t, 1000, 1> multisparkDwell;
	/**
	 * See cltIdleRpmBins
	 * offset 1256
	 */
	pid_s idleRpmPid;
	/**
	 * 0 = No fuel settling on port walls 1 = All the fuel settling on port walls setting this to 0 disables the wall wetting enrichment.
	 * units: Fraction
	 * offset 1276
	 */
	float wwaeBeta;
	/**
	 * See also EFI_CONSOLE_RX_BRAIN_PIN
	 * offset 1280
	 */
	Gpio binarySerialTxPin;
	/**
	 * offset 1282
	 */
	Gpio binarySerialRxPin;
	/**
	 * offset 1284
	 */
	Gpio auxValves[AUX_DIGITAL_VALVE_COUNT] = {};
	/**
	 * offset 1288
	 */
	switch_input_pin_e tcuUpshiftButtonPin;
	/**
	 * offset 1290
	 */
	switch_input_pin_e tcuDownshiftButtonPin;
	/**
	 * units: voltage
	 * offset 1292
	 */
	float throttlePedalUpVoltage;
	/**
	 * Pedal in the floor
	 * units: voltage
	 * offset 1296
	 */
	float throttlePedalWOTVoltage;
	/**
	 * on IGN voltage detection turn fuel pump on to build fuel pressure
	 * units: seconds
	 * offset 1300
	 */
	int16_t startUpFuelPumpDuration;
	/**
	 * If the RPM closer to target than this value, disable closed loop idle correction to prevent oscillation
	 * units: RPM
	 * offset 1302
	 */
	int16_t idlePidRpmDeadZone;
	/**
	 * See Over/Undervoltage Shutdown/Retry bit in documentation
	offset 1304 bit 0 */
	bool mc33810DisableRecoveryMode : 1 {};
	/**
	offset 1304 bit 1 */
	bool mc33810Gpgd0Mode : 1 {};
	/**
	offset 1304 bit 2 */
	bool mc33810Gpgd1Mode : 1 {};
	/**
	offset 1304 bit 3 */
	bool mc33810Gpgd2Mode : 1 {};
	/**
	offset 1304 bit 4 */
	bool mc33810Gpgd3Mode : 1 {};
	/**
	 * Send out board statistics
	offset 1304 bit 5 */
	bool enableExtendedCanBroadcast : 1 {};
	/**
	 * global_can_data performance hack
	offset 1304 bit 6 */
	bool luaCanRxWorkaround : 1 {};
	/**
	offset 1304 bit 7 */
	bool flexSensorInverted : 1 {};
	/**
	offset 1304 bit 8 */
	bool useHardSkipInTraction : 1 {};
	/**
	 * Use Aux Speed 1 as one of speeds for wheel slip ratio?
	offset 1304 bit 9 */
	bool useAuxSpeedForSlipRatio : 1 {};
	/**
	 * VSS and Aux Speed 1 or Aux Speed 1 with Aux Speed 2?
	offset 1304 bit 10 */
	bool useVssAsSecondWheelSpeed : 1 {};
	/**
	offset 1304 bit 11 */
	bool is_enabled_spi_5 : 1 {};
	/**
	offset 1304 bit 12 */
	bool is_enabled_spi_6 : 1 {};
	/**
	 * AEM X-Series EGT gauge kit or rusEFI EGT sensor from Wideband controller
	offset 1304 bit 13 */
	bool enableAemXSeriesEgt : 1 {};
	/**
	offset 1304 bit 14 */
	bool startRequestPinInverted : 1 {};
	/**
	offset 1304 bit 15 */
	bool tcu_rangeSensorPulldown : 1 {};
	/**
	offset 1304 bit 16 */
	bool devBit01 : 1 {};
	/**
	offset 1304 bit 17 */
	bool devBit0 : 1 {};
	/**
	offset 1304 bit 18 */
	bool devBit1 : 1 {};
	/**
	offset 1304 bit 19 */
	bool devBit2 : 1 {};
	/**
	offset 1304 bit 20 */
	bool devBit3 : 1 {};
	/**
	offset 1304 bit 21 */
	bool devBit4 : 1 {};
	/**
	offset 1304 bit 22 */
	bool devBit5 : 1 {};
	/**
	offset 1304 bit 23 */
	bool devBit6 : 1 {};
	/**
	offset 1304 bit 24 */
	bool devBit7 : 1 {};
	/**
	offset 1304 bit 25 */
	bool invertExhaustCamVVTSignal : 1 {};
	/**
	 * "Available via TS Plugin see https://rusefi.com/s/knock"
	offset 1304 bit 26 */
	bool enableKnockSpectrogram : 1 {};
	/**
	offset 1304 bit 27 */
	bool enableKnockSpectrogramFilter : 1 {};
	/**
	offset 1304 bit 28 */
	bool unusedBit_504_28 : 1 {};
	/**
	offset 1304 bit 29 */
	bool unusedBit_504_29 : 1 {};
	/**
	offset 1304 bit 30 */
	bool unusedBit_504_30 : 1 {};
	/**
	offset 1304 bit 31 */
	bool unusedBit_504_31 : 1 {};
	/**
	 * offset 1308
	 */
	int16_t unusedafterCrankingIACtaperDuration;
	/**
	 * This value is an added for base idle value. Idle Value added when coasting and transitioning into idle.
	 * units: percent
	 * offset 1310
	 */
	int16_t iacByTpsTaper;
	/**
	 * offset 1312
	 */
	Gpio accelerometerCsPin;
	/**
	 * Below this speed, disable DFCO. Use this to prevent jerkiness from fuel enable/disable in low gears.
	 * units: kph
	 * offset 1314
	 */
	uint8_t coastingFuelCutVssLow;
	/**
	 * Above this speed, allow DFCO. Use this to prevent jerkiness from fuel enable/disable in low gears.
	 * units: kph
	 * offset 1315
	 */
	uint8_t coastingFuelCutVssHigh;
	/**
	 * Maximum change delta of TPS percentage over the 'length'. Actual TPS change has to be above this value in order for TPS/TPS acceleration to kick in.
	 * units: roc
	 * offset 1316
	 */
	float tpsAccelEnrichmentThreshold;
	/**
	 * offset 1320
	 */
	brain_input_pin_e auxSpeedSensorInputPin[AUX_SPEED_SENSOR_COUNT] = {};
	/**
	 * offset 1324
	 */
	uint8_t totalGearsCount;
	/**
	 * Sets what part of injection's is controlled by the injection phase table.
	 * offset 1325
	 */
	InjectionTimingMode injectionTimingMode;
	/**
	 * See http://rusefi.com/s/debugmode
	 * offset 1326
	 */
	debug_mode_e debugMode;
	/**
	 * Additional idle % when fan #1 is active
	 * units: %
	 * offset 1327
	 */
	uint8_t fan1ExtraIdle;
	/**
	 * Band rate for primary TTL
	 * units: BPs
	 * offset 1328
	 */
	uint32_t uartConsoleSerialSpeed;
	/**
	 * For decel we simply multiply delta of TPS and tFor decel we do not use table?!
	 * units: roc
	 * offset 1332
	 */
	float tpsDecelEnleanmentThreshold;
	/**
	 * Magic multiplier, we multiply delta of TPS and get fuel squirt duration
	 * units: coeff
	 * offset 1336
	 */
	float tpsDecelEnleanmentMultiplier;
	/**
	 * units: voltage
	 * offset 1340
	 */
	float throttlePedalSecondaryUpVoltage;
	/**
	 * Pedal in the floor
	 * units: voltage
	 * offset 1344
	 */
	float throttlePedalSecondaryWOTVoltage;
	/**
	 * offset 1348
	 */
	can_baudrate_e canBaudRate;
	/**
	 * Override the Y axis (load) value used for the VE table.
	 * Advanced users only: If you aren't sure you need this, you probably don't need this.
	 * offset 1349
	 */
	ve_override_e veOverrideMode;
	/**
	 * offset 1350
	 */
	can_baudrate_e can2BaudRate;
	/**
	 * Override the Y axis (load) value used for the AFR table.
	 * Advanced users only: If you aren't sure you need this, you probably don't need this.
	 * offset 1351
	 */
	load_override_e afrOverrideMode;
	/**
	 * units: A
	 * offset 1352
	 */
	scaled_channel<uint8_t, 10, 1> mc33_hpfp_i_peak;
	/**
	 * units: A
	 * offset 1353
	 */
	scaled_channel<uint8_t, 10, 1> mc33_hpfp_i_hold;
	/**
	 * How long to deactivate power when hold current is reached before applying power again
	 * units: us
	 * offset 1354
	 */
	uint8_t mc33_hpfp_i_hold_off;
	/**
	 * Maximum amount of time the solenoid can be active before assuming a programming error
	 * units: ms
	 * offset 1355
	 */
	uint8_t mc33_hpfp_max_hold;
	/**
	 * Enable if DC-motor driver (H-bridge) inverts the signals (eg. RZ7899 on Hellen boards)
	offset 1356 bit 0 */
	bool stepperDcInvertedPins : 1 {};
	/**
	 * Allow OpenBLT on Primary CAN
	offset 1356 bit 1 */
	bool canOpenBLT : 1 {};
	/**
	 * Allow OpenBLT on Secondary CAN
	offset 1356 bit 2 */
	bool can2OpenBLT : 1 {};
	/**
	 * Select whether to configure injector flow in volumetric flow (default, cc/min) or mass flow (g/s).
	offset 1356 bit 3 */
	bool injectorFlowAsMassFlow : 1 {};
	/**
	offset 1356 bit 4 */
	bool boardUseCanTerminator : 1 {};
	/**
	offset 1356 bit 5 */
	bool kLineDoHondaSend : 1 {};
	/**
	 * ListenMode is about acknowledging CAN traffic on the protocol level. Different from canWriteEnabled
	offset 1356 bit 6 */
	bool can1ListenMode : 1 {};
	/**
	offset 1356 bit 7 */
	bool can2ListenMode : 1 {};
	/**
	offset 1356 bit 8 */
	bool unusedBit_540_8 : 1 {};
	/**
	offset 1356 bit 9 */
	bool unusedBit_540_9 : 1 {};
	/**
	offset 1356 bit 10 */
	bool unusedBit_540_10 : 1 {};
	/**
	offset 1356 bit 11 */
	bool unusedBit_540_11 : 1 {};
	/**
	offset 1356 bit 12 */
	bool unusedBit_540_12 : 1 {};
	/**
	offset 1356 bit 13 */
	bool unusedBit_540_13 : 1 {};
	/**
	offset 1356 bit 14 */
	bool unusedBit_540_14 : 1 {};
	/**
	offset 1356 bit 15 */
	bool unusedBit_540_15 : 1 {};
	/**
	offset 1356 bit 16 */
	bool unusedBit_540_16 : 1 {};
	/**
	offset 1356 bit 17 */
	bool unusedBit_540_17 : 1 {};
	/**
	offset 1356 bit 18 */
	bool unusedBit_540_18 : 1 {};
	/**
	offset 1356 bit 19 */
	bool unusedBit_540_19 : 1 {};
	/**
	offset 1356 bit 20 */
	bool unusedBit_540_20 : 1 {};
	/**
	offset 1356 bit 21 */
	bool unusedBit_540_21 : 1 {};
	/**
	offset 1356 bit 22 */
	bool unusedBit_540_22 : 1 {};
	/**
	offset 1356 bit 23 */
	bool unusedBit_540_23 : 1 {};
	/**
	offset 1356 bit 24 */
	bool unusedBit_540_24 : 1 {};
	/**
	offset 1356 bit 25 */
	bool unusedBit_540_25 : 1 {};
	/**
	offset 1356 bit 26 */
	bool unusedBit_540_26 : 1 {};
	/**
	offset 1356 bit 27 */
	bool unusedBit_540_27 : 1 {};
	/**
	offset 1356 bit 28 */
	bool unusedBit_540_28 : 1 {};
	/**
	offset 1356 bit 29 */
	bool unusedBit_540_29 : 1 {};
	/**
	offset 1356 bit 30 */
	bool unusedBit_540_30 : 1 {};
	/**
	offset 1356 bit 31 */
	bool unusedBit_540_31 : 1 {};
	/**
	 * Angle of tooth detection within engine phase cycle
	 * units: angle
	 * offset 1360
	 */
	uint8_t camDecoder2jzPosition;
	/**
	 * offset 1361
	 */
	mc33810maxDwellTimer_e mc33810maxDwellTimer;
	/**
	 * Duration of each test pulse
	 * units: ms
	 * offset 1362
	 */
	scaled_channel<uint16_t, 100, 1> benchTestOnTime;
	/**
	 * units: %
	 * offset 1364
	 */
	uint8_t lambdaProtectionRestoreTps;
	/**
	 * units: %
	 * offset 1365
	 */
	scaled_channel<uint8_t, 1, 10> lambdaProtectionRestoreLoad;
	/**
	 * offset 1366
	 */
	pin_input_mode_e launchActivatePinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1367
	 */
	uint8_t alignmentFill_at_1367[1] = {};
	/**
	 * offset 1368
	 */
	Gpio can2TxPin;
	/**
	 * offset 1370
	 */
	Gpio can2RxPin;
	/**
	 * offset 1372
	 */
	pin_output_mode_e starterControlPinMode;
	/**
	 * offset 1373
	 */
	adc_channel_e wastegatePositionSensor;
	/**
	 * Override the Y axis (load) value used for the ignition table.
	 * Advanced users only: If you aren't sure you need this, you probably don't need this.
	 * offset 1374
	 */
	load_override_e ignOverrideMode;
	/**
	 * Select which fuel pressure sensor measures the pressure of the fuel at your injectors.
	 * offset 1375
	 */
	injector_pressure_type_e injectorPressureType;
	/**
	 * offset 1376
	 */
	output_pin_e hpfpValvePin;
	/**
	 * offset 1378
	 */
	pin_output_mode_e hpfpValvePinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1379
	 */
	uint8_t alignmentFill_at_1379[1] = {};
	/**
	 * MAP value above which fuel is cut in case of overboost.
	 * Set to 0 to disable overboost cut.
	 * units: kPa (absolute)
	 * offset 1380
	 */
	float boostCutPressure;
	/**
	 * units: kg/h
	 * offset 1384
	 */
	scaled_channel<uint8_t, 1, 5> tchargeBins[16] = {};
	/**
	 * units: ratio
	 * offset 1400
	 */
	scaled_channel<uint8_t, 100, 1> tchargeValues[16] = {};
	/**
	 * Fixed timing, useful for TDC testing
	 * units: deg
	 * offset 1416
	 */
	float fixedTiming;
	/**
	 * MAP voltage for low point
	 * units: v
	 * offset 1420
	 */
	float mapLowValueVoltage;
	/**
	 * MAP voltage for low point
	 * units: v
	 * offset 1424
	 */
	float mapHighValueVoltage;
	/**
	 * EGO value correction
	 * units: value
	 * offset 1428
	 */
	float egoValueShift;
	/**
	 * VVT output solenoid pin for this cam
	 * offset 1432
	 */
	output_pin_e vvtPins[CAM_INPUTS_COUNT] = {};
	/**
	 * offset 1440
	 */
	int unusedcrankingIACposition;
	/**
	 * offset 1444
	 */
	scaled_channel<uint8_t, 200, 1> tChargeMinRpmMinTps;
	/**
	 * offset 1445
	 */
	scaled_channel<uint8_t, 200, 1> tChargeMinRpmMaxTps;
	/**
	 * offset 1446
	 */
	scaled_channel<uint8_t, 200, 1> tChargeMaxRpmMinTps;
	/**
	 * offset 1447
	 */
	scaled_channel<uint8_t, 200, 1> tChargeMaxRpmMaxTps;
	/**
	 * offset 1448
	 */
	pwm_freq_t vvtOutputFrequency;
	/**
	 * Minimim timing advance allowed. No spark on any cylinder will ever fire after this angle BTDC. For example, setting -10 here means no spark ever fires later than 10 deg ATDC. Note that this only concerns the primary spark: any trailing sparks or multispark may violate this constraint.
	 * units: deg BTDC
	 * offset 1450
	 */
	int8_t minimumIgnitionTiming;
	/**
	 * Maximum timing advance allowed. No spark on any cylinder will ever fire before this angle BTDC. For example, setting 45 here means no spark ever fires earlier than 45 deg BTDC
	 * units: deg BTDC
	 * offset 1451
	 */
	int8_t maximumIgnitionTiming;
	/**
	 * units: Hz
	 * offset 1452
	 */
	int alternatorPwmFrequency;
	/**
	 * offset 1456
	 */
	vvt_mode_e vvtMode[CAMS_PER_BANK] = {};
	/**
	 * Additional idle % when fan #2 is active
	 * units: %
	 * offset 1458
	 */
	uint8_t fan2ExtraIdle;
	/**
	 * Delay to allow fuel pressure to build before firing the priming pulse.
	 * units: sec
	 * offset 1459
	 */
	scaled_channel<uint8_t, 100, 1> primingDelay;
	/**
	 * offset 1460
	 */
	adc_channel_e auxAnalogInputs[LUA_ANALOG_INPUT_COUNT] = {};
	/**
	 * offset 1468
	 */
	output_pin_e trailingCoilPins[MAX_CYLINDER_COUNT] = {};
	/**
	 * offset 1492
	 */
	tle8888_mode_e tle8888mode;
	/**
	 * offset 1493
	 */
	pin_output_mode_e accelerometerCsPinMode;
	/**
	 * None = I have a MAP-referenced fuel pressure regulator
	 * Fixed rail pressure = I have an atmosphere-referenced fuel pressure regulator (returnless, typically)
	 * Sensed rail pressure = I have a fuel pressure sensor
	 *  HPFP fuel mass compensation = manual mode for GDI engines
	 * offset 1494
	 */
	injector_compensation_mode_e injectorCompensationMode;
	/**
	 * offset 1495
	 */
	pin_output_mode_e fan2PinMode;
	/**
	 * This is the pressure at which your injector flow is known.
	 * For example if your injectors flow 400cc/min at 3.5 bar, enter 350kpa here.
	 * This is gauge pressure/in reference to atmospheric.
	 * units: kPa
	 * offset 1496
	 */
	float fuelReferencePressure;
	/**
	 * offset 1500
	 */
	ThermistorConf auxTempSensor1;
	/**
	 * offset 1532
	 */
	ThermistorConf auxTempSensor2;
	/**
	 * units: Deg
	 * offset 1564
	 */
	int16_t knockSamplingDuration;
	/**
	 * units: Hz
	 * offset 1566
	 */
	int16_t etbFreq;
	/**
	 * offset 1568
	 */
	pid_s etbWastegatePid;
	/**
	 * For micro-stepping, make sure that PWM frequency (etbFreq) is high enough
	 * offset 1588
	 */
	stepper_num_micro_steps_e stepperNumMicroSteps;
	/**
	 * Use to limit the current when the stepper motor is idle, not moving (100% = no limit)
	 * units: %
	 * offset 1589
	 */
	uint8_t stepperMinDutyCycle;
	/**
	 * Use to limit the max.current through the stepper motor (100% = no limit)
	 * units: %
	 * offset 1590
	 */
	uint8_t stepperMaxDutyCycle;
	/**
	 * offset 1591
	 */
	spi_device_e sdCardSpiDevice;
	/**
	 * per-cylinder ignition and fueling timing correction for uneven engines
	 * units: deg
	 * offset 1592
	 */
	angle_t timing_offset_cylinder[MAX_CYLINDER_COUNT] = {};
	/**
	 * units: seconds
	 * offset 1640
	 */
	float idlePidActivationTime;
	/**
	 * offset 1644
	 */
	pin_mode_e spi1SckMode;
	/**
	 * Modes count be used for 3v<>5v integration using pull-ups/pull-downs etc.
	 * offset 1645
	 */
	pin_mode_e spi1MosiMode;
	/**
	 * offset 1646
	 */
	pin_mode_e spi1MisoMode;
	/**
	 * offset 1647
	 */
	pin_mode_e spi2SckMode;
	/**
	 * offset 1648
	 */
	pin_mode_e spi2MosiMode;
	/**
	 * offset 1649
	 */
	pin_mode_e spi2MisoMode;
	/**
	 * offset 1650
	 */
	pin_mode_e spi3SckMode;
	/**
	 * offset 1651
	 */
	pin_mode_e spi3MosiMode;
	/**
	 * offset 1652
	 */
	pin_mode_e spi3MisoMode;
	/**
	 * offset 1653
	 */
	pin_output_mode_e stepperEnablePinMode;
	/**
	 * ResetB
	 * offset 1654
	 */
	Gpio mc33816_rstb;
	/**
	 * offset 1656
	 */
	Gpio mc33816_driven;
	/**
	 * Brake pedal switch
	 * offset 1658
	 */
	switch_input_pin_e brakePedalPin;
	/**
	 * VVT output PID
	 * TODO: rename to vvtPid
	 * offset 1660
	 */
	pid_s auxPid[CAMS_PER_BANK] = {};
	/**
	 * offset 1700
	 */
	float injectorCorrectionPolynomial[8] = {};
	/**
	 * units: C
	 * offset 1732
	 */
	scaled_channel<int8_t, 1, 5> primeBins[PRIME_CURVE_COUNT] = {};
	/**
	 * offset 1740
	 */
	linear_sensor_s oilPressure;
	/**
	 * offset 1760
	 */
	spi_device_e accelerometerSpiDevice;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1761
	 */
	uint8_t alignmentFill_at_1761[1] = {};
	/**
	 * offset 1762
	 */
	output_pin_e fan2Pin;
	/**
	 * Cooling fan turn-on temperature threshold, in Celsius
	 * units: deg C
	 * offset 1764
	 */
	uint8_t fan2OnTemperature;
	/**
	 * Cooling fan turn-off temperature threshold, in Celsius
	 * units: deg C
	 * offset 1765
	 */
	uint8_t fan2OffTemperature;
	/**
	 * offset 1766
	 */
	Gpio stepperEnablePin;
	/**
	 * offset 1768
	 */
	Gpio tle8888_cs;
	/**
	 * offset 1770
	 */
	pin_output_mode_e tle8888_csPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1771
	 */
	uint8_t alignmentFill_at_1771[1] = {};
	/**
	 * offset 1772
	 */
	Gpio mc33816_cs;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1774
	 */
	uint8_t alignmentFill_at_1774[2] = {};
	/**
	 * units: hz
	 * offset 1776
	 */
	float auxFrequencyFilter;
	/**
	 * offset 1780
	 */
	sent_input_pin_e sentInputPins[SENT_INPUT_COUNT] = {};
	/**
	 * This sets the RPM above which fuel cut is active.
	 * units: rpm
	 * offset 1782
	 */
	int16_t coastingFuelCutRpmHigh;
	/**
	 * This sets the RPM below which fuel cut is deactivated, this prevents jerking or issues transitioning to idle
	 * units: rpm
	 * offset 1784
	 */
	int16_t coastingFuelCutRpmLow;
	/**
	 * Throttle position below which fuel cut is active. With an electronic throttle enabled, this checks against pedal position.
	 * units: %
	 * offset 1786
	 */
	int16_t coastingFuelCutTps;
	/**
	 * Fuel cutoff is disabled when the engine is cold.
	 * units: C
	 * offset 1788
	 */
	int16_t coastingFuelCutClt;
	/**
	 * Increases PID reaction for RPM<target by adding extra percent to PID-error
	 * units: %
	 * offset 1790
	 */
	int16_t pidExtraForLowRpm;
	/**
	 * MAP value above which fuel injection is re-enabled.
	 * units: kPa
	 * offset 1792
	 */
	int16_t coastingFuelCutMap;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 1794
	 */
	uint8_t alignmentFill_at_1794[2] = {};
	/**
	 * offset 1796
	 */
	linear_sensor_s highPressureFuel;
	/**
	 * offset 1816
	 */
	linear_sensor_s lowPressureFuel;
	/**
	 * offset 1836
	 */
	gppwm_note_t scriptCurveName[SCRIPT_CURVE_COUNT] = {};
	/**
	 * offset 1932
	 */
	gppwm_note_t scriptTableName[SCRIPT_TABLE_COUNT] = {};
	/**
	 * offset 1996
	 */
	gppwm_note_t scriptSettingName[SCRIPT_SETTING_COUNT] = {};
	/**
	 * Heat transfer coefficient at zero flow.
	 * 0 means the air charge is fully heated to the same temperature as CLT.
	 * 1 means the air charge gains no heat, and enters the cylinder at the temperature measured by IAT.
	 * offset 2124
	 */
	float tChargeAirCoefMin;
	/**
	 * Heat transfer coefficient at high flow, as defined by "max air flow".
	 * 0 means the air charge is fully heated to the same temperature as CLT.
	 * 1 means the air charge gains no heat, and enters the cylinder at the temperature measured by IAT.
	 * offset 2128
	 */
	float tChargeAirCoefMax;
	/**
	 * High flow point for heat transfer estimation.
	 * Set this to perhaps 50-75% of your maximum airflow at wide open throttle.
	 * units: kg/h
	 * offset 2132
	 */
	float tChargeAirFlowMax;
	/**
	 * Maximum allowed rate of increase allowed for the estimated charge temperature
	 * units: deg/sec
	 * offset 2136
	 */
	float tChargeAirIncrLimit;
	/**
	 * Maximum allowed rate of decrease allowed for the estimated charge temperature
	 * units: deg/sec
	 * offset 2140
	 */
	float tChargeAirDecrLimit;
	/**
	 * iTerm min value
	 * offset 2144
	 */
	int16_t etb_iTermMin;
	/**
	 * iTerm max value
	 * offset 2146
	 */
	int16_t etb_iTermMax;
	/**
	 * See useIdleTimingPidControl
	 * offset 2148
	 */
	pid_s idleTimingPid;
	/**
	 * When entering idle, and the PID settings are aggressive, it's good to make a soft entry upon entering closed loop
	 * offset 2168
	 */
	float idleTimingSoftEntryTime;
	/**
	 * offset 2172
	 */
	pin_input_mode_e torqueReductionTriggerPinMode;
	/**
	 * offset 2173
	 */
	torqueReductionActivationMode_e torqueReductionActivationMode;
	/**
	 * A delay in cycles between fuel-enrich. portions
	 * units: cycles
	 * offset 2174
	 */
	int16_t tpsAccelFractionPeriod;
	/**
	 * A fraction divisor: 1 or less = entire portion at once, or split into diminishing fractions
	 * units: coef
	 * offset 2176
	 */
	float tpsAccelFractionDivisor;
	/**
	 * offset 2180
	 */
	spi_device_e tle8888spiDevice;
	/**
	 * offset 2181
	 */
	spi_device_e mc33816spiDevice;
	/**
	 * iTerm min value
	 * offset 2182
	 */
	int16_t idlerpmpid_iTermMin;
	/**
	 * offset 2184
	 */
	spi_device_e tle6240spiDevice;
	/**
	 * Stoichiometric ratio for your primary fuel. When Flex Fuel is enabled, this value is used when the Flex Fuel sensor indicates E0.
	 * E0 = 14.7
	 * E10 = 14.1
	 * E85 = 9.9
	 * E100 = 9.0
	 * units: :1
	 * offset 2185
	 */
	scaled_channel<uint8_t, 10, 1> stoichRatioPrimary;
	/**
	 * iTerm max value
	 * offset 2186
	 */
	int16_t idlerpmpid_iTermMax;
	/**
	 * This sets the range of the idle control on the ETB. At 100% idle position, the value specified here sets the base ETB position.
	 * units: %
	 * offset 2188
	 */
	float etbIdleThrottleRange;
	/**
	 * Select which fuel correction bank this cylinder belongs to. Group cylinders that share the same O2 sensor
	 * offset 2192
	 */
	uint8_t cylinderBankSelect[MAX_CYLINDER_COUNT] = {};
	/**
	 * units: mg
	 * offset 2204
	 */
	scaled_channel<uint8_t, 1, 5> primeValues[PRIME_CURVE_COUNT] = {};
	/**
	 * Trigger comparator center point voltage
	 * units: V
	 * offset 2212
	 */
	scaled_channel<uint8_t, 50, 1> triggerCompCenterVolt;
	/**
	 * Trigger comparator hysteresis voltage (Min)
	 * units: V
	 * offset 2213
	 */
	scaled_channel<uint8_t, 50, 1> triggerCompHystMin;
	/**
	 * Trigger comparator hysteresis voltage (Max)
	 * units: V
	 * offset 2214
	 */
	scaled_channel<uint8_t, 50, 1> triggerCompHystMax;
	/**
	 * VR-sensor saturation RPM
	 * units: RPM
	 * offset 2215
	 */
	scaled_channel<uint8_t, 1, 50> triggerCompSensorSatRpm;
	/**
	 * units: ratio
	 * offset 2216
	 */
	scaled_channel<uint16_t, 100, 1> tractionControlSlipBins[TRACTION_CONTROL_ETB_DROP_SIZE] = {};
	/**
	 * units: RPM
	 * offset 2228
	 */
	uint8_t tractionControlSpeedBins[TRACTION_CONTROL_ETB_DROP_SIZE] = {};
	/**
	 * offset 2234
	 */
	int8_t disableFan1AtSpeed;
	/**
	 * offset 2235
	 */
	int8_t disableFan2AtSpeed;
	/**
	 * offset 2236
	 */
	can_vss_nbc_e canVssNbcType;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2238
	 */
	uint8_t alignmentFill_at_2238[2] = {};
	/**
	 * offset 2240
	 */
	gppwm_channel gppwm[GPPWM_CHANNELS] = {};
	/**
	 * Boost Current
	 * units: mA
	 * offset 2672
	 */
	uint16_t mc33_i_boost;
	/**
	 * Peak Current
	 * units: mA
	 * offset 2674
	 */
	uint16_t mc33_i_peak;
	/**
	 * Hold Current
	 * units: mA
	 * offset 2676
	 */
	uint16_t mc33_i_hold;
	/**
	 * Maximum allowed boost phase time. If the injector current doesn't reach the threshold before this time elapses, it is assumed that the injector is missing or has failed open circuit.
	 * units: us
	 * offset 2678
	 */
	uint16_t mc33_t_max_boost;
	/**
	 * units: us
	 * offset 2680
	 */
	uint16_t mc33_t_peak_off;
	/**
	 * Peak phase duration
	 * units: us
	 * offset 2682
	 */
	uint16_t mc33_t_peak_tot;
	/**
	 * units: us
	 * offset 2684
	 */
	uint16_t mc33_t_bypass;
	/**
	 * units: us
	 * offset 2686
	 */
	uint16_t mc33_t_hold_off;
	/**
	 * Hold phase duration
	 * units: us
	 * offset 2688
	 */
	uint16_t mc33_t_hold_tot;
	/**
	 * offset 2690
	 */
	pin_input_mode_e tcuUpshiftButtonPinMode;
	/**
	 * offset 2691
	 */
	pin_input_mode_e tcuDownshiftButtonPinMode;
	/**
	 * offset 2692
	 */
	pin_input_mode_e acSwitchMode;
	/**
	 * offset 2693
	 */
	pin_output_mode_e tcu_solenoid_mode[TCU_SOLENOID_COUNT] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2695
	 */
	uint8_t alignmentFill_at_2695[1] = {};
	/**
	 * units: ratio
	 * offset 2696
	 */
	float triggerGapOverrideFrom[GAP_TRACKING_LENGTH] = {};
	/**
	 * units: ratio
	 * offset 2768
	 */
	float triggerGapOverrideTo[GAP_TRACKING_LENGTH] = {};
	/**
	 * Below this RPM, use camshaft information to synchronize the crank's position for full sequential operation. Use this if your cam sensor does weird things at high RPM. Set to 0 to disable, and always use cam to help sync crank.
	 * units: rpm
	 * offset 2840
	 */
	scaled_channel<uint8_t, 1, 50> maxCamPhaseResolveRpm;
	/**
	 * Delay before cutting fuel. Set to 0 to cut immediately with no delay. May cause rumbles and pops out of your exhaust...
	 * units: sec
	 * offset 2841
	 */
	scaled_channel<uint8_t, 10, 1> dfcoDelay;
	/**
	 * Delay before engaging the AC compressor. Set to 0 to engage immediately with no delay. Use this to prevent bogging at idle when AC engages.
	 * units: sec
	 * offset 2842
	 */
	scaled_channel<uint8_t, 10, 1> acDelay;
	/**
	 * offset 2843
	 */
	tChargeMode_e tChargeMode;
	/**
	 * units: mg
	 * offset 2844
	 */
	scaled_channel<uint16_t, 1000, 1> fordInjectorSmallPulseBreakPoint;
	/**
	 * Threshold in ETB error (target vs. actual) above which the jam timer is started. If the timer reaches the time specified in the jam detection timeout period, the throttle is considered jammed, and engine operation limited.
	 * units: %
	 * offset 2846
	 */
	uint8_t etbJamDetectThreshold;
	/**
	 * units: lobes/cam
	 * offset 2847
	 */
	uint8_t hpfpCamLobes;
	/**
	 * offset 2848
	 */
	hpfp_cam_e hpfpCam;
	/**
	 * Low engine speed for A/C. Larger engines can survive lower values
	 * units: RPM
	 * offset 2849
	 */
	scaled_channel<int8_t, 1, 10> acLowRpmLimit;
	/**
	 * If the requested activation time is below this angle, don't bother running the pump
	 * units: deg
	 * offset 2850
	 */
	uint8_t hpfpMinAngle;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2851
	 */
	uint8_t alignmentFill_at_2851[1] = {};
	/**
	 * Size of the pump chamber in cc. Typical Bosch HDP5 has a 9.0mm diameter, typical BMW N* stroke is 4.4mm.
	 * units: cc
	 * offset 2852
	 */
	scaled_channel<uint16_t, 1000, 1> hpfpPumpVolume;
	/**
	 * How long to keep the valve activated (in order to allow the pump to build pressure and keep the valve open on its own)
	 * https://rusefi.com/forum/viewtopic.php?t=2192
	 * units: deg
	 * offset 2854
	 */
	uint8_t hpfpActivationAngle;
	/**
	 * offset 2855
	 */
	uint8_t issFilterReciprocal;
	/**
	 * units: %/kPa
	 * offset 2856
	 */
	scaled_channel<uint16_t, 1000, 1> hpfpPidP;
	/**
	 * units: %/kPa/lobe
	 * offset 2858
	 */
	scaled_channel<uint16_t, 100000, 1> hpfpPidI;
	/**
	 * iTerm min value
	 * offset 2860
	 */
	int16_t hpfpPid_iTermMin;
	/**
	 * iTerm max value
	 * offset 2862
	 */
	int16_t hpfpPid_iTermMax;
	/**
	 * The fastest rate the target pressure can be reduced by. This is because HPFP have no way to bleed off pressure other than injecting fuel.
	 * units: kPa/s
	 * offset 2864
	 */
	uint16_t hpfpTargetDecay;
	/**
	 * offset 2866
	 */
	output_pin_e stepper_raw_output[4] = {};
	/**
	 * units: ratio
	 * offset 2874
	 */
	scaled_channel<uint16_t, 100, 1> gearRatio[TCU_GEAR_COUNT] = {};
	/**
	 * We need to give engine time to build oil pressure without diverting it to VVT
	 * units: ms
	 * offset 2878
	 */
	uint16_t vvtActivationDelayMs;
	/**
	 * offset 2880
	 */
	GearControllerMode gearControllerMode;
	/**
	 * offset 2881
	 */
	TransmissionControllerMode transmissionControllerMode;
	/**
	 * During revolution where ACR should be disabled at what specific angle to disengage
	 * units: deg
	 * offset 2882
	 */
	uint16_t acrDisablePhase;
	/**
	 * offset 2884
	 */
	linear_sensor_s auxLinear1;
	/**
	 * offset 2904
	 */
	linear_sensor_s auxLinear2;
	/**
	 * offset 2924
	 */
	output_pin_e tcu_tcc_onoff_solenoid;
	/**
	 * offset 2926
	 */
	pin_output_mode_e tcu_tcc_onoff_solenoid_mode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2927
	 */
	uint8_t alignmentFill_at_2927[1] = {};
	/**
	 * offset 2928
	 */
	output_pin_e tcu_tcc_pwm_solenoid;
	/**
	 * offset 2930
	 */
	pin_output_mode_e tcu_tcc_pwm_solenoid_mode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2931
	 */
	uint8_t alignmentFill_at_2931[1] = {};
	/**
	 * offset 2932
	 */
	pwm_freq_t tcu_tcc_pwm_solenoid_freq;
	/**
	 * offset 2934
	 */
	output_pin_e tcu_pc_solenoid_pin;
	/**
	 * offset 2936
	 */
	pin_output_mode_e tcu_pc_solenoid_pin_mode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2937
	 */
	uint8_t alignmentFill_at_2937[1] = {};
	/**
	 * offset 2938
	 */
	pwm_freq_t tcu_pc_solenoid_freq;
	/**
	 * offset 2940
	 */
	output_pin_e tcu_32_solenoid_pin;
	/**
	 * offset 2942
	 */
	pin_output_mode_e tcu_32_solenoid_pin_mode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2943
	 */
	uint8_t alignmentFill_at_2943[1] = {};
	/**
	 * offset 2944
	 */
	pwm_freq_t tcu_32_solenoid_freq;
	/**
	 * offset 2946
	 */
	output_pin_e acrPin2;
	/**
	 * Set a minimum allowed target position to avoid slamming/driving against the hard mechanical stop in the throttle.
	 * units: %
	 * offset 2948
	 */
	scaled_channel<uint8_t, 10, 1> etbMinimumPosition;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2949
	 */
	uint8_t alignmentFill_at_2949[1] = {};
	/**
	 * offset 2950
	 */
	uint16_t tuneHidingKey;
	/**
	 * Individual characters are accessible using vin(index) Lua function
	 * offset 2952
	 */
	vin_number_t vinNumber;
	/**
	 * units: C
	 * offset 2969
	 */
	int8_t torqueReductionActivationTemperature;
	/**
	 * offset 2970
	 */
	uint16_t highSpeedOffsets[HIGH_SPEED_COUNT] = {};
	/**
	 * offset 3034
	 */
	fuel_pressure_sensor_mode_e fuelPressureSensorMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3035
	 */
	uint8_t alignmentFill_at_3035[1] = {};
	/**
	 * offset 3036
	 */
	switch_input_pin_e luaDigitalInputPins[LUA_DIGITAL_INPUT_COUNT] = {};
	/**
	 * units: rpm
	 * offset 3052
	 */
	int16_t ALSMinRPM;
	/**
	 * units: rpm
	 * offset 3054
	 */
	int16_t ALSMaxRPM;
	/**
	 * units: sec
	 * offset 3056
	 */
	int16_t ALSMaxDuration;
	/**
	 * units: C
	 * offset 3058
	 */
	int8_t ALSMinCLT;
	/**
	 * units: C
	 * offset 3059
	 */
	int8_t ALSMaxCLT;
	/**
	 * offset 3060
	 */
	uint8_t alsMinTimeBetween;
	/**
	 * offset 3061
	 */
	uint8_t alsEtbPosition;
	/**
	 * units: %
	 * offset 3062
	 */
	uint8_t acRelayAlternatorDutyAdder;
	/**
	 * If you have SENT TPS sensor please select type. For analog TPS leave None
	 * offset 3063
	 */
	SentEtbType sentEtbType;
	/**
	 * offset 3064
	 */
	uint16_t customSentTpsMin;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3066
	 */
	uint8_t alignmentFill_at_3066[2] = {};
	/**
	 * units: %
	 * offset 3068
	 */
	int ALSIdleAdd;
	/**
	 * units: %
	 * offset 3072
	 */
	int ALSEtbAdd;
	/**
	 * offset 3076
	 */
	float ALSSkipRatio;
	/**
	 * Hysterisis: if Pressure High Disable is 240kpa, and acPressureEnableHyst is 20, when the ECU sees 240kpa, A/C will be disabled, and stay disabled until 240-20=220kpa is reached
	 * units: kPa (absolute)
	 * offset 3080
	 */
	scaled_channel<uint8_t, 2, 1> acPressureEnableHyst;
	/**
	 * offset 3081
	 */
	pin_input_mode_e ALSActivatePinMode;
	/**
	 * For Ford TPS, use 53%. For Toyota ETCS-i, use ~65%
	 * units: %
	 * offset 3082
	 */
	scaled_channel<uint8_t, 2, 1> tpsSecondaryMaximum;
	/**
	 * For Toyota ETCS-i, use ~69%
	 * units: %
	 * offset 3083
	 */
	scaled_channel<uint8_t, 2, 1> ppsSecondaryMaximum;
	/**
	 * offset 3084
	 */
	pin_input_mode_e luaDigitalInputPinModes[LUA_DIGITAL_INPUT_COUNT] = {};
	/**
	 * offset 3092
	 */
	uint16_t customSentTpsMax;
	/**
	 * offset 3094
	 */
	uint16_t kLineBaudRate;
	/**
	 * offset 3096
	 */
	CanGpioType canGpioType;
	/**
	 * offset 3097
	 */
	UiMode uiMode;
	/**
	 * Crank angle ATDC of first lobe peak
	 * units: deg
	 * offset 3098
	 */
	int16_t hpfpPeakPos;
	/**
	 * units: us
	 * offset 3100
	 */
	int16_t kLinePeriodUs;
	/**
	 * Window that the correction will be added throughout (example, if rpm limit is 7000, and rpmSoftLimitWindowSize is 200, the corrections activate at 6800RPM, creating a 200rpm window)
	 * units: RPM
	 * offset 3102
	 */
	scaled_channel<uint8_t, 1, 10> rpmSoftLimitWindowSize;
	/**
	 * Degrees of timing REMOVED from actual timing during soft RPM limit window
	 * units: deg
	 * offset 3103
	 */
	scaled_channel<uint8_t, 5, 1> rpmSoftLimitTimingRetard;
	/**
	 * % of fuel ADDED during window
	 * units: %
	 * offset 3104
	 */
	scaled_channel<uint8_t, 5, 1> rpmSoftLimitFuelAdded;
	/**
	 * Hysterisis: if the hard limit is 7200rpm and rpmHardLimitHyst is 200rpm, then when the ECU sees 7200rpm, fuel/ign will cut, and stay cut until 7000rpm (7200-200) is reached
	 * units: RPM
	 * offset 3105
	 */
	scaled_channel<uint8_t, 1, 10> rpmHardLimitHyst;
	/**
	 * Time between bench test pulses
	 * units: ms
	 * offset 3106
	 */
	scaled_channel<uint16_t, 10, 1> benchTestOffTime;
	/**
	 * Hysterisis: if hard cut is 240kpa, and boostCutPressureHyst is 20, when the ECU sees 240kpa, fuel/ign will cut, and stay cut until 240-20=220kpa is reached
	 * units: kPa (absolute)
	 * offset 3108
	 */
	scaled_channel<uint8_t, 2, 1> boostCutPressureHyst;
	/**
	 * Boost duty cycle modified by gear
	 * units: %
	 * offset 3109
	 */
	scaled_channel<int8_t, 2, 1> gearBasedOpenLoopBoostAdder[TCU_GEAR_COUNT] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3111
	 */
	uint8_t alignmentFill_at_3111[1] = {};
	/**
	 * How many test bench pulses do you want
	 * offset 3112
	 */
	uint32_t benchTestCount;
	/**
	 * How long initial idle adder is held before starting to decay.
	 * units: seconds
	 * offset 3116
	 */
	scaled_channel<uint8_t, 10, 1> iacByTpsHoldTime;
	/**
	 * How long it takes to remove initial IAC adder to return to normal idle.
	 * units: seconds
	 * offset 3117
	 */
	scaled_channel<uint8_t, 10, 1> iacByTpsDecayTime;
	/**
	 * offset 3118
	 */
	switch_input_pin_e tcu_rangeInput[RANGE_INPUT_COUNT] = {};
	/**
	 * offset 3130
	 */
	pin_input_mode_e tcu_rangeInputMode[RANGE_INPUT_COUNT] = {};
	/**
	 * Scale the reported vehicle speed value from CAN. Example: Parameter set to 1.1, CAN VSS reports 50kph, ECU will report 55kph instead.
	 * units: ratio
	 * offset 3136
	 */
	scaled_channel<uint16_t, 10000, 1> canVssScaling;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3138
	 */
	uint8_t alignmentFill_at_3138[2] = {};
	/**
	 * offset 3140
	 */
	ThermistorConf oilTempSensor;
	/**
	 * offset 3172
	 */
	ThermistorConf fuelTempSensor;
	/**
	 * offset 3204
	 */
	ThermistorConf ambientTempSensor;
	/**
	 * offset 3236
	 */
	ThermistorConf compressorDischargeTemperature;
	/**
	 * Place the sensor before the throttle, but after any turbocharger/supercharger and intercoolers if fitted. Uses the same calibration as the MAP sensor.
	 * offset 3268
	 */
	adc_channel_e throttleInletPressureChannel;
	/**
	 * Place the sensor after the turbocharger/supercharger, but before any intercoolers if fitted. Uses the same calibration as the MAP sensor.
	 * offset 3269
	 */
	adc_channel_e compressorDischargePressureChannel;
	/**
	 * offset 3270
	 */
	Gpio dacOutputPins[DAC_OUTPUT_COUNT] = {};
	/**
	 * offset 3274
	 */
	output_pin_e speedometerOutputPin;
	/**
	 * Number of speedometer pulses per kilometer travelled.
	 * offset 3276
	 */
	uint16_t speedometerPulsePerKm;
	/**
	 * offset 3278
	 */
	uint8_t simulatorCamPosition[CAM_INPUTS_COUNT] = {};
	/**
	 * offset 3282
	 */
	adc_channel_e ignKeyAdcChannel;
	/**
	 * offset 3283
	 */
	pin_mode_e spi6MisoMode;
	/**
	 * units: ratio
	 * offset 3284
	 */
	float triggerVVTGapOverrideFrom[VVT_TRACKING_LENGTH] = {};
	/**
	 * units: ratio
	 * offset 3300
	 */
	float triggerVVTGapOverrideTo[VVT_TRACKING_LENGTH] = {};
	/**
	 * units: %
	 * offset 3316
	 */
	int8_t tractionControlEtbDrop[TRACTION_CONTROL_ETB_DROP_SIZE][TRACTION_CONTROL_ETB_DROP_SIZE] = {};
	/**
	 * If injector duty cycle hits this value, instantly cut fuel.
	 * units: %
	 * offset 3352
	 */
	uint8_t maxInjectorDutyInstant;
	/**
	 * If injector duty cycle hits this value for the specified delay time, cut fuel.
	 * units: %
	 * offset 3353
	 */
	uint8_t maxInjectorDutySustained;
	/**
	 * Timeout period for duty cycle over the sustained limit to trigger duty cycle protection.
	 * units: sec
	 * offset 3354
	 */
	scaled_channel<uint8_t, 10, 1> maxInjectorDutySustainedTimeout;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3355
	 */
	uint8_t alignmentFill_at_3355[1] = {};
	/**
	 * offset 3356
	 */
	output_pin_e injectionPinsStage2[MAX_CYLINDER_COUNT] = {};
	/**
	 * units: Deg
	 * offset 3380
	 */
	int8_t tractionControlTimingDrop[TRACTION_CONTROL_ETB_DROP_SIZE][TRACTION_CONTROL_ETB_DROP_SIZE] = {};
	/**
	 * units: %
	 * offset 3416
	 */
	int8_t tractionControlIgnitionSkip[TRACTION_CONTROL_ETB_DROP_SIZE][TRACTION_CONTROL_ETB_DROP_SIZE] = {};
	/**
	 * offset 3452
	 */
	float auxSpeed1Multiplier;
	/**
	 * offset 3456
	 */
	float brakeMeanEffectivePressureDifferential;
	/**
	 * offset 3460
	 */
	Gpio spi4mosiPin;
	/**
	 * offset 3462
	 */
	Gpio spi4misoPin;
	/**
	 * offset 3464
	 */
	Gpio spi4sckPin;
	/**
	 * offset 3466
	 */
	Gpio spi5mosiPin;
	/**
	 * offset 3468
	 */
	Gpio spi5misoPin;
	/**
	 * offset 3470
	 */
	Gpio spi5sckPin;
	/**
	 * offset 3472
	 */
	Gpio spi6mosiPin;
	/**
	 * offset 3474
	 */
	Gpio spi6misoPin;
	/**
	 * offset 3476
	 */
	Gpio spi6sckPin;
	/**
	 * offset 3478
	 */
	pin_mode_e spi4SckMode;
	/**
	 * offset 3479
	 */
	pin_mode_e spi4MosiMode;
	/**
	 * offset 3480
	 */
	pin_mode_e spi4MisoMode;
	/**
	 * offset 3481
	 */
	pin_mode_e spi5SckMode;
	/**
	 * offset 3482
	 */
	pin_mode_e spi5MosiMode;
	/**
	 * offset 3483
	 */
	pin_mode_e spi5MisoMode;
	/**
	 * offset 3484
	 */
	pin_mode_e spi6SckMode;
	/**
	 * offset 3485
	 */
	pin_mode_e spi6MosiMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3486
	 */
	uint8_t alignmentFill_at_3486[2] = {};
	/**
	 * Secondary TTL channel baud rate
	 * units: BPs
	 * offset 3488
	 */
	uint32_t tunerStudioSerialSpeed;
	/**
	 * offset 3492
	 */
	Gpio camSimulatorPin;
	/**
	 * offset 3494
	 */
	pin_output_mode_e camSimulatorPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3495
	 */
	uint8_t alignmentFill_at_3495[1] = {};
	/**
	 * offset 3496
	 */
	int anotherCiTest;
	/**
	 * offset 3500
	 */
	uint32_t device_uid[3] = {};
	/**
	 * offset 3512
	 */
	adc_channel_e tcu_rangeAnalogInput[RANGE_INPUT_COUNT] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3518
	 */
	uint8_t alignmentFill_at_3518[2] = {};
	/**
	 * units: Ohm
	 * offset 3520
	 */
	float tcu_rangeSensorBiasResistor;
	/**
	 * offset 3524
	 */
	MsIoBox_config_s msIoBox0;
	/**
	 * Nominal coil charge current, 0.25A step
	 * units: A
	 * offset 3528
	 */
	scaled_channel<uint8_t, 4, 1> mc33810Nomi;
	/**
	 * Maximum coil charge current, 1A step
	 * units: A
	 * offset 3529
	 */
	uint8_t mc33810Maxi;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3530
	 */
	uint8_t alignmentFill_at_3530[2] = {};
	/**
	 * offset 3532
	 */
	linear_sensor_s acPressure;
	/**
	 * value of A/C pressure in kPa before that compressor is disengaged
	 * units: kPa
	 * offset 3552
	 */
	uint16_t minAcPressure;
	/**
	 * value of A/C pressure in kPa after that compressor is disengaged
	 * units: kPa
	 * offset 3554
	 */
	uint16_t maxAcPressure;
	/**
	 * Delay before cutting fuel due to low oil pressure. Use this to ignore short pressure blips and sensor noise.
	 * units: sec
	 * offset 3556
	 */
	scaled_channel<uint8_t, 10, 1> minimumOilPressureTimeout;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3557
	 */
	uint8_t alignmentFill_at_3557[3] = {};
	/**
	 * offset 3560
	 */
	linear_sensor_s auxLinear3;
	/**
	 * offset 3580
	 */
	linear_sensor_s auxLinear4;
	/**
	 * Below TPS value all knock suppression will be disabled.
	 * units: %
	 * offset 3600
	 */
	scaled_channel<uint8_t, 1, 1> knockSuppressMinTps;
	/**
	 * Fuel to odd when a knock event occurs. Advice: 5% (mild), 10% (turbo/high comp.), 15% (high knock, e.g. GDI), 20% (spicy lump),
	 * units: %
	 * offset 3601
	 */
	scaled_channel<uint8_t, 10, 1> knockFuelTrimAggression;
	/**
	 * After a knock event, reapply fuel at this rate.
	 * units: 1%/s
	 * offset 3602
	 */
	scaled_channel<uint8_t, 10, 1> knockFuelTrimReapplyRate;
	/**
	 * Fuel trim when knock, max 30%
	 * units: %
	 * offset 3603
	 */
	scaled_channel<uint8_t, 1, 1> knockFuelTrim;
	/**
	 * units: sense
	 * offset 3604
	 */
	float knockSpectrumSensitivity;
	/**
	 * "Estimated knock frequency, ignore cylinderBore if this one > 0"
	 * units: Hz
	 * offset 3608
	 */
	float knockFrequency;
	/**
	 * None = I have a MAP-referenced fuel pressure regulator
	 * Fixed rail pressure = I have an atmosphere-referenced fuel pressure regulator (returnless, typically)
	 * Sensed rail pressure = I have a fuel pressure sensor
	 * offset 3612
	 */
	injector_compensation_mode_e secondaryInjectorCompensationMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3613
	 */
	uint8_t alignmentFill_at_3613[3] = {};
	/**
	 * This is the pressure at which your injector flow is known.
	 * For example if your injectors flow 400cc/min at 3.5 bar, enter 350kpa here.
	 * units: kPa
	 * offset 3616
	 */
	float secondaryInjectorFuelReferencePressure;
	/**
	 * SENT input connected to ETB
	 * offset 3620
	 */
	SentInput EtbSentInput;
	/**
	 * SENT input used for high pressure fuel sensor
	 * offset 3621
	 */
	SentInput FuelHighPressureSentInput;
	/**
	 * If you have SENT High Pressure Fuel Sensor please select type. For analog TPS leave None
	 * offset 3622
	 */
	SentFuelHighPressureType FuelHighPressureSentType;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3623
	 */
	uint8_t alignmentFill_at_3623[1] = {};
	/**
	offset 3624 bit 0 */
	bool nitrousControlEnabled : 1 {};
	/**
	offset 3624 bit 1 */
	bool unusedFancy2 : 1 {};
	/**
	offset 3624 bit 2 */
	bool unusedFancy3 : 1 {};
	/**
	offset 3624 bit 3 */
	bool unusedFancy4 : 1 {};
	/**
	offset 3624 bit 4 */
	bool unusedFancy5 : 1 {};
	/**
	offset 3624 bit 5 */
	bool unusedFancy6 : 1 {};
	/**
	offset 3624 bit 6 */
	bool unusedFancy7 : 1 {};
	/**
	offset 3624 bit 7 */
	bool unusedFancy8 : 1 {};
	/**
	offset 3624 bit 8 */
	bool unusedFancy9 : 1 {};
	/**
	offset 3624 bit 9 */
	bool unusedFancy10 : 1 {};
	/**
	offset 3624 bit 10 */
	bool unusedFancy11 : 1 {};
	/**
	offset 3624 bit 11 */
	bool unusedFancy12 : 1 {};
	/**
	offset 3624 bit 12 */
	bool unusedFancy13 : 1 {};
	/**
	offset 3624 bit 13 */
	bool unusedFancy14 : 1 {};
	/**
	offset 3624 bit 14 */
	bool unusedFancy15 : 1 {};
	/**
	offset 3624 bit 15 */
	bool unusedFancy16 : 1 {};
	/**
	offset 3624 bit 16 */
	bool unusedFancy17 : 1 {};
	/**
	offset 3624 bit 17 */
	bool unusedFancy18 : 1 {};
	/**
	offset 3624 bit 18 */
	bool unusedFancy19 : 1 {};
	/**
	offset 3624 bit 19 */
	bool unusedFancy20 : 1 {};
	/**
	offset 3624 bit 20 */
	bool unusedFancy21 : 1 {};
	/**
	offset 3624 bit 21 */
	bool unusedFancy22 : 1 {};
	/**
	offset 3624 bit 22 */
	bool unusedFancy23 : 1 {};
	/**
	offset 3624 bit 23 */
	bool unusedFancy24 : 1 {};
	/**
	offset 3624 bit 24 */
	bool unusedFancy25 : 1 {};
	/**
	offset 3624 bit 25 */
	bool unusedFancy26 : 1 {};
	/**
	offset 3624 bit 26 */
	bool unusedFancy27 : 1 {};
	/**
	offset 3624 bit 27 */
	bool unusedFancy28 : 1 {};
	/**
	offset 3624 bit 28 */
	bool unusedFancy29 : 1 {};
	/**
	offset 3624 bit 29 */
	bool unusedFancy30 : 1 {};
	/**
	offset 3624 bit 30 */
	bool unusedFancy31 : 1 {};
	/**
	offset 3624 bit 31 */
	bool unusedFancy32 : 1 {};
	/**
	 * offset 3628
	 */
	nitrous_arming_method_e nitrousControlArmingMethod;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3629
	 */
	uint8_t alignmentFill_at_3629[1] = {};
	/**
	 * Pin that activates nitrous control
	 * offset 3630
	 */
	switch_input_pin_e nitrousControlTriggerPin;
	/**
	 * offset 3632
	 */
	pin_input_mode_e nitrousControlTriggerPinMode;
	/**
	 * offset 3633
	 */
	lua_gauge_e nitrousLuaGauge;
	/**
	 * offset 3634
	 */
	lua_gauge_meaning_e nitrousLuaGaugeMeaning;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3635
	 */
	uint8_t alignmentFill_at_3635[1] = {};
	/**
	 * offset 3636
	 */
	float nitrousLuaGaugeArmingValue;
	/**
	 * offset 3640
	 */
	int nitrousMinimumTps;
	/**
	 * units: deg C
	 * offset 3644
	 */
	uint8_t nitrousMinimumClt;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3645
	 */
	uint8_t alignmentFill_at_3645[1] = {};
	/**
	 * units: kPa
	 * offset 3646
	 */
	int16_t nitrousMaximumMap;
	/**
	 * units: afr
	 * offset 3648
	 */
	scaled_channel<uint8_t, 10, 1> nitrousMaximumAfr;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3649
	 */
	uint8_t alignmentFill_at_3649[1] = {};
	/**
	 * units: rpm
	 * offset 3650
	 */
	uint16_t nitrousActivationRpm;
	/**
	 * units: rpm
	 * offset 3652
	 */
	uint16_t nitrousDeactivationRpm;
	/**
	 * units: rpm
	 * offset 3654
	 */
	uint16_t nitrousDeactivationRpmWindow;
	/**
	 * Retard timing by this amount during DFCO. Smooths the transition back from fuel cut. After fuel is restored, ramp timing back in over the period specified.
	 * units: deg
	 * offset 3656
	 */
	uint8_t dfcoRetardDeg;
	/**
	 * Smooths the transition back from fuel cut. After fuel is restored, ramp timing back in over the period specified.
	 * units: s
	 * offset 3657
	 */
	scaled_channel<uint8_t, 10, 1> dfcoRetardRampInTime;
	/**
	 * offset 3658
	 */
	output_pin_e nitrousRelayPin;
	/**
	 * offset 3660
	 */
	pin_output_mode_e nitrousRelayPinMode;
	/**
	 * units: %
	 * offset 3661
	 */
	int8_t nitrousFuelAdderPercent;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3662
	 */
	uint8_t alignmentFill_at_3662[2] = {};
	/**
	 * Retard timing to remove from actual final timing (after all corrections) due to additional air.
	 * units: deg
	 * offset 3664
	 */
	float nitrousIgnitionRetard;
	/**
	 * units: Kph
	 * offset 3668
	 */
	uint16_t nitrousMinimumVehicleSpeed;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3670
	 */
	uint8_t alignmentFill_at_3670[2] = {};
	/**
	 * Exponential Average Alpha filtering parameter
	 * offset 3672
	 */
	float fuelLevelAveragingAlpha;
	/**
	 * How often do we update fuel level gauge
	 * units: seconds
	 * offset 3676
	 */
	float fuelLevelUpdatePeriodSec;
	/**
	 * Error below specified value
	 * units: v
	 * offset 3680
	 */
	float fuelLevelLowThresholdVoltage;
	/**
	 * Error above specified value
	 * units: v
	 * offset 3684
	 */
	float fuelLevelHighThresholdVoltage;
	/**
	 * offset 3688
	 */
	float afrExpAverageAlpha;
	/**
	 * Compensates for trigger delay due to belt stretch, or other electromechanical issues. beware that raising this value advances ignition timing!
	 * units: uS
	 * offset 3692
	 */
	scaled_channel<uint8_t, 1, 1> sparkHardwareLatencyCorrection;
	/**
	 * Delay before cutting fuel due to extra high oil pressure. Use this to ignore short pressure blips and sensor noise.
	 * units: sec
	 * offset 3693
	 */
	scaled_channel<uint8_t, 10, 1> maxOilPressureTimeout;
	/**
	 * units: kg/h
	 * offset 3694
	 */
	scaled_channel<uint16_t, 100, 1> idleFlowEstimateFlow[8] = {};
	/**
	 * units: %
	 * offset 3710
	 */
	scaled_channel<uint8_t, 2, 1> idleFlowEstimatePosition[8] = {};
	/**
	 * units: mg
	 * offset 3718
	 */
	int8_t airmassToTimingBins[8] = {};
	/**
	 * units: deg
	 * offset 3726
	 */
	int8_t airmassToTimingValues[8] = {};
	/**
	 * idle return target ramp duration
	 * units: seconds
	 * offset 3734
	 */
	scaled_channel<uint8_t, 10, 1> idleReturnTargetRampDuration;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 3735
	 */
	uint8_t alignmentFill_at_3735[1] = {};
	/**
	 * Voltage when the wastegate is fully open
	 * units: v
	 * offset 3736
	 */
	float wastegatePositionOpenedVoltage;
	/**
	 * Voltage when the wastegate is closed
	 * units: v
	 * offset 3740
	 */
	float wastegatePositionClosedVoltage;
	/**
	 * units: units
	 * offset 3744
	 */
	uint8_t unusedOftenChangesDuringFirmwareUpdate[END_OF_CALIBRATION_PADDING] = {};
};
static_assert(sizeof(engine_configuration_s) == 3788);

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
static_assert(sizeof(blend_table_s) == 188);

// start of persistent_config_s
struct persistent_config_s {
	/**
	 * offset 0
	 */
	engine_configuration_s engineConfiguration;
	/**
	 * offset 3788
	 */
	float tmfTable[TMF_SIZE][TMF_SIZE] = {};
	/**
	 * offset 3804
	 */
	float tmfRatioBins[TMF_SIZE] = {};
	/**
	 * offset 3812
	 */
	float tmfOpeningBins[TMF_SIZE] = {};
	/**
	 * units: Nm
	 * offset 3820
	 */
	scaled_channel<uint8_t, 1, 10> torqueTable[TORQUE_CURVE_SIZE][TORQUE_CURVE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 3824
	 */
	uint16_t torqueRpmBins[TORQUE_CURVE_SIZE] = {};
	/**
	 * units: Load
	 * offset 3828
	 */
	uint16_t torqueLoadBins[TORQUE_CURVE_SIZE] = {};
	/**
	 * units: mult
	 * offset 3832
	 */
	float postCrankingFactor[CRANKING_ENRICH_COUNT][CRANKING_ENRICH_COUNT] = {};
	/**
	 * units: count
	 * offset 3976
	 */
	uint16_t postCrankingDurationBins[CRANKING_ENRICH_COUNT] = {};
	/**
	 * units: C
	 * offset 3988
	 */
	int16_t postCrankingCLTBins[CRANKING_ENRICH_COUNT] = {};
	/**
	 * target TPS value, 0 to 100%
	 * TODO: use int8 data date once we template interpolation method
	 * units: target TPS position
	 * offset 4000
	 */
	float etbBiasBins[ETB_BIAS_CURVE_LENGTH] = {};
	/**
	 * PWM bias, open loop component of PID closed loop control
	 * units: ETB duty cycle bias
	 * offset 4032
	 */
	float etbBiasValues[ETB_BIAS_CURVE_LENGTH] = {};
	/**
	 * target Wastegate value, 0 to 100%
	 * units: target DC position
	 * offset 4064
	 */
	int8_t dcWastegateBiasBins[ETB_BIAS_CURVE_LENGTH] = {};
	/**
	 * PWM bias, open loop component of PID closed loop control
	 * units: DC wastegate duty cycle bias
	 * offset 4072
	 */
	scaled_channel<int16_t, 100, 1> dcWastegateBiasValues[ETB_BIAS_CURVE_LENGTH] = {};
	/**
	 * units: %
	 * offset 4088
	 */
	scaled_channel<uint8_t, 20, 1> iacPidMultTable[IAC_PID_MULT_SIZE][IAC_PID_MULT_SIZE] = {};
	/**
	 * units: Load
	 * offset 4152
	 */
	uint8_t iacPidMultLoadBins[IAC_PID_MULT_SIZE] = {};
	/**
	 * units: RPM
	 * offset 4160
	 */
	scaled_channel<uint8_t, 1, 10> iacPidMultRpmBins[IAC_PID_MULT_SIZE] = {};
	/**
	 * On Single Coil or Wasted Spark setups you have to lower dwell at high RPM
	 * units: RPM
	 * offset 4168
	 */
	uint16_t sparkDwellRpmBins[DWELL_CURVE_SIZE] = {};
	/**
	 * units: ms
	 * offset 4172
	 */
	scaled_channel<uint16_t, 100, 1> sparkDwellValues[DWELL_CURVE_SIZE] = {};
	/**
	 * CLT-based target RPM for automatic idle controller
	 * units: C
	 * offset 4176
	 */
	scaled_channel<int8_t, 1, 2> cltIdleRpmBins[CLT_CURVE_SIZE] = {};
	/**
	 * See idleRpmPid
	 * units: RPM
	 * offset 4182
	 */
	scaled_channel<uint8_t, 1, 20> cltIdleRpm[CLT_CURVE_SIZE] = {};
	/**
	 * units: deg
	 * offset 4188
	 */
	scaled_channel<int16_t, 10, 1> ignitionCltCorrTable[CLT_TIMING_CURVE_SIZE][CLT_TIMING_CURVE_SIZE] = {};
	/**
	 * CLT-based timing correction
	 * units: C
	 * offset 4238
	 */
	scaled_channel<int8_t, 1, 5> ignitionCltCorrTempBins[CLT_TIMING_CURVE_SIZE] = {};
	/**
	 * units: Load
	 * offset 4243
	 */
	scaled_channel<uint8_t, 1, 5> ignitionCltCorrLoadBins[CLT_TIMING_CURVE_SIZE] = {};
	/**
	 * units: x
	 * offset 4248
	 */
	float scriptCurve1Bins[SCRIPT_CURVE_16] = {};
	/**
	 * units: y
	 * offset 4312
	 */
	float scriptCurve1[SCRIPT_CURVE_16] = {};
	/**
	 * units: x
	 * offset 4376
	 */
	float scriptCurve2Bins[SCRIPT_CURVE_16] = {};
	/**
	 * units: y
	 * offset 4440
	 */
	float scriptCurve2[SCRIPT_CURVE_16] = {};
	/**
	 * units: x
	 * offset 4504
	 */
	float scriptCurve3Bins[SCRIPT_CURVE_8] = {};
	/**
	 * units: y
	 * offset 4536
	 */
	float scriptCurve3[SCRIPT_CURVE_8] = {};
	/**
	 * units: x
	 * offset 4568
	 */
	float scriptCurve4Bins[SCRIPT_CURVE_8] = {};
	/**
	 * units: y
	 * offset 4600
	 */
	float scriptCurve4[SCRIPT_CURVE_8] = {};
	/**
	 * units: x
	 * offset 4632
	 */
	float scriptCurve5Bins[SCRIPT_CURVE_8] = {};
	/**
	 * units: y
	 * offset 4664
	 */
	float scriptCurve5[SCRIPT_CURVE_8] = {};
	/**
	 * units: x
	 * offset 4696
	 */
	float scriptCurve6Bins[SCRIPT_CURVE_8] = {};
	/**
	 * units: y
	 * offset 4728
	 */
	float scriptCurve6[SCRIPT_CURVE_8] = {};
	/**
	 * units: kPa
	 * offset 4760
	 */
	float baroCorrPressureBins[BARO_CORR_SIZE] = {};
	/**
	 * units: RPM
	 * offset 4776
	 */
	float baroCorrRpmBins[BARO_CORR_SIZE] = {};
	/**
	 * units: ratio
	 * offset 4792
	 */
	float baroCorrTable[BARO_CORR_SIZE][BARO_CORR_SIZE] = {};
	/**
	 * Cranking fuel correction coefficient based on TPS
	 * units: Ratio
	 * offset 4856
	 */
	float crankingTpsCoef[CRANKING_CURVE_SIZE] = {};
	/**
	 * units: %
	 * offset 4876
	 */
	float crankingTpsBins[CRANKING_CURVE_SIZE] = {};
	/**
	 * Optional timing advance table for Cranking (see useSeparateAdvanceForCranking)
	 * units: RPM
	 * offset 4896
	 */
	uint16_t crankingAdvanceBins[CRANKING_ADVANCE_CURVE_SIZE] = {};
	/**
	 * Optional timing advance table for Cranking (see useSeparateAdvanceForCranking)
	 * units: deg
	 * offset 4904
	 */
	scaled_channel<int16_t, 100, 1> crankingAdvance[CRANKING_ADVANCE_CURVE_SIZE] = {};
	/**
	 * RPM-based idle position for coasting
	 * units: RPM
	 * offset 4912
	 */
	scaled_channel<uint8_t, 1, 100> iacCoastingRpmBins[CLT_CURVE_SIZE] = {};
	/**
	 * RPM-based idle position for coasting
	 * units: %
	 * offset 4918
	 */
	scaled_channel<uint8_t, 2, 1> iacCoasting[CLT_CURVE_SIZE] = {};
	/**
	 * offset 4924
	 */
	scaled_channel<uint8_t, 2, 1> boostTableOpenLoop[BOOST_LOAD_COUNT][BOOST_RPM_COUNT] = {};
	/**
	 * units: RPM
	 * offset 4988
	 */
	scaled_channel<uint8_t, 1, 100> boostRpmBins[BOOST_RPM_COUNT] = {};
	/**
	 * offset 4996
	 */
	uint16_t boostOpenLoopLoadBins[BOOST_LOAD_COUNT] = {};
	/**
	 * offset 5012
	 */
	scaled_channel<uint8_t, 1, 2> boostTableClosedLoop[BOOST_LOAD_COUNT][BOOST_RPM_COUNT] = {};
	/**
	 * offset 5076
	 */
	uint16_t boostClosedLoopLoadBins[BOOST_LOAD_COUNT] = {};
	/**
	 * units: %
	 * offset 5092
	 */
	uint8_t pedalToTpsTable[PEDAL_TO_TPS_SIZE][PEDAL_TO_TPS_SIZE] = {};
	/**
	 * units: %
	 * offset 5156
	 */
	uint8_t pedalToTpsPedalBins[PEDAL_TO_TPS_SIZE] = {};
	/**
	 * units: RPM
	 * offset 5164
	 */
	scaled_channel<uint8_t, 1, 100> pedalToTpsRpmBins[PEDAL_TO_TPS_SIZE] = {};
	/**
	 * CLT-based cranking position % for simple manual idle controller
	 * units: C
	 * offset 5172
	 */
	float cltCrankingCorrBins[CLT_CRANKING_CURVE_SIZE] = {};
	/**
	 * CLT-based cranking position % for simple manual idle controller
	 * units: percent
	 * offset 5196
	 */
	float cltCrankingCorr[CLT_CRANKING_CURVE_SIZE] = {};
	/**
	 * units: C
	 * offset 5220
	 */
	float afterCrankingIACtaperDurationBins[CLT_CRANKING_TAPER_CURVE_SIZE] = {};
	/**
	 * This is the duration in cycles that the IAC will take to reach its normal idle position, it can be used to hold the idle higher for a few seconds after cranking to improve startup.
	 * Should be 100 once tune is better
	 * units: cycles
	 * offset 5240
	 */
	uint16_t afterCrankingIACtaperDuration[CLT_CRANKING_TAPER_CURVE_SIZE] = {};
	/**
	 * Optional timing advance table for Idle (see useSeparateAdvanceForIdle)
	 * units: RPM
	 * offset 5250
	 */
	scaled_channel<uint8_t, 1, 50> idleAdvanceBins[IDLE_ADVANCE_CURVE_SIZE] = {};
	/**
	 * Optional timing advance table for Idle (see useSeparateAdvanceForIdle)
	 * units: deg
	 * offset 5256
	 */
	float idleAdvance[IDLE_ADVANCE_CURVE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 5280
	 */
	scaled_channel<uint8_t, 1, 10> idleVeRpmBins[IDLE_VE_SIZE] = {};
	/**
	 * units: load
	 * offset 5284
	 */
	uint8_t idleVeLoadBins[IDLE_VE_SIZE] = {};
	/**
	 * units: %
	 * offset 5288
	 */
	scaled_channel<uint16_t, 10, 1> idleVeTable[IDLE_VE_SIZE][IDLE_VE_SIZE] = {};
	/**
	 * offset 5320
	 */
	lua_script_t luaScript;
	/**
	 * units: C
	 * offset 13320
	 */
	float cltFuelCorrBins[CLT_FUEL_CURVE_SIZE] = {};
	/**
	 * units: ratio
	 * offset 13344
	 */
	float cltFuelCorr[CLT_FUEL_CURVE_SIZE] = {};
	/**
	 * units: C
	 * offset 13368
	 */
	float iatFuelCorrBins[IAT_CURVE_SIZE] = {};
	/**
	 * units: ratio
	 * offset 13392
	 */
	float iatFuelCorr[IAT_CURVE_SIZE] = {};
	/**
	 * units: ratio
	 * offset 13416
	 */
	float crankingFuelCoef[CRANKING_CURVE_SIZE] = {};
	/**
	 * units: C
	 * offset 13436
	 */
	float crankingFuelBins[CRANKING_CURVE_SIZE] = {};
	/**
	 * units: counter
	 * offset 13456
	 */
	float crankingCycleBins[CRANKING_CURVE_SIZE] = {};
	/**
	 * units: C
	 * offset 13476
	 */
	int16_t crankingCycleFuelCltBins[CRANKING_CYCLE_CLT_SIZE] = {};
	/**
	 * Base mass of the per-cylinder fuel injected during cranking. This is then modified by the multipliers for IAT, TPS ect, to give the final cranking pulse width.
	 * A reasonable starting point is 60mg per liter per cylinder.
	 * ex: 2 liter 4 cyl = 500cc/cyl, so 30mg cranking fuel.
	 * units: mg
	 * offset 13484
	 */
	float crankingCycleBaseFuel[CRANKING_CYCLE_CLT_SIZE][CRANKING_CURVE_SIZE] = {};
	/**
	 * CLT-based idle position for simple manual idle controller
	 * units: C
	 * offset 13564
	 */
	float cltIdleCorrBins[CLT_IDLE_TABLE_CLT_SIZE] = {};
	/**
	 * CLT-based idle position for simple manual idle controller
	 * units: %
	 * offset 13588
	 */
	float cltIdleCorrTable[CLT_IDLE_TABLE_RPM_SIZE][CLT_IDLE_TABLE_CLT_SIZE] = {};
	/**
	 * units: RPM
	 * offset 13636
	 */
	scaled_channel<uint8_t, 1, 100> rpmIdleCorrBins[CLT_IDLE_TABLE_RPM_SIZE] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 13638
	 */
	uint8_t alignmentFill_at_13638[2] = {};
	/**
	 * Also known as MAF transfer function.
	 * kg/hour value.
	 * By the way 2.081989116 kg/h = 1 ft3/m
	 * units: kg/hour
	 * offset 13640
	 */
	float mafDecoding[MAF_DECODING_COUNT] = {};
	/**
	 * units: V
	 * offset 13768
	 */
	float mafDecodingBins[MAF_DECODING_COUNT] = {};
	/**
	 * units: deg
	 * offset 13896
	 */
	scaled_channel<int16_t, 10, 1> ignitionIatCorrTable[IAT_IGN_CORR_COUNT][IAT_IGN_CORR_COUNT] = {};
	/**
	 * units: C
	 * offset 13968
	 */
	int8_t ignitionIatCorrTempBins[IAT_IGN_CORR_COUNT] = {};
	/**
	 * units: Load
	 * offset 13974
	 */
	scaled_channel<uint8_t, 1, 5> ignitionIatCorrLoadBins[IAT_IGN_CORR_COUNT] = {};
	/**
	 * units: deg
	 * offset 13980
	 */
	int16_t injectionPhase[INJ_PHASE_LOAD_COUNT][INJ_PHASE_RPM_COUNT] = {};
	/**
	 * units: Load
	 * offset 14052
	 */
	uint16_t injPhaseLoadBins[INJ_PHASE_LOAD_COUNT] = {};
	/**
	 * units: RPM
	 * offset 14064
	 */
	uint16_t injPhaseRpmBins[INJ_PHASE_RPM_COUNT] = {};
	/**
	 * units: onoff
	 * offset 14076
	 */
	uint8_t tcuSolenoidTable[TCU_SOLENOID_COUNT][TCU_GEAR_COUNT] = {};
	/**
	 * units: kPa
	 * offset 14080
	 */
	scaled_channel<uint16_t, 100, 1> mapEstimateTable[MAP_EST_LOAD_COUNT][MAP_EST_RPM_COUNT] = {};
	/**
	 * units: % TPS
	 * offset 14152
	 */
	scaled_channel<uint16_t, 100, 1> mapEstimateTpsBins[MAP_EST_LOAD_COUNT] = {};
	/**
	 * units: RPM
	 * offset 14164
	 */
	uint16_t mapEstimateRpmBins[MAP_EST_RPM_COUNT] = {};
	/**
	 * units: value
	 * offset 14176
	 */
	int8_t vvtTable1[VVT_TABLE_SIZE][VVT_TABLE_SIZE] = {};
	/**
	 * units: L
	 * offset 14240
	 */
	uint16_t vvtTable1LoadBins[VVT_TABLE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 14256
	 */
	uint16_t vvtTable1RpmBins[VVT_TABLE_SIZE] = {};
	/**
	 * units: value
	 * offset 14272
	 */
	int8_t vvtTable2[VVT_TABLE_SIZE][VVT_TABLE_SIZE] = {};
	/**
	 * units: L
	 * offset 14336
	 */
	uint16_t vvtTable2LoadBins[VVT_TABLE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 14352
	 */
	uint16_t vvtTable2RpmBins[VVT_TABLE_SIZE] = {};
	/**
	 * units: deg
	 * offset 14368
	 */
	scaled_channel<int16_t, 10, 1> ignitionTable[IGN_LOAD_COUNT][IGN_RPM_COUNT] = {};
	/**
	 * units: Load
	 * offset 14880
	 */
	uint16_t ignitionLoadBins[IGN_LOAD_COUNT] = {};
	/**
	 * units: RPM
	 * offset 14912
	 */
	uint16_t ignitionRpmBins[IGN_RPM_COUNT] = {};
	/**
	 * units: %
	 * offset 14944
	 */
	scaled_channel<uint16_t, 10, 1> veTable[VE_LOAD_COUNT][VE_RPM_COUNT] = {};
	/**
	 * units: {bitStringValue(fuelUnits, fuelAlgorithm) }
	 * offset 15456
	 */
	uint16_t veLoadBins[VE_LOAD_COUNT] = {};
	/**
	 * units: RPM
	 * offset 15488
	 */
	uint16_t veRpmBins[VE_RPM_COUNT] = {};
	/**
	 * units: lambda
	 * offset 15520
	 */
	scaled_channel<uint8_t, 147, 1> lambdaTable[FUEL_LOAD_COUNT][FUEL_RPM_COUNT] = {};
	/**
	 * offset 15776
	 */
	uint16_t lambdaLoadBins[FUEL_LOAD_COUNT] = {};
	/**
	 * units: RPM
	 * offset 15808
	 */
	uint16_t lambdaRpmBins[FUEL_RPM_COUNT] = {};
	/**
	 * units: value
	 * offset 15840
	 */
	float tpsTpsAccelTable[TPS_TPS_ACCEL_TABLE][TPS_TPS_ACCEL_TABLE] = {};
	/**
	 * units: from
	 * offset 16096
	 */
	float tpsTpsAccelFromRpmBins[TPS_TPS_ACCEL_TABLE] = {};
	/**
	 * units: to
	 * offset 16128
	 */
	float tpsTpsAccelToRpmBins[TPS_TPS_ACCEL_TABLE] = {};
	/**
	 * units: value
	 * offset 16160
	 */
	float scriptTable1[SCRIPT_TABLE_8][SCRIPT_TABLE_8] = {};
	/**
	 * units: L
	 * offset 16416
	 */
	int16_t scriptTable1LoadBins[SCRIPT_TABLE_8] = {};
	/**
	 * units: RPM
	 * offset 16432
	 */
	int16_t scriptTable1RpmBins[SCRIPT_TABLE_8] = {};
	/**
	 * units: value
	 * offset 16448
	 */
	float scriptTable2[TABLE_2_LOAD_SIZE][TABLE_2_RPM_SIZE] = {};
	/**
	 * units: L
	 * offset 16704
	 */
	int16_t scriptTable2LoadBins[TABLE_2_LOAD_SIZE] = {};
	/**
	 * units: RPM
	 * offset 16720
	 */
	int16_t scriptTable2RpmBins[TABLE_2_RPM_SIZE] = {};
	/**
	 * units: value
	 * offset 16736
	 */
	uint8_t scriptTable3[SCRIPT_TABLE_8][SCRIPT_TABLE_8] = {};
	/**
	 * units: L
	 * offset 16800
	 */
	int16_t scriptTable3LoadBins[SCRIPT_TABLE_8] = {};
	/**
	 * units: RPM
	 * offset 16816
	 */
	int16_t scriptTable3RpmBins[SCRIPT_TABLE_8] = {};
	/**
	 * units: value
	 * offset 16832
	 */
	uint8_t scriptTable4[SCRIPT_TABLE_8][TABLE_4_RPM] = {};
	/**
	 * units: L
	 * offset 16912
	 */
	int16_t scriptTable4LoadBins[SCRIPT_TABLE_8] = {};
	/**
	 * units: RPM
	 * offset 16928
	 */
	int16_t scriptTable4RpmBins[TABLE_4_RPM] = {};
	/**
	 * offset 16948
	 */
	uint16_t ignTrimLoadBins[IGN_TRIM_SIZE] = {};
	/**
	 * units: rpm
	 * offset 16956
	 */
	uint16_t ignTrimRpmBins[IGN_TRIM_SIZE] = {};
	/**
	 * offset 16964
	 */
	ign_cyl_trim_s ignTrims[12] = {};
	/**
	 * offset 17156
	 */
	uint16_t fuelTrimLoadBins[FUEL_TRIM_SIZE] = {};
	/**
	 * units: rpm
	 * offset 17164
	 */
	uint16_t fuelTrimRpmBins[FUEL_TRIM_SIZE] = {};
	/**
	 * offset 17172
	 */
	fuel_cyl_trim_s fuelTrims[12] = {};
	/**
	 * units: ratio
	 * offset 17364
	 */
	scaled_channel<uint16_t, 100, 1> crankingFuelCoefE100[CRANKING_CURVE_SIZE] = {};
	/**
	 * units: Airmass
	 * offset 17374
	 */
	scaled_channel<uint8_t, 1, 5> tcu_pcAirmassBins[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17376
	 */
	uint8_t tcu_pcValsR[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17378
	 */
	uint8_t tcu_pcValsN[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17380
	 */
	uint8_t tcu_pcVals1[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17382
	 */
	uint8_t tcu_pcVals2[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17384
	 */
	uint8_t tcu_pcVals3[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17386
	 */
	uint8_t tcu_pcVals4[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17388
	 */
	uint8_t tcu_pcVals12[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17390
	 */
	uint8_t tcu_pcVals23[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17392
	 */
	uint8_t tcu_pcVals34[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17394
	 */
	uint8_t tcu_pcVals21[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17396
	 */
	uint8_t tcu_pcVals32[TCU_TABLE_WIDTH] = {};
	/**
	 * units: %
	 * offset 17398
	 */
	uint8_t tcu_pcVals43[TCU_TABLE_WIDTH] = {};
	/**
	 * units: TPS
	 * offset 17400
	 */
	uint8_t tcu_tccTpsBins[8] = {};
	/**
	 * units: MPH
	 * offset 17408
	 */
	uint8_t tcu_tccLockSpeed[8] = {};
	/**
	 * units: MPH
	 * offset 17416
	 */
	uint8_t tcu_tccUnlockSpeed[8] = {};
	/**
	 * units: KPH
	 * offset 17424
	 */
	uint8_t tcu_32SpeedBins[8] = {};
	/**
	 * units: %
	 * offset 17432
	 */
	uint8_t tcu_32Vals[8] = {};
	/**
	 * units: %
	 * offset 17440
	 */
	scaled_channel<int8_t, 10, 1> throttle2TrimTable[ETB2_TRIM_SIZE][ETB2_TRIM_SIZE] = {};
	/**
	 * units: %
	 * offset 17476
	 */
	uint8_t throttle2TrimTpsBins[ETB2_TRIM_SIZE] = {};
	/**
	 * units: RPM
	 * offset 17482
	 */
	scaled_channel<uint8_t, 1, 100> throttle2TrimRpmBins[ETB2_TRIM_SIZE] = {};
	/**
	 * units: deg
	 * offset 17488
	 */
	scaled_channel<uint8_t, 4, 1> maxKnockRetardTable[KNOCK_TABLE_SIZE][KNOCK_TABLE_SIZE] = {};
	/**
	 * units: %
	 * offset 17524
	 */
	uint8_t maxKnockRetardLoadBins[KNOCK_TABLE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 17530
	 */
	scaled_channel<uint8_t, 1, 100> maxKnockRetardRpmBins[KNOCK_TABLE_SIZE] = {};
	/**
	 * units: deg
	 * offset 17536
	 */
	scaled_channel<int16_t, 10, 1> ALSTimingRetardTable[ALS_SIZE][ALS_SIZE] = {};
	/**
	 * units: TPS
	 * offset 17568
	 */
	uint16_t alsIgnRetardLoadBins[ALS_SIZE] = {};
	/**
	 * units: RPM
	 * offset 17576
	 */
	uint16_t alsIgnRetardrpmBins[ALS_SIZE] = {};
	/**
	 * units: percent
	 * offset 17584
	 */
	scaled_channel<int16_t, 10, 1> ALSFuelAdjustment[ALS_SIZE][ALS_SIZE] = {};
	/**
	 * units: TPS
	 * offset 17616
	 */
	uint16_t alsFuelAdjustmentLoadBins[ALS_SIZE] = {};
	/**
	 * units: RPM
	 * offset 17624
	 */
	uint16_t alsFuelAdjustmentrpmBins[ALS_SIZE] = {};
	/**
	 * units: ratio
	 * offset 17632
	 */
	scaled_channel<int16_t, 1, 10> ALSIgnSkipTable[ALS_SIZE][ALS_SIZE] = {};
	/**
	 * units: TPS
	 * offset 17664
	 */
	uint16_t alsIgnSkipLoadBins[ALS_SIZE] = {};
	/**
	 * units: RPM
	 * offset 17672
	 */
	uint16_t alsIgnSkiprpmBins[ALS_SIZE] = {};
	/**
	 * offset 17680
	 */
	blend_table_s ignBlends[IGN_BLEND_COUNT] = {};
	/**
	 * offset 18432
	 */
	blend_table_s veBlends[VE_BLEND_COUNT] = {};
	/**
	 * units: %
	 * offset 19184
	 */
	scaled_channel<uint16_t, 10, 1> throttleEstimateEffectiveAreaBins[THR_EST_SIZE] = {};
	/**
	 * In units of g/s normalized to choked flow conditions
	 * units: g/s
	 * offset 19208
	 */
	scaled_channel<uint16_t, 10, 1> throttleEstimateEffectiveAreaValues[THR_EST_SIZE] = {};
	/**
	 * offset 19232
	 */
	blend_table_s boostOpenLoopBlends[BOOST_BLEND_COUNT] = {};
	/**
	 * offset 19608
	 */
	blend_table_s boostClosedLoopBlends[BOOST_BLEND_COUNT] = {};
	/**
	 * units: level
	 * offset 19984
	 */
	float tcu_rangeP[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 20008
	 */
	float tcu_rangeR[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 20032
	 */
	float tcu_rangeN[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 20056
	 */
	float tcu_rangeD[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 20080
	 */
	float tcu_rangeM[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 20104
	 */
	float tcu_rangeM3[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 20128
	 */
	float tcu_rangeM2[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 20152
	 */
	float tcu_rangeM1[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 20176
	 */
	float tcu_rangePlus[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 20200
	 */
	float tcu_rangeMinus[RANGE_INPUT_COUNT] = {};
	/**
	 * units: level
	 * offset 20224
	 */
	float tcu_rangeLow[RANGE_INPUT_COUNT] = {};
	/**
	 * units: lambda
	 * offset 20248
	 */
	scaled_channel<uint8_t, 100, 1> lambdaMaxDeviationTable[LAM_SIZE][LAM_SIZE] = {};
	/**
	 * offset 20264
	 */
	uint16_t lambdaMaxDeviationLoadBins[LAM_SIZE] = {};
	/**
	 * units: RPM
	 * offset 20272
	 */
	uint16_t lambdaMaxDeviationRpmBins[LAM_SIZE] = {};
	/**
	 * units: %
	 * offset 20280
	 */
	uint8_t injectorStagingTable[INJ_STAGING_COUNT][INJ_STAGING_COUNT] = {};
	/**
	 * offset 20316
	 */
	uint16_t injectorStagingLoadBins[INJ_STAGING_COUNT] = {};
	/**
	 * units: RPM
	 * offset 20328
	 */
	uint16_t injectorStagingRpmBins[INJ_STAGING_COUNT] = {};
	/**
	 * units: deg C
	 * offset 20340
	 */
	int8_t wwCltBins[WWAE_TABLE_SIZE] = {};
	/**
	 * offset 20348
	 */
	scaled_channel<uint8_t, 100, 1> wwTauCltValues[WWAE_TABLE_SIZE] = {};
	/**
	 * offset 20356
	 */
	scaled_channel<uint8_t, 100, 1> wwBetaCltValues[WWAE_TABLE_SIZE] = {};
	/**
	 * units: kPa
	 * offset 20364
	 */
	uint8_t wwMapBins[WWAE_TABLE_SIZE] = {};
	/**
	 * offset 20372
	 */
	scaled_channel<uint8_t, 100, 1> wwTauMapValues[WWAE_TABLE_SIZE] = {};
	/**
	 * offset 20380
	 */
	scaled_channel<uint8_t, 100, 1> wwBetaMapValues[WWAE_TABLE_SIZE] = {};
	/**
	 * units: %
	 * offset 20388
	 */
	scaled_channel<uint8_t, 2, 1> hpfpLobeProfileQuantityBins[HPFP_LOBE_PROFILE_SIZE] = {};
	/**
	 * units: deg
	 * offset 20390
	 */
	scaled_channel<uint8_t, 2, 1> hpfpLobeProfileAngle[HPFP_LOBE_PROFILE_SIZE] = {};
	/**
	 * units: volts
	 * offset 20392
	 */
	uint8_t hpfpDeadtimeVoltsBins[HPFP_DEADTIME_SIZE] = {};
	/**
	 * units: ms
	 * offset 20394
	 */
	scaled_channel<uint16_t, 1000, 1> hpfpDeadtimeMS[HPFP_DEADTIME_SIZE] = {};
	/**
	 * units: kPa
	 * offset 20398
	 */
	uint16_t hpfpTarget[HPFP_TARGET_SIZE][HPFP_TARGET_SIZE] = {};
	/**
	 * units: load
	 * offset 20406
	 */
	scaled_channel<uint16_t, 10, 1> hpfpTargetLoadBins[HPFP_TARGET_SIZE] = {};
	/**
	 * units: RPM
	 * offset 20410
	 */
	scaled_channel<uint8_t, 1, 50> hpfpTargetRpmBins[HPFP_TARGET_SIZE] = {};
	/**
	 * units: %
	 * offset 20412
	 */
	int8_t hpfpCompensation[HPFP_COMPENSATION_SIZE][HPFP_COMPENSATION_SIZE] = {};
	/**
	 * units: cc/lobe
	 * offset 20416
	 */
	scaled_channel<uint16_t, 1000, 1> hpfpCompensationLoadBins[HPFP_COMPENSATION_SIZE] = {};
	/**
	 * units: RPM
	 * offset 20420
	 */
	scaled_channel<uint8_t, 1, 50> hpfpCompensationRpmBins[HPFP_COMPENSATION_SIZE] = {};
	/**
	 * units: %
	 * offset 20422
	 */
	scaled_channel<uint16_t, 100, 1> hpfpFuelMassCompensation[HPFP_FUEL_MASS_COMPENSATION_SIZE][HPFP_FUEL_MASS_COMPENSATION_SIZE] = {};
	/**
	 * units: fuel mass/mg
	 * offset 20430
	 */
	scaled_channel<uint16_t, 100, 1> hpfpFuelMassCompensationFuelMass[HPFP_FUEL_MASS_COMPENSATION_SIZE] = {};
	/**
	 * units: bar
	 * offset 20434
	 */
	scaled_channel<uint16_t, 10, 1> hpfpFuelMassCompensationFuelPressure[HPFP_FUEL_MASS_COMPENSATION_SIZE] = {};
	/**
	 * units: ms
	 * offset 20438
	 */
	scaled_channel<uint16_t, 100, 1> injectorFlowLinearization[FLOW_LINEARIZATION_PRESSURE_SIZE][FLOW_LINEARIZATION_MASS_SIZE] = {};
	/**
	 * units: fuel mass/mg
	 * offset 20446
	 */
	scaled_channel<uint16_t, 100, 1> injectorFlowLinearizationFuelMassBins[FLOW_LINEARIZATION_MASS_SIZE] = {};
	/**
	 * units: bar
	 * offset 20450
	 */
	scaled_channel<uint16_t, 10, 1> injectorFlowLinearizationPressureBins[FLOW_LINEARIZATION_PRESSURE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 20454
	 */
	uint16_t knockNoiseRpmBins[ENGINE_NOISE_CURVE_SIZE] = {};
	/**
	 * Knock sensor output knock detection threshold depending on current RPM.
	 * units: dB
	 * offset 20458
	 */
	scaled_channel<int8_t, 2, 1> knockBaseNoise[ENGINE_NOISE_CURVE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 20460
	 */
	scaled_channel<uint8_t, 1, 50> tpsTspCorrValuesBins[TPS_TPS_ACCEL_CLT_CORR_TABLE] = {};
	/**
	 * units: multiplier
	 * offset 20464
	 */
	scaled_channel<uint8_t, 50, 1> tpsTspCorrValues[TPS_TPS_ACCEL_CLT_CORR_TABLE] = {};
	/**
	 * units: C
	 * offset 20468
	 */
	scaled_channel<int8_t, 1, 5> cltRevLimitRpmBins[CLT_LIMITER_CURVE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 20472
	 */
	uint16_t cltRevLimitRpm[CLT_LIMITER_CURVE_SIZE] = {};
	/**
	 * units: volt
	 * offset 20480
	 */
	scaled_channel<uint16_t, 1000, 1> fuelLevelBins[FUEL_LEVEL_TABLE_COUNT] = {};
	/**
	 * units: %
	 * offset 20496
	 */
	uint8_t fuelLevelValues[FUEL_LEVEL_TABLE_COUNT] = {};
	/**
	 * units: volts
	 * offset 20504
	 */
	scaled_channel<uint8_t, 10, 1> dwellVoltageCorrVoltBins[DWELL_CURVE_SIZE] = {};
	/**
	 * units: multiplier
	 * offset 20506
	 */
	scaled_channel<uint8_t, 50, 1> dwellVoltageCorrValues[DWELL_CURVE_SIZE] = {};
	/**
	 * units: %
	 * offset 20508
	 */
	scaled_channel<uint8_t, 1, 1> tcu_shiftTpsBins[TCU_TABLE_WIDTH] = {};
	/**
	 * units: MPH
	 * offset 20510
	 */
	uint8_t tcu_shiftSpeed12[TCU_TABLE_WIDTH] = {};
	/**
	 * units: MPH
	 * offset 20512
	 */
	uint8_t tcu_shiftSpeed23[TCU_TABLE_WIDTH] = {};
	/**
	 * units: MPH
	 * offset 20514
	 */
	uint8_t tcu_shiftSpeed34[TCU_TABLE_WIDTH] = {};
	/**
	 * units: MPH
	 * offset 20516
	 */
	uint8_t tcu_shiftSpeed21[TCU_TABLE_WIDTH] = {};
	/**
	 * units: MPH
	 * offset 20518
	 */
	uint8_t tcu_shiftSpeed32[TCU_TABLE_WIDTH] = {};
	/**
	 * units: MPH
	 * offset 20520
	 */
	uint8_t tcu_shiftSpeed43[TCU_TABLE_WIDTH] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 20522
	 */
	uint8_t alignmentFill_at_20522[2] = {};
	/**
	 * units: ms
	 * offset 20524
	 */
	float tcu_shiftTime;
	/**
	 * units: Volts
	 * offset 20528
	 */
	scaled_channel<int16_t, 10, 1> alternatorVoltageTargetTable[ALTERNATOR_VOLTAGE_TARGET_SIZE][ALTERNATOR_VOLTAGE_TARGET_SIZE] = {};
	/**
	 * units: Load
	 * offset 20560
	 */
	uint16_t alternatorVoltageTargetLoadBins[ALTERNATOR_VOLTAGE_TARGET_SIZE] = {};
	/**
	 * units: RPM
	 * offset 20568
	 */
	uint16_t alternatorVoltageTargetRpmBins[ALTERNATOR_VOLTAGE_TARGET_SIZE] = {};
	/**
	 * units: C
	 * offset 20576
	 */
	float cltBoostCorrBins[BOOST_CURVE_SIZE] = {};
	/**
	 * units: ratio
	 * offset 20596
	 */
	float cltBoostCorr[BOOST_CURVE_SIZE] = {};
	/**
	 * units: C
	 * offset 20616
	 */
	float iatBoostCorrBins[BOOST_CURVE_SIZE] = {};
	/**
	 * units: ratio
	 * offset 20636
	 */
	float iatBoostCorr[BOOST_CURVE_SIZE] = {};
	/**
	 * units: C
	 * offset 20656
	 */
	float cltBoostAdderBins[BOOST_CURVE_SIZE] = {};
	/**
	 * offset 20676
	 */
	float cltBoostAdder[BOOST_CURVE_SIZE] = {};
	/**
	 * units: C
	 * offset 20696
	 */
	float iatBoostAdderBins[BOOST_CURVE_SIZE] = {};
	/**
	 * offset 20716
	 */
	float iatBoostAdder[BOOST_CURVE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 20736
	 */
	scaled_channel<uint8_t, 1, 100> minimumOilPressureBins[8] = {};
	/**
	 * units: kPa
	 * offset 20744
	 */
	scaled_channel<uint8_t, 1, 10> minimumOilPressureValues[8] = {};
	/**
	 * offset 20752
	 */
	blend_table_s targetAfrBlends[TARGET_AFR_BLEND_COUNT] = {};
	/**
	 * units: RPM
	 * offset 21128
	 */
	scaled_channel<uint8_t, 1, 100> trimRpmBins[FUEL_TRIM_RPM_COUNT] = {};
	/**
	 * offset 21136
	 */
	uint16_t trimLoadBins[FUEL_TRIM_LOAD_COUNT] = {};
	/**
	 * @@DYNO_RPM_STEP_TOOLTIP@@
	 * units: Rpm
	 * offset 21152
	 */
	scaled_channel<uint8_t, 1, 1> dynoRpmStep;
	/**
	 * @@DYNO_SAE_TEMPERATURE_C_TOOLTIP@@
	 * units: C
	 * offset 21153
	 */
	scaled_channel<int8_t, 1, 1> dynoSaeTemperatureC;
	/**
	 * @@DYNO_SAE_RELATIVE_HUMIDITY_TOOLTIP@@
	 * units: %
	 * offset 21154
	 */
	scaled_channel<uint8_t, 1, 1> dynoSaeRelativeHumidity;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 21155
	 */
	uint8_t alignmentFill_at_21155[1] = {};
	/**
	 * @@DYNO_SAE_BARO_TOOLTIP@@
	 * units: KPa
	 * offset 21156
	 */
	scaled_channel<float, 1, 1> dynoSaeBaro;
	/**
	 * @@DYNO_CAR_WHEEL_DIA_INCH_TOOLTIP@@
	 * units: Inch
	 * offset 21160
	 */
	scaled_channel<int8_t, 1, 1> dynoCarWheelDiaInch;
	/**
	 * @@DYNO_CAR_WHEEL_ASPECT_RATIO_TOOLTIP@@
	 * units: Aspect Ratio (height)
	 * offset 21161
	 */
	scaled_channel<int8_t, 1, 1> dynoCarWheelAspectRatio;
	/**
	 * @@DYNO_CAR_WHEEL_TIRE_WIDTH_TOOLTIP@@
	 * units: Width mm
	 * offset 21162
	 */
	scaled_channel<int16_t, 1, 1> dynoCarWheelTireWidthMm;
	/**
	 * @@DYNO_CAR_GEAR_PRIMARY_REDUCTION_TOOLTIP@@
	 * units: Units
	 * offset 21164
	 */
	scaled_channel<float, 1, 1> dynoCarGearPrimaryReduction;
	/**
	 * @@DYNO_CAR_GEAR_RATIO_TOOLTIP@@
	 * units: Units
	 * offset 21168
	 */
	scaled_channel<float, 1, 1> dynoCarGearRatio;
	/**
	 * @@DYNO_CAR_GEAR_FINAL_DRIVE_TOOLTIP@@
	 * units: Units
	 * offset 21172
	 */
	scaled_channel<float, 1, 1> dynoCarGearFinalDrive;
	/**
	 * @@DYNO_CAR_CAR_MASS_TOOLTIP@@
	 * units: Kg
	 * offset 21176
	 */
	scaled_channel<int16_t, 1, 1> dynoCarCarMassKg;
	/**
	 * @@DYNO_CAR_CARGO_MASS_TOOLTIP@@
	 * units: Kg
	 * offset 21178
	 */
	scaled_channel<int16_t, 1, 1> dynoCarCargoMassKg;
	/**
	 * @@DYNO_CAR_COEFF_OF_DRAG_TOOLTIP@@
	 * units: Coeff
	 * offset 21180
	 */
	scaled_channel<float, 1, 1> dynoCarCoeffOfDrag;
	/**
	 * @@DYNO_CAR_FRONTAL_AREA_TOOLTIP@@
	 * units: m2
	 * offset 21184
	 */
	scaled_channel<float, 1, 1> dynoCarFrontalAreaM2;
	/**
	 * units: deg
	 * offset 21188
	 */
	scaled_channel<int8_t, 10, 1> trailingSparkTable[TRAILING_SPARK_SIZE][TRAILING_SPARK_SIZE] = {};
	/**
	 * units: rpm
	 * offset 21204
	 */
	scaled_channel<uint8_t, 1, 50> trailingSparkRpmBins[TRAILING_SPARK_SIZE] = {};
	/**
	 * units: Load
	 * offset 21208
	 */
	scaled_channel<uint8_t, 1, 5> trailingSparkLoadBins[TRAILING_SPARK_SIZE] = {};
	/**
	 * units: RPM
	 * offset 21212
	 */
	scaled_channel<uint8_t, 1, 100> maximumOilPressureBins[4] = {};
	/**
	 * units: kPa
	 * offset 21216
	 */
	scaled_channel<uint8_t, 1, 10> maximumOilPressureValues[4] = {};
	/**
	 * Selects the X axis to use for the table.
	 * offset 21220
	 */
	gppwm_channel_e torqueReductionCutXaxis;
	/**
	 * How many % of ignition events will be cut
	 * units: %
	 * offset 21221
	 */
	int8_t torqueReductionIgnitionCutTable[TORQUE_TABLE_Y_SIZE][TORQUE_TABLE_X_SIZE] = {};
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 21233
	 */
	uint8_t alignmentFill_at_21233[1] = {};
	/**
	 * offset 21234
	 */
	int16_t torqueReductionCutXBins[TORQUE_TABLE_X_SIZE] = {};
	/**
	 * units: gear N°
	 * offset 21246
	 */
	int8_t torqueReductionCutGearBins[TORQUE_TABLE_Y_SIZE] = {};
	/**
	 * Selects the X axis to use for the table.
	 * offset 21248
	 */
	gppwm_channel_e torqueReductionTimeXaxis;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 21249
	 */
	uint8_t alignmentFill_at_21249[3] = {};
	/**
	 * For how long after the pin has been triggered will the cut/reduction stay active. After that, even if the pin is still triggered, torque is re-introduced
	 * units: ms
	 * offset 21252
	 */
	float torqueReductionTimeTable[TORQUE_TABLE_Y_SIZE][TORQUE_TABLE_X_SIZE] = {};
	/**
	 * offset 21300
	 */
	int16_t torqueReductionTimeXBins[TORQUE_TABLE_X_SIZE] = {};
	/**
	 * units: gear N°
	 * offset 21312
	 */
	int8_t torqueReductionTimeGearBins[TORQUE_TABLE_Y_SIZE] = {};
	/**
	 * Selects the X axis to use for the table.
	 * offset 21314
	 */
	gppwm_channel_e torqueReductionIgnitionRetardXaxis;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 21315
	 */
	uint8_t alignmentFill_at_21315[1] = {};
	/**
	 * How many degrees of timing advance will be reduced during the Torque Reduction Time
	 * units: deg
	 * offset 21316
	 */
	float torqueReductionIgnitionRetardTable[TORQUE_TABLE_Y_SIZE][TORQUE_TABLE_X_SIZE] = {};
	/**
	 * offset 21364
	 */
	int16_t torqueReductionIgnitionRetardXBins[TORQUE_TABLE_X_SIZE] = {};
	/**
	 * units: gear N°
	 * offset 21376
	 */
	int8_t torqueReductionIgnitionRetardGearBins[TORQUE_TABLE_Y_SIZE] = {};
	/**
	 * offset 21378
	 */
	Gpio communityCommsLedPin;
	/**
	 * offset 21380
	 */
	int technicalDebt7738;
};
static_assert(sizeof(persistent_config_s) == 21384);

// end
// this section was generated automatically by rusEFI tool config_definition-all.jar based on (unknown script) integration/rusefi_config.txt
