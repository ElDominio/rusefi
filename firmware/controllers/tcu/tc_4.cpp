/**
 * @file tc_4.cpp
 * @brief Generic 4-speed transmission controller.
 *
 * Drives a 4-speed automatic's shift solenoids (a per-gear on/off truth table,
 * updateShiftSolenoids()) and, if wired, its line pressure/TCC lock-up solenoids
 * (setPcState()/updateTccLockup()) -- a board that only wires the shift solenoid pins gets
 * shift-only behavior for free, since the EPC/TCC pins simply no-op when left unconfigured.
 */

#include "pch.h"

#include "tc_4.h"
#include "table_helper.h"
#include "gear_detector.h"

#if EFI_TCU
Generic4TransmissionController generic4TransmissionController;
static SimplePwm pcPwm("Pressure Control");
static Map3D<TCU_PC_TABLE_SIZE, TCU_PC_TABLE_SIZE, uint8_t, uint16_t, uint8_t> pcTable{"pc"};

void Generic4TransmissionController::init() {
	for (size_t i = 0; i < efi::size(engineConfiguration->tcu_solenoid); i++) {
		enginePins.tcuSolenoids[i].initPin("Transmission Solenoid", engineConfiguration->tcu_solenoid[i], engineConfiguration->tcu_solenoid_mode[i]);
	}

	enginePins.tcuTccOnoffSolenoid.initPin("TCC On/Off Solenoid", engineConfiguration->tcu_tcc_onoff_solenoid, engineConfiguration->tcu_tcc_onoff_solenoid_mode);

	enginePins.tcuPcSolenoid.initPin("Pressure Control Solenoid", engineConfiguration->tcu_pc_solenoid_pin, engineConfiguration->tcu_pc_solenoid_pin_mode);
	startSimplePwm(&pcPwm,
								 "Line Pressure",
								 &engine->scheduler,
								 &enginePins.tcuPcSolenoid,
								 engineConfiguration->tcu_pc_solenoid_freq,
								 0);

	pcTable.initTable(config->tcu_pcTable, config->tcu_pcRpmBins, config->tcu_pcTpsBins);
}

void Generic4TransmissionController::update(gear_e gear) {
	if (gear != getCurrentGear()) {
		shiftingFrom = getCurrentGear();
		isShifting = true;
		measureShiftTime(gear);

		// A trim learned for the outgoing gear's clutch isn't meaningful for the next one's.
		m_pcSlipTrim = 0;
		m_pcSlipCleanTimer.reset();
	}

	// set torque converter and pressure control state
	updateTccLockup(gear);
	setPcState(gear);

	setCurrentGear(gear);

	updateShiftSolenoids(gear);

	float time = isShiftCompleted();
	// 0 means shift is not completed
	if (time != 0) {
		lastShiftTime = time;
		isShifting = false;
	}

	postState();
}

void Generic4TransmissionController::updateShiftSolenoids(gear_e gear) {
	for (size_t i = 0; i < efi::size(engineConfiguration->tcu_solenoid); i++) {
#if ! EFI_UNIT_TEST
		enginePins.tcuSolenoids[i].setValue(config->tcuSolenoidTable[i][static_cast<int>(gear) + 1]);
#endif
	}

	// Only solenoids 1 & 2 are exposed in the TS "Shift Solenoids" dialog today; publish
	// their commanded on/off state as gauges (see tcu_controller.txt).
	tcu_solenoid1On = config->tcuSolenoidTable[0][static_cast<int>(gear) + 1] != 0;
	tcu_solenoid2On = config->tcuSolenoidTable[1][static_cast<int>(gear) + 1] != 0;

#if EFI_TUNER_STUDIO
	if (engineConfiguration->debugMode == DBG_TCU) {
		engine->outputChannels.debugIntField1 = config->tcuSolenoidTable[static_cast<int>(gear) + 1][0];
		engine->outputChannels.debugIntField2 = config->tcuSolenoidTable[static_cast<int>(gear) + 1][1];
		engine->outputChannels.debugIntField3 = config->tcuSolenoidTable[static_cast<int>(gear) + 1][2];
		engine->outputChannels.debugIntField4 = config->tcuSolenoidTable[static_cast<int>(gear) + 1][3];
		engine->outputChannels.debugIntField5 = config->tcuSolenoidTable[static_cast<int>(gear) + 1][4];
	}
#endif
}

