/*
 * @file vvt.cpp
 *
 * @date Jun 26, 2016
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#include "pch.h"

#include "local_version_holder.h"
#include "vvt.h"
#include "bench_test.h"
#include "custom_page.h"
#include "engine_state_machine.h"

using vvt_map_t = Map3D<VVT_TABLE_RPM_SIZE, VVT_TABLE_SIZE, int8_t, uint16_t, uint16_t>;

// todo: rename to intakeVvtTable?
static vvt_map_t vvtTable1{"vvt1"};
static vvt_map_t vvtTable2{"vvt2"};

VvtController::VvtController(int p_index)
	: index(p_index)
	, m_bank(BANK_BY_INDEX(p_index))
	, m_cam(CAM_BY_INDEX(p_index))
{
}

void VvtController::init(const ValueProvider3D* targetMap, IPwm* pwm) {
	// Use the same settings for the Nth cam in every bank (ie, all exhaust cams use the same PID)
	m_pid.initPidClass(&engineConfiguration->auxPid[m_cam]);

	m_targetMap = targetMap;
	m_pwm = pwm;
	m_targetHysteresis = Hysteresis();
}

void VvtController::onFastCallback() {
	if (!m_pwm || !m_targetMap) {
		// not init yet
		return;
	}

	m_isRpmHighEnough = Sensor::getOrZero(SensorType::Rpm) > engineConfiguration->vvtControlMinRpm;
	m_isCltWarmEnough = Sensor::getOrZero(SensorType::Clt) > engineConfiguration->vvtControlMinClt;

	auto nowNt = getTimeNowNt();
	m_engineRunningLongEnough = engine->rpmCalculator.getSecondsSinceEngineStart(nowNt) > engineConfiguration->vvtActivationDelayMs / MS_PER_SECOND;

	update();
}

void VvtController::onConfigurationChange(engine_configuration_s const * previousConfig) {
	if (!previousConfig || !m_pid.isSame(&previousConfig->auxPid[m_cam])) {
		m_pid.reset();
	}
}

static bool shouldInvertVvt(int camIndex) {
	// grumble grumble, can't do an array of bits in c++
	switch (camIndex) {
		case 0: return engineConfiguration->invertVvtControlIntake;
		case 1: return engineConfiguration->invertVvtControlExhaust;
	}

	return false;
}

expected<angle_t> VvtController::observePlant() {
#if EFI_SHAFT_POSITION_INPUT
	return engine->triggerCentral.getVVTPosition(m_bank, m_cam);
#else
	return unexpected;
#endif // EFI_SHAFT_POSITION_INPUT
}

expected<angle_t> VvtController::getSetpoint() {
	float rpm = Sensor::getOrZero(SensorType::Rpm);
	bool enabled = m_engineRunningLongEnough &&
#if EFI_PROD_CODE || EFI_UNIT_TEST
// simulator functional test does not have CLT or flag?
                 		m_isCltWarmEnough &&
#endif
                 		m_isRpmHighEnough;
	if (!enabled) {
		return unexpected;
    }

	float load = getFuelingLoad();
	float target = m_targetMap->getValue(rpm, load);

	// Eco Mode: retarget the cams for economy while the overlay is active (intake = cam 0,
	// exhaust = cam 1). Opt-in so non-VVT and untuned setups are unaffected.
	if (getCustomPage()->ecoModeVvtOverride
			&& engine->module<EngineStateMachine>().unmock().engineSmIsEcoMode) {
		target = (m_cam == 0) ? getCustomPage()->ecoVvtIntakeTarget : getCustomPage()->ecoVvtExhaustTarget;
	}

#if EFI_GHOST_CAM
	// Ghost Cam: override VVT targets while active. Cam overlap is the primary lope mechanism.
	if (engine->module<EngineStateMachine>().unmock().engineSmIsGhostCam) {
		target = (m_cam == 0) ? getCustomPage()->ghostCamIntakeCamAngle : getCustomPage()->ghostCamExhaustCamAngle;
	}
#endif // EFI_GHOST_CAM

#if EFI_TUNER_STUDIO
	engine->outputChannels.vvtTargets[index] = target;
#endif

	// If the target is very near the rest position, disable control entirely
	// Couple reasons for this:
	// - Avoid integrator windup from trying to jam the cam against the stop
	// - Many VVT implementations don't like being controlled near the stop,
	//       as this can cause problems with the lock pin jamming.
	bool allowCamControl;
	if (shouldInvertVvt(m_cam)) {
		allowCamControl = m_targetHysteresis.test(target < -3, target > -1);
	} else {
		allowCamControl = m_targetHysteresis.test(target > 3, target < 1);
	}

	if (!allowCamControl) {
		return unexpected;
	}

	vvtTarget = target;

	return target;
}

expected<percent_t> VvtController::getOpenLoop(angle_t target) {
	// TODO: could we do VVT open loop?
	UNUSED(target);
	return 0;
}

#if EFI_VVT_ADVANCED_MODE
// VVT Advanced Mode: duty vs. signed distance-from-target, per cam type (0=intake, 1=exhaust).
// Only used beyond the PID fade distance -- see getClosedLoop.
static float getVvtAdvancedBaseDuty(int camIndex, float distance) {
	page6_s* cfg = getCustomPage();
	if (camIndex == 0) {
		return interpolate2d(distance, cfg->vvtAdvDistanceBinsIntake, cfg->vvtAdvDutyIntake);
	} else {
		return interpolate2d(distance, cfg->vvtAdvDistanceBinsExhaust, cfg->vvtAdvDutyExhaust);
	}
}

// VVT Advanced Mode: oil pressure multiplier on top of the base duty above. Neutral (1.0) if no
// oil pressure sensor is configured.
static float getVvtAdvancedOilPressureMult(int camIndex) {
	if (!Sensor::hasSensor(SensorType::OilPressure)) {
		return 1.0f;
	}

	float oilPressure = Sensor::getOrZero(SensorType::OilPressure);
	page6_s* cfg = getCustomPage();
	if (camIndex == 0) {
		return interpolate2d(oilPressure, cfg->vvtAdvOilPressureBinsIntake, cfg->vvtAdvOilPressureMultIntake);
	} else {
		return interpolate2d(oilPressure, cfg->vvtAdvOilPressureBinsExhaust, cfg->vvtAdvOilPressureMultExhaust);
	}
}
#endif // EFI_VVT_ADVANCED_MODE

expected<percent_t> VvtController::getClosedLoop(angle_t target, angle_t observation) {
	// User labels say "advance" and "retard"
	// "advance" means that additional solenoid duty makes indicated VVT position more positive
	// "retard" means that additional solenoid duty makes indicated VVT position more negative
	bool isInverted = shouldInvertVvt(m_cam);
	m_pid.setErrorAmplification(isInverted ? -1.0f : 1.0f);

	m_pid.iTermMin = (m_cam == 0) ? engineConfiguration->vvtIntake_iTermMin : engineConfiguration->vvtExhaust_iTermMin;
	m_pid.iTermMax = (m_cam == 0) ? engineConfiguration->vvtIntake_iTermMax : engineConfiguration->vvtExhaust_iTermMax;

#if EFI_VVT_ADVANCED_MODE
	if (getCustomPage()->vvtAdvancedModeEnabled) {
		// Signed distance uses the same sign convention as the PID error above, so curve tuning
		// doesn't depend on the solenoid inversion setting.
		float distance = (target - observation) * (isInverted ? -1.0f : 1.0f);
		vvtDistance = distance;

		// The duty curve, scaled by oil pressure, is always the feedforward baseline.
		float feedforward = getVvtAdvancedBaseDuty(m_cam, distance) * getVvtAdvancedOilPressureMult(m_cam);

		// Inside the pause window the PID is not evaluated at all -- its P/I/D state (iTerm in
		// particular) is frozen rather than integrating quietly in the background, so there's no
		// windup waiting to slam into the output once distance grows past the window. Outside the
		// window (or with pausing turned off) the PID runs at full authority as usual, and its raw
		// P+I+D trim (with the constant "Hold Duty" offset removed -- Hold Duty is never used in
		// Advanced Mode) is added on top of the feedforward.
		bool isPaused = getCustomPage()->vvtAdvancedPidPauseEnabled
			&& fabsf(distance) < maxF(getCustomPage()->vvtAdvancedPidPauseDeg, 0.0f);

		float output;
		if (isPaused) {
			output = feedforward;
		} else {
			float dTime = MS2SEC(GET_PERIOD_LIMITED(&engineConfiguration->auxPid[m_cam]));
			float rawPid = m_pid.getUnclampedOutput(target, observation, dTime) - m_pid.getOffset();
			output = feedforward + rawPid;
		}

		output = clampF(engineConfiguration->auxPid[m_cam].minValue, output, engineConfiguration->auxPid[m_cam].maxValue);

		// Keep the PID status telemetry reflecting the actual commanded duty.
		m_pid.output = output;

#if EFI_TUNER_STUDIO
		m_pid.postState(engine->outputChannels.vvtStatus[index]);
#endif /* EFI_TUNER_STUDIO */

		return output;
	}
