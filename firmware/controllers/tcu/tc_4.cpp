/**
 * @file tc_4.cpp
 * @brief Generic 4-speed transmission controller.
 *
 * Controls the shift solenoids of a simple 4-speed automatic transmission, selecting
 * the solenoid pattern for the requested gear.
 */

#include "pch.h"

#include "tc_4.h"

#if EFI_TCU
Generic4TransmissionController generic4TransmissionController;
static SimplePwm pcPwm("Pressure Control");

void Generic4TransmissionController::init() {
	SimpleTransmissionController::init();

	enginePins.tcuTccOnoffSolenoid.initPin("TCC On/Off Solenoid", engineConfiguration->tcu_tcc_onoff_solenoid, engineConfiguration->tcu_tcc_onoff_solenoid_mode);

	enginePins.tcuPcSolenoid.initPin("Pressure Control Solenoid", engineConfiguration->tcu_pc_solenoid_pin, engineConfiguration->tcu_pc_solenoid_pin_mode);
	startSimplePwm(&pcPwm,
								 "Line Pressure",
								 &engine->scheduler,
								 &enginePins.tcuPcSolenoid,
								 engineConfiguration->tcu_pc_solenoid_freq,
								 0);
}

void Generic4TransmissionController::update(gear_e gear) {
	if (gear != getCurrentGear()) {
		shiftingFrom = getCurrentGear();
		isShifting = true;
		measureShiftTime(gear);
	}

	// set torque converter and pressure control state
	updateTccLockup(gear);
	setPcState();

	setCurrentGear(gear);

	SimpleTransmissionController::update(gear);

	float time = isShiftCompleted();
	// 0 means shift is not completed
	if (time != 0) {
		lastShiftTime = time;
		isShifting = false;
	}
}

// Simplified line pressure control: a 3-band driver-demand read on TPS (Low/Mid/High), each
// with its own duty for cruising vs. shifting -- 6 duties total, the same for every gear.
// Band transitions use separate rising/falling TPS thresholds (Schmitt trigger) so cruising
// right at a boundary doesn't chatter the duty between two values.
void Generic4TransmissionController::setPcState() {
	auto tps = Sensor::get(SensorType::DriverThrottleIntent);
	if (!tps.Valid) {
		return;
	}

	switch (tcu_pcDemandBand) {
	case 0: // Low
		if (tps.Value >= config->tcu_pcLowMidTpsEnter) {
			tcu_pcDemandBand = 1;
		}
		break;
	case 2: // High
		if (tps.Value <= config->tcu_pcMidHighTpsExit) {
			tcu_pcDemandBand = 1;
		}
		break;
	default: // Mid
		if (tps.Value >= config->tcu_pcMidHighTpsEnter) {
			tcu_pcDemandBand = 2;
		} else if (tps.Value <= config->tcu_pcLowMidTpsExit) {
			tcu_pcDemandBand = 0;
		}
		break;
	}

	uint8_t duty;
	switch (tcu_pcDemandBand) {
	case 0:
		duty = isShifting ? config->tcu_pcLowShiftDuty : config->tcu_pcLowCruiseDuty;
		break;
	case 2:
		duty = isShifting ? config->tcu_pcHighShiftDuty : config->tcu_pcHighCruiseDuty;
		break;
	default:
		duty = isShifting ? config->tcu_pcMidShiftDuty : config->tcu_pcMidCruiseDuty;
		break;
	}

	pressureControlDuty = duty;
	pcPwm.setSimplePwmDutyCycle(0.01f * duty);
}

Generic4TransmissionController* getGeneric4TransmissionController() {
	return &generic4TransmissionController;
}

