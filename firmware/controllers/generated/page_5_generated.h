// this section was generated automatically by rusEFI tool config_definition-all.jar based on gen_config.sh integration/config_page_5.txt
// by class com.rusefi.output.CHeaderConsumer
// begin
#pragma once
#include "rusefi_types.h"
// start of page5_s
struct page5_s {
	/**
	 * Downshift Blipper: master enable. Blips the ETB to rev-match a single-gear (N to N-1) downshift.
	offset 0 bit 0 */
	bool downshiftBlipperEnabled : 1 {};
	/**
	 * Downshift Blipper: require the brake pedal to be pressed before a blip is allowed (heel-and-toe behavior).
	offset 0 bit 1 */
	bool downshiftBlipperRequireBrake : 1 {};
	/**
	 * Downshift Blipper: scale the commanded throttle by a Lua gauge multiplier curve.
	offset 0 bit 2 */
	bool downshiftBlipperUseLuaGauge : 1 {};
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
	/**
	 * Downshift Blipper: Lua gauge index (0-7 for LuaGauge1-8) used as the throttle multiplier input.
	 * offset 4
	 */
	uint8_t downshiftBlipperLuaGauge;
	/**
	 * Downshift Blipper: driver pedal % above which the blipper disables (driver is manually blipping).
	 * units: %
	 * offset 5
	 */
	uint8_t downshiftBlipperDriverTpsThreshold;
	/**
	 * Downshift Blipper: maximum throttle % the PID is allowed to command.
	 * units: %
	 * offset 6
	 */
	uint8_t downshiftBlipperMaxTpsLimit;
	/**
	 * unused
	 * offset 7
	 */
	uint8_t downshiftBlipperAlignmentPad;
	/**
	 * Downshift Blipper: maximum duration of an active blip before it is force-terminated (safety timeout).
	 * units: ms
	 * offset 8
	 */
	uint16_t downshiftBlipperMaxTimeMs;
	/**
	 * Downshift Blipper: lockout after a blip during which another blip cannot start (anti-chatter).
	 * units: ms
	 * offset 10
	 */
	uint16_t downshiftBlipperLockoutTimeMs;
	/**
	 * Downshift Blipper: minimum engine RPM required to start a blip.
	 * units: rpm
	 * offset 12
	 */
	uint16_t downshiftBlipperMinRpm;
	/**
	 * Downshift Blipper: blip is blocked when the computed target RPM exceeds this, to prevent over-rev.
	 * units: rpm
	 * offset 14
	 */
	uint16_t downshiftBlipperMaxRpm;
	/**
	 * Downshift Blipper: time to ramp the ETB up to the first PID value at blip start.
	 * units: ms
	 * offset 16
	 */
	uint16_t downshiftBlipRampOpenMs;
	/**
	 * Downshift Blipper: time to ramp the ETB back to the pedal target after the blip cuts.
	 * units: ms
	 * offset 18
	 */
	uint16_t downshiftBlipRampCloseMs;
	/**
	 * Downshift Blipper: cut the blip this many RPM below the target (early-cut buffer).
	 * units: rpm
	 * offset 20
	 */
	uint16_t downshiftBlipRpmOffset;
	/**
	 * Downshift Blipper: minimum vehicle speed to allow blipping.
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 22
	 */
	uint16_t downshiftBlipperMinVss;
	/**
	 * Downshift Blipper: PID proportional gain. Error is normalized to per-100-RPM units, output is throttle %.
	 * offset 24
	 */
	float downshiftBlipperKp;
	/**
	 * Downshift Blipper: PID integral gain.
	 * offset 28
	 */
	float downshiftBlipperKi;
	/**
	 * Downshift Blipper: PID derivative gain.
	 * offset 32
	 */
	float downshiftBlipperKd;
	/**
	 * Downshift Blipper: Lua gauge value bins for the throttle multiplier curve.
	 * units: x
	 * offset 36
	 */
	float downshiftBlipperLuaMultBins[8] = {};
	/**
	 * Downshift Blipper: throttle multiplier values (0-1) interpolated against the bins.
	 * units: mult
	 * offset 68
	 */
	float downshiftBlipperLuaMultValues[8] = {};
	/**
	 * Exhaust cutout output type: Digital on/off, PWM, or H-Bridge motor
	 * offset 100
	 */
	exhaust_cutout_output_mode_e exhaustCutoutOutputMode;
	/**
	 * Exhaust cutout activation mode
	 * offset 101
	 */
	exhaust_cutout_activation_e exhaustCutoutActivationMode;
	/**
	 * Cutout behavior when switch is OFF or gauge is below threshold
	 * offset 102
	 */
	exhaust_cutout_behavior_e exhaustCutoutBehaviorLow;
	/**
	 * Cutout behavior when switch is ON or gauge is above threshold
	 * offset 103
	 */
	exhaust_cutout_behavior_e exhaustCutoutBehaviorHigh;
	/**
	 * Lua gauge index for activation
	 * offset 104
	 */
	lua_gauge_e exhaustCutoutLuaGauge;
	/**
	 * Lua gauge comparison direction
	 * offset 105
	 */
	lua_gauge_meaning_e exhaustCutoutLuaGaugeMeaning;
	/**
	 * Hardware switch input pin
	 * offset 106
	 */
	switch_input_pin_e exhaustCutoutSwitchPin;
	/**
	 * offset 108
	 */
	pin_input_mode_e exhaustCutoutSwitchPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 109
	 */
	uint8_t alignmentFill_at_109[1] = {};
	/**
	 * Cutout output pin (digital) or H-Bridge IN1 (Open direction)
	 * offset 110
	 */
	output_pin_e exhaustCutoutOutputPin;
	/**
	 * offset 112
	 */
	pin_output_mode_e exhaustCutoutOutputPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 113
	 */
	uint8_t alignmentFill_at_113[1] = {};
	/**
	 * H-Bridge IN2 pin (Close direction)
	 * offset 114
	 */
	output_pin_e exhaustCutoutHBridgePin2;
	/**
	 * offset 116
	 */
	pin_output_mode_e exhaustCutoutHBridgePin2Mode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 117
	 */
	uint8_t alignmentFill_at_117[1] = {};
	/**
	 * Status LED output pin
	 * offset 118
	 */
	output_pin_e exhaustCutoutLedPin;
	/**
	 * offset 120
	 */
	pin_output_mode_e exhaustCutoutLedPinMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 121
	 */
	uint8_t alignmentFill_at_121[3] = {};
	/**
	 * Lua gauge activation threshold
	 * offset 124
	 */
	float exhaustCutoutLuaGaugeThreshold;
	/**
	 * RPM threshold to open cutout (0 to disable)
	 * units: rpm
	 * offset 128
	 */
	uint16_t exhaustCutoutOpenRpm;
	/**
	 * TPS threshold to open cutout (0 to disable)
	 * units: %
	 * offset 130
	 */
	uint8_t exhaustCutoutOpenTps;
	/**
	 * Anti-blip: TPS must exceed threshold for this long before opening
	 * units: s
	 * offset 131
	 */
	scaled_channel<uint8_t, 10, 1> exhaustCutoutTpsDelayS;
	/**
	 * MAP/Boost threshold to open cutout (0 to disable)
	 * units: kPa
	 * offset 132
	 */
	float exhaustCutoutOpenMapKpa;
	/**
	 * Hold-open time after triggers clear
	 * units: s
	 * offset 136
	 */
	scaled_channel<uint8_t, 10, 1> exhaustCutoutClosingDelayS;
	/**
	 * H-Bridge motor drive time (de-energizes after). Also controls LED blink duration and actuator test step duration.
	 * units: s
	 * offset 137
	 */
	scaled_channel<uint8_t, 10, 1> exhaustCutoutMoveDurationS;
	/**
	 * PWM output frequency (PWM mode only)
	 * units: Hz
	 * offset 138
	 */
	uint16_t exhaustCutoutPwmFrequency;
	/**
	 * PWM duty when cutout is OPEN (PWM mode only)
	 * units: %
	 * offset 140
	 */
	scaled_channel<uint8_t, 1, 1> exhaustCutoutPwmOpenDuty;
	/**
	 * PWM duty when cutout is CLOSED (PWM mode only)
	 * units: %
	 * offset 141
	 */
	scaled_channel<uint8_t, 1, 1> exhaustCutoutPwmClosedDuty;
	/**
	 * H-Bridge IN1/IN2 PWM frequency (H-Bridge mode only)
	 * units: Hz
	 * offset 142
	 */
	uint16_t exhaustCutoutHBridgePwmFrequency;
	/**
	 * H-Bridge motor drive duty cycle (H-Bridge mode only)
	 * units: %
	 * offset 144
	 */
	scaled_channel<uint8_t, 1, 1> exhaustCutoutHBridgeDutyCycle;
	/**
	 * Engine SM: TPS threshold used for shift-direction disambiguation (above = driver was on throttle = upshift).
	 * units: %
	 * offset 145
	 */
	uint8_t smShiftTpsThreshold;
	/**
	 * Engine SM: TPS threshold above which the engine is at Wide Open Throttle.
	 * units: %
	 * offset 146
	 */
	uint8_t smWotTpsThreshold;
	/**
	 * Engine SM: slow-callback periods (50 ms each) to hold the Transient state after the accel-enrichment threshold drops. 0 = no hold-off.
	 * units: callbacks
	 * offset 147
	 */
	uint8_t smTransientHoldoffCallbacks;
	/**
	 * Engine SM: Clutch switch that signals an upshift. 'Clutch Up' fires on pedal release
	 * units: 'Clutch Down' fires on full press. Flat-shift torque reduction always overrides this.
	 * offset 148
	 */
	sm_clutch_switch_e smUpshiftClutchSwitch;
	/**
	 * Engine SM: Clutch switch that signals a downshift.
	 * offset 149
	 */
	sm_clutch_switch_e smDownshiftClutchSwitch;
	/**
	 * Engine SM: Method used to confirm shift direction. RPM Rate: upshift=RPM drops, downshift=RPM rises. VSS Rate: upshift=VSS rises (accel), downshift=VSS falls (decel). Both require configured rate thresholds.
	 * offset 150
	 */
	sm_shift_detection_mode_e smShiftDetectionMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 151
	 */
	uint8_t alignmentFill_at_151[1] = {};
	/**
	 * Engine SM: Sensor history window used for shift direction evaluation (100-1000ms).
	 * units: ms
	 * offset 152
	 */
	uint16_t smShiftLookbackMs;
	/**
	 * Engine SM: Delay after Clutch-Up switch fires before evaluating shift direction. Compensates for the time between electrical and mechanical clutch disengagement.
	 * units: ms
	 * offset 154
	 */
	uint16_t smClutchUpDisengagementDelayMs;
	/**
	 * Engine SM: Rate-of-change magnitude to confirm upshift in RPM Rate or VSS Rate mode. RPM mode: RPM/s drop. VSS mode: km/h/s rise.
	 * units: /s
	 * offset 156
	 */
	int16_t smUpshiftRateThreshold;
	/**
	 * Engine SM: Rate-of-change magnitude to confirm downshift in RPM Rate or VSS Rate mode. RPM mode: RPM/s rise. VSS mode: km/h/s fall.
	 * units: /s
	 * offset 158
	 */
	int16_t smDownshiftRateThreshold;
	/**
	 * Delay after overrun starts before pops and bangs activates.
	 * units: sec
	 * offset 160
	 */
	scaled_channel<uint8_t, 10, 1> popsAndBangsDelay;
	/**
	 * Duration of pops and bangs mode. 0 = indefinitely active (DFCO will never engage).
	 * units: sec
	 * offset 161
	 */
	scaled_channel<uint8_t, 10, 1> popsAndBangsDuration;
	/**
	 * Additional idle / ETB % while pops and bangs is active.
	 * units: %
	 * offset 162
	 */
	uint8_t popsAndBangsAirAdd;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 163
	 */
	uint8_t alignmentFill_at_163[1] = {};
	/**
	 * Activate above this RPM.
	 * units: rpm
	 * offset 164
	 */
	uint16_t popsAndBangsRpmHigh;
	/**
	 * Deactivate below this RPM.
	 * units: rpm
	 * offset 166
	 */
	uint16_t popsAndBangsRpmLow;
	/**
	 * Never activate above this RPM.
	 * units: rpm
	 * offset 168
	 */
	uint16_t popsAndBangsRpmMax;
	/**
	 * Do not activate below this coolant temperature (cold engine protection).
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 170
	 */
	int16_t popsAndBangsCltMin;
	/**
	 * Do not activate above this coolant temperature (overheating protection).
	 * units: {bitStringValue(unitsLabels, useMetricOnInterface)}
	 * offset 172
	 */
	int16_t popsAndBangsCltMax;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 174
	 */
	uint8_t alignmentFill_at_174[2] = {};
	/**
	 * Flat ignition timing override when pops and bangs is active.
	 * units: deg
	 * offset 176
	 */
	float popsAndBangsTimingOverride;
	/**
	 * VE override percentage when pops and bangs is active.
	 * units: %
	 * offset 180
	 */
	float popsAndBangsVeOverride;
	/**
	 * Condition that disables pops and bangs while active.
	 * offset 184
	 */
	pops_and_bangs_disable_mode_e popsAndBangsDisableMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 185
	 */
	uint8_t alignmentFill_at_185[1] = {};
	/**
	 * Switch pin that disables pops and bangs when active.
	 * offset 186
	 */
	switch_input_pin_e popsAndBangsDisablePin;
	/**
	 * offset 188
	 */
	pin_input_mode_e popsAndBangsDisablePinMode;
	/**
	 * Lua gauge used to disable pops and bangs.
	 * offset 189
	 */
	lua_gauge_e popsAndBangsLuaGauge;
	/**
	 * offset 190
	 */
	lua_gauge_meaning_e popsAndBangsLuaGaugeMeaning;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 191
	 */
	uint8_t alignmentFill_at_191[1] = {};
	/**
	 * Lua gauge threshold for disabling pops and bangs.
	 * offset 192
	 */
	float popsAndBangsLuaGaugeValue;
	/**
	 * Seconds after launch control exits before deactivating CDV solenoid. Set 0 to deactivate immediately.
	 * units: s
	 * offset 196
	 */
	scaled_channel<uint8_t, 10, 1> cdvExitDelay;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 197
	 */
	uint8_t alignmentFill_at_197[1] = {};
	/**
	 * Deactivate CDV above this vehicle speed. Set 0 to disable.
	 * units: {bitStringValue(velocityUnitsLabels, useMetricOnInterface)}
	 * offset 198
	 */
	uint16_t cdvExitVss;
	/**
	 * RPM added to closed-loop idle target when returning from off-idle (Running/Coasting) conditions. PID chases this elevated target to prevent stalling.
	 * units: RPM
	 * offset 200
	 */
	int16_t offIdleRpmAdder;
	/**
	 * Time to wait after RPM stabilizes before starting the off-idle adder decay.
	 * units: sec
	 * offset 202
	 */
	scaled_channel<uint8_t, 10, 1> offIdleWaitTime;
	/**
	 * Time over which the off-idle RPM adder linearly decays from max to zero after the wait expires.
	 * units: sec
	 * offset 203
	 */
	scaled_channel<uint8_t, 10, 1> offIdleRpmAdderDecayTime;
	/**
	 * RPM rate-of-change (RPM/s) below which RPM is considered stable when returning to idle. Transition from Stabilizing to Waiting occurs when dRPM/s falls below this value.
	 * units: RPM/s
	 * offset 204
	 */
	scaled_channel<uint8_t, 1, 10> offIdleRpmStabilityThreshold;
	/**
	 * Lua gauge used as the limiter input axis.
	 * offset 205
	 */
	lua_gauge_e luaLimiterGaugeSelect;
	/**
	 * Fuel pump output mode: Single relay, Dual relay with secondary activation, or PWM pressure control
	 * offset 206
	 */
	fuel_pump_mode_e fuelPumpMode;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 207
	 */
	uint8_t alignmentFill_at_207[1] = {};
	/**
	 * Secondary pump activates above this RPM (Dual mode)
	 * units: rpm
	 * offset 208
	 */
	uint16_t secondaryFpActivationRpm;
	/**
	 * Secondary pump activates above this MAP load (Dual mode)
	 * units: %
	 * offset 210
	 */
	uint8_t secondaryFpActivationLoad;
	/**
	 * Secondary pump activates above this TPS (Dual mode)
	 * units: %
	 * offset 211
	 */
	uint8_t secondaryFpActivationTps;
	/**
	 * Secondary pump deactivates when RPM drops this far below activation threshold (Dual mode)
	 * units: rpm
	 * offset 212
	 */
	uint16_t secondaryFpRpmHysteresis;
	/**
	 * Secondary pump deactivates when load drops this far below activation threshold (Dual mode)
	 * units: %
	 * offset 214
	 */
	uint8_t secondaryFpLoadHysteresis;
	/**
	 * Secondary pump deactivates when TPS drops this far below activation threshold (Dual mode)
	 * units: %
	 * offset 215
	 */
	uint8_t secondaryFpTpsHysteresis;
	/**
	 * Fuel pump PWM frequency (PWM mode)
	 * units: Hz
	 * offset 216
	 */
	uint16_t fuelPumpPwmFrequency;
	/**
	 * Minimum PWM duty - keeps pump spinning at low demand (PWM mode)
	 * units: %
	 * offset 218
	 */
	uint8_t fuelPumpMinDuty;
	/**
	 * Maximum PWM duty - also held during prime (PWM mode)
	 * units: %
	 * offset 219
	 */
	uint8_t fuelPumpMaxDuty;
	/**
	 * PID integrator lower limit (PWM mode)
	 * offset 220
	 */
	int16_t fuelPump_iTermMin;
	/**
	 * PID integrator upper limit (PWM mode)
	 * offset 222
	 */
	int16_t fuelPump_iTermMax;
	/**
	 * units: value
	 * offset 224
	 */
	float luaLimiterRpmAddBins[LUA_LIMITER_CURVE_SIZE] = {};
	/**
	 * units: RPM
	 * offset 256
	 */
	float luaLimiterRpmAdd[LUA_LIMITER_CURVE_SIZE] = {};
	/**
	 * units: value
	 * offset 288
	 */
	float luaLimiterBoostAddBins[LUA_LIMITER_CURVE_SIZE] = {};
	/**
	 * units: kPa
	 * offset 320
	 */
	float luaLimiterBoostAdd[LUA_LIMITER_CURVE_SIZE] = {};
};
static_assert(sizeof(page5_s) == 352);

// end
// this section was generated automatically by rusEFI tool config_definition-all.jar based on gen_config.sh integration/config_page_5.txt