#endif // EFI_VVT_ADVANCED_MODE

	float retVal = m_pid.getOutput(target, observation);

#if EFI_TUNER_STUDIO
	m_pid.postState(engine->outputChannels.vvtStatus[index]);
#endif /* EFI_TUNER_STUDIO */

	return retVal;
}

void VvtController::setOutput(expected<percent_t> outputValue) {
#if EFI_SHAFT_POSITION_INPUT
	vvtOutput = outputValue.value_or(0);

	if (outputValue) {
		m_pwm->setSimplePwmDutyCycle(PERCENT_TO_DUTY(outputValue.Value));
	} else {
		m_pwm->setSimplePwmDutyCycle(0);

		// we need to avoid accumulating iTerm while engine is not running
		m_pid.reset();
	}
#endif // EFI_SHAFT_POSITION_INPUT
}

#if EFI_VVT_PID

static const char *vvtOutputNames[CAM_INPUTS_COUNT] = {
"Vvt Output#1",
#if CAM_INPUTS_COUNT > 1
"Vvt Output#2",
#endif
#if CAM_INPUTS_COUNT > 2
"Vvt Output#3",
#endif
#if CAM_INPUTS_COUNT > 3
"Vvt Output#4",
#endif
 };

static OutputPin vvtPins[CAM_INPUTS_COUNT];
static SimplePwm vvtPwms[CAM_INPUTS_COUNT] = { "VVT1", "VVT2", "VVT3", "VVT4" };