// here we have default 4R70W calibration
void configureTcu4R70W() {
	// TCU Software Config
	engineConfiguration->tcuEnabled = true;
	engineConfiguration->gearControllerMode = GearControllerMode::Generic;
	engineConfiguration->transmissionControllerMode = TransmissionControllerMode::Generic4;

	// TCU Inputs
	// Buttonshift
	engineConfiguration->tcuUpshiftButtonPin = Gpio::E14;
	engineConfiguration->tcuUpshiftButtonPinMode = PI_PULLUP;
	engineConfiguration->tcuDownshiftButtonPin = Gpio::E13;
	engineConfiguration->tcuDownshiftButtonPinMode = PI_PULLUP;
	// Analog range sensor - used in early 4R70
	engineConfiguration->tcu_rangeSensorBiasResistor = 1753;
	engineConfiguration->tcu_rangeAnalogInput[0] = EFI_ADC_4;
	// Two digital for +/- add-on
	engineConfiguration->tcu_rangeInput[1] = Gpio::C6;
	engineConfiguration->tcu_rangeInputMode[1] = PI_PULLUP;
	engineConfiguration->tcu_rangeInput[2] = Gpio::E15;
	engineConfiguration->tcu_rangeInputMode[2] = PI_PULLUP;
  // Range voltages/digital states
	config->tcu_rangeR[0] = 6072.0;
	config->tcu_rangeR[1] = 0.0;
	config->tcu_rangeR[2] = 0.0;
	config->tcu_rangeP[0] = 18500.0;
	config->tcu_rangeP[1] = 0.0;
	config->tcu_rangeP[2] = 0.0;
	config->tcu_rangeN[0] = 3028.0;
	config->tcu_rangeN[1] = 0.0;
	config->tcu_rangeN[2] = 0.0;
	config->tcu_rangeD[0] = 1645.0;
	config->tcu_rangeD[1] = 0.0;
	config->tcu_rangeD[2] = 0.0;
	config->tcu_rangeM2[0] = 864.0;
	config->tcu_rangeM2[1] = 0.0;
	config->tcu_rangeM2[2] = 0.0;
	config->tcu_rangeM1[0] = 358.0;
	config->tcu_rangeM1[1] = 0.0;
	config->tcu_rangeM1[2] = 0.0;
	config->tcu_rangePlus[0] = 0.0;
	config->tcu_rangePlus[1] = 1.0;
	config->tcu_rangePlus[2] = 0.0;
	config->tcu_rangeMinus[0] = 0.0;
	config->tcu_rangeMinus[1] = 0.0;
	config->tcu_rangeMinus[2] = 1.0;
	// Disable these states
	config->tcu_rangeM[1] = 3.0;
	config->tcu_rangeM3[1] = 3.0;
	config->tcu_rangeLow[1] = 3.0;

	// TCU Outputs
	engineConfiguration->tcu_tcc_onoff_solenoid = Gpio::B8;
	engineConfiguration->tcu_tcc_onoff_solenoid_mode = OM_DEFAULT;
	engineConfiguration->tcu_pc_solenoid_pin = Gpio::E1;
	engineConfiguration->tcu_pc_solenoid_pin_mode = OM_DEFAULT;
	engineConfiguration->tcu_pc_solenoid_freq = 240;
	engineConfiguration->tcu_solenoid[0] = Gpio::B6;
	engineConfiguration->tcu_solenoid_mode[0] = OM_DEFAULT;
	engineConfiguration->tcu_solenoid[1] = Gpio::B5;
	engineConfiguration->tcu_solenoid_mode[1] = OM_DEFAULT;
	// Reverse
	config->tcuSolenoidTable[0][0] = 1;
	config->tcuSolenoidTable[1][0] = 0;
	// Neutral
	config->tcuSolenoidTable[0][1] = 1;
	config->tcuSolenoidTable[1][1] = 0;
	// 1
	config->tcuSolenoidTable[0][2] = 1;
	config->tcuSolenoidTable[1][2] = 0;
	// 2
	config->tcuSolenoidTable[0][3] = 0;
	config->tcuSolenoidTable[1][3] = 0;
	// 3
	config->tcuSolenoidTable[0][4] = 0;
	config->tcuSolenoidTable[1][4] = 1;
	// 4
	config->tcuSolenoidTable[0][5] = 1;
	config->tcuSolenoidTable[1][5] = 1;

	// Pressure Control: 3-band TPS demand (Low/Mid/High) x cruise/shift, same for every gear.
	// Hysteresis on the band thresholds avoids duty chatter at a fixed cruising TPS.
	config->tcu_pcLowMidTpsEnter = 15;
	config->tcu_pcLowMidTpsExit = 10;
	config->tcu_pcMidHighTpsEnter = 55;
	config->tcu_pcMidHighTpsExit = 48;
	config->tcu_pcLowCruiseDuty = 30;
	config->tcu_pcMidCruiseDuty = 40;
	config->tcu_pcHighCruiseDuty = 60;
	config->tcu_pcLowShiftDuty = 45;
	config->tcu_pcMidShiftDuty = 55;
	config->tcu_pcHighShiftDuty = 80;

	// TCC Control
	config->tcu_tccTpsBins[0] = 11.0;
	config->tcu_tccTpsBins[1] = 22.0;
	config->tcu_tccTpsBins[2] = 33.0;
	config->tcu_tccTpsBins[3] = 44.0;
	config->tcu_tccTpsBins[4] = 55.0;
	config->tcu_tccTpsBins[5] = 66.0;
	config->tcu_tccTpsBins[6] = 77.0;
	config->tcu_tccTpsBins[7] = 88.0;
	config->tcu_tccLockSpeed[0] = 40.0;
	config->tcu_tccLockSpeed[1] = 45.0;
	config->tcu_tccLockSpeed[2] = 52.0;
	config->tcu_tccLockSpeed[3] = 60.0;
	config->tcu_tccLockSpeed[4] = 70.0;
	config->tcu_tccLockSpeed[5] = 83.0;
	config->tcu_tccLockSpeed[6] = 97.0;
	config->tcu_tccLockSpeed[7] = 115.0;
	config->tcu_tccUnlockSpeed[0] = 30.0;
	config->tcu_tccUnlockSpeed[1] = 35.0;
	config->tcu_tccUnlockSpeed[2] = 41.0;
	config->tcu_tccUnlockSpeed[3] = 49.0;
	config->tcu_tccUnlockSpeed[4] = 53.0;
	config->tcu_tccUnlockSpeed[5] = 67.0;
	config->tcu_tccUnlockSpeed[6] = 78.0;
	config->tcu_tccUnlockSpeed[7] = 93.0;
	// preserve prior behavior: lock-up only in top gear, no minimum coolant temp gate
	config->tcu_tccMinGear = GEAR_4;
	config->tcu_tccMinClt = 0;

	// Shift Config
	config->tcu_shiftTime = 600.0;
	config->tcu_shiftTpsBins[0] = 11.0;
	config->tcu_shiftTpsBins[1] = 22.0;
	config->tcu_shiftTpsBins[2] = 33.0;
	config->tcu_shiftTpsBins[3] = 44.0;
	config->tcu_shiftTpsBins[4] = 55.0;
	config->tcu_shiftTpsBins[5] = 67.0;
	config->tcu_shiftTpsBins[6] = 76.0;
	config->tcu_shiftTpsBins[7] = 88.0;
	config->tcu_shiftSpeed12[0] = 10.0;
	config->tcu_shiftSpeed12[1] = 12.0;
	config->tcu_shiftSpeed12[2] = 16.0;
	config->tcu_shiftSpeed12[3] = 23.0;
	config->tcu_shiftSpeed12[4] = 28.0;
	config->tcu_shiftSpeed12[5] = 32.0;
	config->tcu_shiftSpeed12[6] = 38.0;
	config->tcu_shiftSpeed12[7] = 42.0;
	config->tcu_shiftSpeed23[0] = 20.0;
	config->tcu_shiftSpeed23[1] = 25.0;
	config->tcu_shiftSpeed23[2] = 31.0;
	config->tcu_shiftSpeed23[3] = 37.0;
	config->tcu_shiftSpeed23[4] = 45.0;
	config->tcu_shiftSpeed23[5] = 55.0;
	config->tcu_shiftSpeed23[6] = 63.0;
	config->tcu_shiftSpeed23[7] = 72.0;
	config->tcu_shiftSpeed34[0] = 35.0;
	config->tcu_shiftSpeed34[1] = 40.0;
	config->tcu_shiftSpeed34[2] = 47.0;
	config->tcu_shiftSpeed34[3] = 55.0;
	config->tcu_shiftSpeed34[4] = 65.0;
	config->tcu_shiftSpeed34[5] = 78.0;
	config->tcu_shiftSpeed34[6] = 92.0;
	config->tcu_shiftSpeed34[7] = 110.0;
	config->tcu_shiftSpeed21[0] = 5.0;
	config->tcu_shiftSpeed21[1] = 7.0;
	config->tcu_shiftSpeed21[2] = 11.0;
	config->tcu_shiftSpeed21[3] = 17.0;
	config->tcu_shiftSpeed21[4] = 20.0;
	config->tcu_shiftSpeed21[5] = 23.0;
	config->tcu_shiftSpeed21[6] = 26.0;
	config->tcu_shiftSpeed21[7] = 32.0;
	config->tcu_shiftSpeed32[0] = 10.0;
	config->tcu_shiftSpeed32[1] = 13.0;
	config->tcu_shiftSpeed32[2] = 21.0;
	config->tcu_shiftSpeed32[3] = 30.0;
	config->tcu_shiftSpeed32[4] = 35.0;
	config->tcu_shiftSpeed32[5] = 45.0;
	config->tcu_shiftSpeed32[6] = 50.0;
	config->tcu_shiftSpeed32[7] = 55.0;
	config->tcu_shiftSpeed43[0] = 25.0;
	config->tcu_shiftSpeed43[1] = 30.0;
	config->tcu_shiftSpeed43[2] = 36.0;
	config->tcu_shiftSpeed43[3] = 44.0;
	config->tcu_shiftSpeed43[4] = 48.0;
	config->tcu_shiftSpeed43[5] = 62.0;
	config->tcu_shiftSpeed43[6] = 73.0;
	config->tcu_shiftSpeed43[7] = 88.0;
}

#endif // EFI_TCU
