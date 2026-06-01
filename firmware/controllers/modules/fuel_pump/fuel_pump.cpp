/**
 * low pressure fuel pump control
 * for high-pressure see HpfpController@high_pressure_fuel_pump
 */
#include "pch.h"

#include "fuel_pump.h"
#include "bench_test.h"

#if EFI_ADVANCED_FUEL_PUMP
#if EFI_PROD_CODE || EFI_SIMULATOR
static SimplePwm fuelPumpPwm("fp_pwm");
#endif

FuelPumpController::FuelPumpController() {
	m_fuelPumpPid.initPidClass(&engineConfiguration->fuelPumpControl);
}
#endif // EFI_ADVANCED_FUEL_PUMP

void FuelPumpController::onSlowCallback() {
	auto timeSinceIgn = m_ignOnTimer.getElapsedSeconds();

	isPrime = timeSinceIgn >= 0 && timeSinceIgn < engineConfiguration->startUpFuelPumpDuration;

#if EFI_SHAFT_POSITION_INPUT
	engineTurnedRecently = engine->triggerCentral.engineMovedRecently();
#endif

	isFuelPumpOn = isPrime || engineTurnedRecently;

#if EFI_ADVANCED_FUEL_PUMP
	fuel_pump_mode_e mode = engineConfiguration->fuelPumpMode;

	if (mode == FP_MODE_SINGLE) {
		if (!isRunningBenchTest()) {
			enginePins.fuelPumpRelay.setValue("FP", isFuelPumpOn);
		}
	} else if (mode == FP_MODE_DUAL) {
		if (!isRunningBenchTest()) {
			enginePins.fuelPumpRelay.setValue("FP", isFuelPumpOn);
		}
		updateDualRelay();
	}
	// PWM mode: relay is not used; onFastCallback drives the duty via PID
#else
	if (!isRunningBenchTest()) {
		enginePins.fuelPumpRelay.setValue("FP", isFuelPumpOn);
	}
#endif // EFI_ADVANCED_FUEL_PUMP
}

#if EFI_ADVANCED_FUEL_PUMP
void FuelPumpController::updateDualRelay() {
	if (!isFuelPumpOn) {
		m_secondaryPumpOn = false;
		enginePins.fuelPumpRelay2.setValue("FP2", false);
		isSecondaryPumpOn = false;
		return;
	}

	const float rpm  = Sensor::getOrZero(SensorType::Rpm);
	const float load = getEngineState()->fuelingLoad;
	const float tps  = Sensor::getOrZero(SensorType::Tps1);

	if (!m_secondaryPumpOn) {
		// Activation: all three axes must exceed their activation threshold
		if (rpm  > engineConfiguration->secondaryFpActivationRpm  &&
		    load > engineConfiguration->secondaryFpActivationLoad &&
		    tps  > engineConfiguration->secondaryFpActivationTps) {
			m_secondaryPumpOn = true;
		}
	} else {
		// Deactivation: any single axis dropping below its deactivation threshold shuts off secondary
		if (rpm  < engineConfiguration->secondaryFpDeactivationRpm  ||
		    load < engineConfiguration->secondaryFpDeactivationLoad ||
		    tps  < engineConfiguration->secondaryFpDeactivationTps) {
			m_secondaryPumpOn = false;
		}
	}

	enginePins.fuelPumpRelay2.setValue("FP2", m_secondaryPumpOn);
	isSecondaryPumpOn = m_secondaryPumpOn;
}

void FuelPumpController::onFastCallback() {
	if (engineConfiguration->fuelPumpMode != FP_MODE_PWM) {
		return;
	}

#if EFI_PROD_CODE || EFI_SIMULATOR
	if (!isBrainPinValid(engineConfiguration->fuelPumpPin)) {
		return;
	}

	update();
#endif
}

expected<float> FuelPumpController::getSetpoint() {
	if (!isFuelPumpOn) {
		return unexpected;
	}

	const float rpm  = Sensor::getOrZero(SensorType::Rpm);
	const float load = getEngineState()->fuelingLoad;

	float target = interpolate3d(
		config->fuelPressureTargetTable,
		config->fuelPressureTargetLoadBins, load,
		config->fuelPressureTargetRpmBins,  rpm
	);

	fuelPressureTarget = static_cast<uint8_t>(clampF(0, target / 5, 255));

	return target;
}

expected<float> FuelPumpController::observePlant() {
	return Sensor::get(SensorType::FuelPressureLow);
}

expected<percent_t> FuelPumpController::getOpenLoop(float target) {
	const float rpm = Sensor::getOrZero(SensorType::Rpm);

	return interpolate3d(
		config->fuelPumpBaseDutyTable,
		config->fuelPumpBaseDutyFpBins,  target,
		config->fuelPumpBaseDutyRpmBins, rpm
	);
}

expected<percent_t> FuelPumpController::getClosedLoop(float setpoint, float observation) {
	isFpPidActive = true;
	m_fuelPumpPid.iTermMin = engineConfiguration->fuelPump_iTermMin;
	m_fuelPumpPid.iTermMax = engineConfiguration->fuelPump_iTermMax;
	return m_fuelPumpPid.getOutput(setpoint, observation, FAST_CALLBACK_PERIOD_MS / 1000.0f);
}

void FuelPumpController::setOutput(expected<percent_t> output) {
	percent_t duty;

	if (isPrime) {
		duty = engineConfiguration->fuelPumpMaxDuty;
		m_fuelPumpPid.reset();
	} else if (output) {
		duty = clampF(engineConfiguration->fuelPumpMinDuty,
		              output.Value,
		              engineConfiguration->fuelPumpMaxDuty);
	} else {
		duty = engineConfiguration->fuelPumpMinDuty;
		isFpPidActive = false;
		m_fuelPumpPid.reset();
	}

	fuelPumpDuty = static_cast<uint8_t>(duty);

#if EFI_PROD_CODE || EFI_SIMULATOR
	fuelPumpPwm.setSimplePwmDutyCycle(PERCENT_TO_DUTY(duty));
#endif
}

void FuelPumpController::onConfigurationChange(engine_configuration_s const* prev) {
	if (!m_fuelPumpPid.isSame(&prev->fuelPumpControl)) {
		m_fuelPumpPid.reset();
	}
}

void initFuelPumpPwm() {
#if EFI_PROD_CODE || EFI_SIMULATOR
	if (engineConfiguration->fuelPumpMode != FP_MODE_PWM) {
		return;
	}

	if (!isBrainPinValid(engineConfiguration->fuelPumpPin)) {
		return;
	}

	startSimplePwm(&fuelPumpPwm,
	               "Fuel pump PWM",
	               &engine->scheduler,
	               &enginePins.fuelPumpRelay,
	               engineConfiguration->fuelPumpPwmFrequency,
	               PERCENT_TO_DUTY(engineConfiguration->fuelPumpMinDuty));
#endif
}
#endif // EFI_ADVANCED_FUEL_PUMP

void FuelPumpController::onIgnitionStateChanged(bool ignitionOnParam) {
	ignitionOn = ignitionOnParam;
	if (ignitionOn) {
		m_ignOnTimer.reset();
	}
}