OutputPin* getVvtOutputPin(int index) {
    return &vvtPins[index];
}

static void applyVvtPinState(int stateIndex, PwmConfig *state) /* pwm_gen_callback */ {
    OutputPin *output = state->outputPins[0];
    if (output == getOutputOnTheBenchTest()) {
        return;
    }
    state->applyPwmValue(output, stateIndex);
}

static void turnVvtPidOn(int index) {
	if (!isBrainPinValid(engineConfiguration->vvtPins[index])) {
		return;
	}

	startSimplePwmExt(&vvtPwms[index], vvtOutputNames[index],
			&engine->scheduler,
			engineConfiguration->vvtPins[index],
			getVvtOutputPin(index),
			engineConfiguration->vvtOutputFrequency, 0.1,
			applyVvtPinState);
}

void startVvtControlPins() {
	for (int i = 0;i <CAM_INPUTS_COUNT;i++) {
		turnVvtPidOn(i);
	}
}

void stopVvtControlPins() {
	for (int i = 0;i < CAM_INPUTS_COUNT;i++) {
		getVvtOutputPin(i)->deInit();
	}
}

void initVvtActuators() {

	vvtTable1.initTable(config->vvtTable1, config->vvtTable1RpmBins, config->vvtTable1LoadBins);
	vvtTable2.initTable(config->vvtTable2, config->vvtTable2RpmBins, config->vvtTable2LoadBins);


	engine->module<VvtController1>()->init(&vvtTable1, &vvtPwms[0]);
	engine->module<VvtController2>()->init(&vvtTable2, &vvtPwms[1]);
	engine->module<VvtController3>()->init(&vvtTable1, &vvtPwms[2]);
	engine->module<VvtController4>()->init(&vvtTable2, &vvtPwms[3]);

	startVvtControlPins();
}

#endif