// Line pressure control: a 2D table of RPM x driver demand (TPS) gives a base duty, then four
// signed modifiers are added on top: a shift adder while a shift is in progress, the TCC lock-up
// adder, a per-gear adder indexed by desired gear, and the slip-based closed-loop trim (see
// updateSlipTrim()).
void Generic4TransmissionController::setPcState(gear_e desiredGear) {
	auto tps = Sensor::get(SensorType::DriverThrottleIntent);
	auto rpm = Sensor::get(SensorType::Rpm);
	if (!tps.Valid || !rpm.Valid) {
		return;
	}

	float targetDuty = pcTable.getValue(rpm.Value, tps.Value);

	if (isShifting) {
		targetDuty += config->tcu_pcShiftAdderDuty;
	}

	// adjust line pressure duty right after TCC lock-up engages, to help control converter
	// clutch apply shock -- signed, use whichever sign raises pressure on this EPC solenoid
	targetDuty += getPcLockupAdderDuty();

	// desiredGear is GEAR_1..GEAR_4 (1..4); anything else (Neutral/Reverse) gets no adder
	int gearIndex = static_cast<int>(desiredGear) - static_cast<int>(GEAR_1);
	if (gearIndex >= 0 && gearIndex < 4) {
		targetDuty += config->tcu_pcGearAdderDuty[gearIndex];
	}

	updateSlipTrim();
	targetDuty += m_pcSlipTrim;

	targetDuty = clampF(0, targetDuty, 100);

	// Slew the output toward targetDuty at tcu_pcRampTimeMs (time to cross the full 0-100% range)
	// instead of stepping instantly, regardless of which modifiers above contributed to the target
	// -- so table changes, shift transitions, the lock-up adder, and the gear adder are all
	// smoothed the same way. 0 disables the ramp (instant change, prior behavior). m_pcDutyRamped
	// is a float so slow ramp rates don't get lost to integer rounding every tick.
	float dtSeconds = m_pcRampTimer.getElapsedSeconds();
	m_pcRampTimer.reset();
	if (config->tcu_pcRampTimeMs == 0) {
		m_pcDutyRamped = targetDuty;
	} else {
		float maxStep = 100.0f * 1000.0f * dtSeconds / config->tcu_pcRampTimeMs;
		float delta = clampF(-maxStep, targetDuty - m_pcDutyRamped, maxStep);
		m_pcDutyRamped = clampF(0, m_pcDutyRamped + delta, 100);
	}

	pressureControlDuty = static_cast<int8_t>(m_pcDutyRamped + 0.5f);
	pcPwm.setSimplePwmDutyCycle(0.01f * m_pcDutyRamped);
}

