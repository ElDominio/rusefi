/**
 * @file    alternator_controller.cpp
 * @brief   alternator controller - some newer vehicles control alternator with ECU
 *
 * @date Apr 6, 2014
 * @author Dmitry Sidin
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#include "pch.h"

#if EFI_ALTERNATOR_CONTROL
#include "alternator_controller.h"
#include "efi_pid.h"
#include "local_version_holder.h"
#include "periodic_task.h"
#include "engine_state_machine.h"
#include "custom_page.h"

#if defined(HAS_OS_ACCESS)
#error "Unexpected OS ACCESS HERE"
#endif /* HAS_OS_ACCESS */

static SimplePwm alternatorControl("alt");

AlternatorController::AlternatorController() {
	alternatorPid.initPidClass(&engineConfiguration->alternatorControl);
}

void AlternatorController::onFastCallback() {
	if (!isBrainPinValid(engineConfiguration->alternatorControlPin)) {
		return;
	}

	update();

#if EFI_TUNER_STUDIO
	// alternatorStatus is the trimmed alternator_pid_status_s (pTerm/iTerm/dTerm/output only), not
	// pid_status_s, so Pid::postState() can't write it directly - stage into a pid_status_s and
	// copy over just the terms we keep.
	pid_status_s pidStatus;
	alternatorPid.postState(pidStatus);
	auto& status = engine->outputChannels.alternatorStatus;
	status.pTerm = pidStatus.pTerm;
	status.iTerm = pidStatus.iTerm;
	status.dTerm = pidStatus.dTerm;
	// postState()'s own .output is the raw closed-loop PID term (pTerm+iTerm+dTerm+offset), which
	// getClosedLoop() partially un-does (subtracts offset) before summing with the open-loop base
	// duty. Use the actual duty just applied to the pin instead, so this gauge isn't misleading
	// about what the alternator is really doing.
	status.output = engine->outputChannels.alternatorOutputDuty;
#endif /* EFI_TUNER_STUDIO */
}

expected<float> AlternatorController::getSetpoint() {
	const float rpm = Sensor::getOrZero(SensorType::Rpm);
	bool alternatorShouldBeEnabledAtCurrentRpm = rpm > engineConfiguration->cranking.rpm;

	if (!engineConfiguration->isAlternatorControlEnabled || !alternatorShouldBeEnabledAtCurrentRpm) {
		return unexpected;
	}

	const float load = getEngineState()->fuelingLoad;
	float targetVoltage = interpolate3d(
		config->alternatorVoltageTargetTable,
		config->alternatorVoltageTargetLoadBins, load,
		config->alternatorVoltageTargetRpmBins, rpm
	);

	// Eco Mode: hold a lower absolute charging target to reduce alternator drag while the economy
	// overlay is active. A target of 0 disables the override (use the normal voltage target table).
	// Blends in/out over smSlowStateTransitionEnabled's ramp instead of snapping (see
	// getEcoModeBlend()).
	const float ecoTarget = getCustomPage()->ecoAlternatorVoltageTarget;
	if (ecoTarget > 0) {
		float ecoModeBlend = engine->module<EngineStateMachine>().unmock().getEcoModeBlend();
		if (ecoModeBlend > 0) {
			targetVoltage = interpolateClamped(0, targetVoltage, 1, ecoTarget, ecoModeBlend);
		}
	}

	engine->outputChannels.alternatorVoltageTarget = targetVoltage;
	return targetVoltage;
}

expected<float> AlternatorController::observePlant() {
	return Sensor::get(SensorType::BatteryVoltage);
}

expected<percent_t> AlternatorController::getOpenLoop(float target) {
	percent_t baseDuty;

	if (engineConfiguration->alternatorBaseDutyUseTable) {
		const float rpm = Sensor::getOrZero(SensorType::Rpm);
		baseDuty = interpolate3d(
			config->alternatorBaseDutyTable,
			config->alternatorBaseDutyVoltageBins, target,
			config->alternatorBaseDutyRpmBins, rpm
		);
	} else {
		// Single-value mode: the PID offset field serves as a fixed feedforward duty
		baseDuty = engineConfiguration->alternatorControl.offset;
	}

	engine->outputChannels.alternatorBaseDuty = baseDuty;

	// see "idle air Bump for AC" comment
	const percent_t acAdder = engine->module<AcController>().unmock().acButtonState
		? engineConfiguration->acRelayAlternatorDutyAdder : 0;

	return baseDuty + acAdder;
}

expected<percent_t> AlternatorController::getClosedLoop(float setpoint, float observation) {
	alternatorPid.iTermMin = engineConfiguration->alternator_iTermMin;
	alternatorPid.iTermMax = engineConfiguration->alternator_iTermMax;
	// The PID offset is owned by getOpenLoop; subtract it here so it doesn't double-feed
	// regardless of whether table or single-value mode is active.
	float pidOutput = alternatorPid.getOutput(setpoint, observation, FAST_CALLBACK_PERIOD_MS / 1000.0f);
	return pidOutput - engineConfiguration->alternatorControl.offset;
}

void AlternatorController::setOutput(expected<percent_t> outputValue) {
	if (outputValue) {
		// Open loop (base duty + AC adder) plus closed loop correction can add up negative,
		// e.g. base duty near zero while the PID pulls down hard to correct overvoltage.
		// Clamp here so the logged/gauge duty matches what's actually driven, instead of
		// relying on SimplePwm's internal clamp (which also spams a warning every cycle).
		percent_t clampedDuty = clampPercentValue(outputValue.Value);
		engine->outputChannels.alternatorOutputDuty = clampedDuty;
		alternatorControl.setSimplePwmDutyCycle(PERCENT_TO_DUTY(clampedDuty));
	} else {
		// Shut off output if not needed
		engine->outputChannels.alternatorOutputDuty = 0;
		alternatorPid.reset();
		alternatorControl.setSimplePwmDutyCycle(0);
	}
}

void AlternatorController::onConfigurationChange(engine_configuration_s const * previousConfiguration) {
	if(!alternatorPid.isSame(&previousConfiguration->alternatorControl)) {
		alternatorPid.reset();
	}
}

void initAlternatorCtrl() {
	if (!isBrainPinValid(engineConfiguration->alternatorControlPin))
		return;

	startSimplePwm(&alternatorControl,
				"Alternator control",
				&engine->scheduler,
				&enginePins.alternatorPin,
				engineConfiguration->alternatorPwmFrequency, 0);
}

#endif /* EFI_ALTERNATOR_CONTROL */