// Closed-loop trim on top of the table+adders above, driven by Gear Setup's Slip Detection.
// There is no line pressure sensor, so this cannot be a PID against a setpoint -- it's a
// threshold-triggered accumulator: raise (fast, proportional to how far over) while slip exceeds
// Max Allowed Slip, decay (slow, fixed step) once slip has stayed clean for Decay Hold Time,
// otherwise hold. m_pcSlipTrim only ever moves in whichever sign tcu_pcSlipCorrectionGain is
// calibrated to (the direction that raises pressure on this EPC solenoid) and decay only ever
// pulls it back toward 0, never past -- so this can only ever push pressure higher than the base
// table+adders already call for, never lower. Reset to 0 on every commanded shift (see update())
// and every key-on; not persisted, not indexed by gear/RPM/TPS -- a single accumulator, like
// simple (non-region) short term fuel trim.
void Generic4TransmissionController::updateSlipTrim() {
	if (config->tcu_pcSlipCorrectionGain == 0) {
		// feature disabled (default)
		return;
	}

	if (isShifting) {
		// Slip during a shift is the shift itself, not a fault -- hold whatever the trim already
		// is (it was zeroed when this shift was commanded) until the shift completes.
		return;
	}

	auto gearDetector = engine->module<GearDetector>();
	if (!gearDetector->isSlipValid()) {
		// Can't tell right now (Slip Detection disabled, neutral, non-OSS speed source, below
		// transmissionSlipMinVss, or an invalid RPM source sensor) -- hold, don't accumulate or
		// decay on a reading we don't trust.
		tcu_pcSlipTrimDuty = static_cast<int8_t>(m_pcSlipTrim);
		return;
	}

	float excess = gearDetector->getSlipPercent() - config->tcu_pcSlipMaxAllowedPercent;

	if (excess > 0) {
		m_pcSlipCleanTimer.reset();

		float step = config->tcu_pcSlipCorrectionGain * excess;
		step = clampF(-config->tcu_pcSlipCorrectionStepMax, step, config->tcu_pcSlipCorrectionStepMax);
		m_pcSlipTrim += step;
	} else if (m_pcSlipCleanTimer.hasElapsedMs(config->tcu_pcSlipDecayHoldMs)) {
		float decayStep = config->tcu_pcSlipDecayStepDuty;
		if (m_pcSlipTrim > 0) {
			m_pcSlipTrim = maxF(0, m_pcSlipTrim - decayStep);
		} else if (m_pcSlipTrim < 0) {
			m_pcSlipTrim = minF(0, m_pcSlipTrim + decayStep);
		}
	}

	m_pcSlipTrim = clampF(-config->tcu_pcSlipTrimMaxDuty, m_pcSlipTrim, config->tcu_pcSlipTrimMaxDuty);

	tcu_pcSlipTrimDuty = static_cast<int8_t>(m_pcSlipTrim);
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

	// Pressure Control: RPM x driver demand (TPS) table of base duty. This default calibration
	// targets the 4R70W's inverted/normally-open EPC solenoid (duty falls as RPM/demand rise, so
	// pressure rises with RPM/demand) -- a board with a direct-acting solenoid would need this
	// table's gradient (and the adders below) entered with the opposite sign/slope.
	config->tcu_pcRpmBins[0] = 800;
	config->tcu_pcRpmBins[1] = 1500;
	config->tcu_pcRpmBins[2] = 2500;
	config->tcu_pcRpmBins[3] = 4000;
	config->tcu_pcRpmBins[4] = 6000;
	config->tcu_pcTpsBins[0] = 0;
	config->tcu_pcTpsBins[1] = 25;
	config->tcu_pcTpsBins[2] = 50;
	config->tcu_pcTpsBins[3] = 75;
	config->tcu_pcTpsBins[4] = 100;
	// rows = TPS bins (Y), columns = RPM bins (X)
	config->tcu_pcTable[0][0] = 70; config->tcu_pcTable[0][1] = 65; config->tcu_pcTable[0][2] = 60; config->tcu_pcTable[0][3] = 55; config->tcu_pcTable[0][4] = 50;
	config->tcu_pcTable[1][0] = 60; config->tcu_pcTable[1][1] = 55; config->tcu_pcTable[1][2] = 50; config->tcu_pcTable[1][3] = 45; config->tcu_pcTable[1][4] = 40;
	config->tcu_pcTable[2][0] = 50; config->tcu_pcTable[2][1] = 45; config->tcu_pcTable[2][2] = 40; config->tcu_pcTable[2][3] = 35; config->tcu_pcTable[2][4] = 30;
	config->tcu_pcTable[3][0] = 35; config->tcu_pcTable[3][1] = 30; config->tcu_pcTable[3][2] = 25; config->tcu_pcTable[3][3] = 20; config->tcu_pcTable[3][4] = 15;
	config->tcu_pcTable[4][0] = 20; config->tcu_pcTable[4][1] = 15; config->tcu_pcTable[4][2] = 10; config->tcu_pcTable[4][3] = 5;  config->tcu_pcTable[4][4] = 0;

	// Firm the line up while a shift is in progress.
	config->tcu_pcShiftAdderDuty = -15;

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
	config->tcu_shiftGearConfirmTime = 150.0;
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
