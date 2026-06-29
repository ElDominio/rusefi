#include "pch.h"
#include "imrc_controller.h"
#include "custom_page.h"

#if EFI_IMRC

void ImrcController::deEnergize() {
	m_hbridgePwm1.setSimplePwmDutyCycle(0);
	m_hbridgePwm2.setSimplePwmDutyCycle(0);
}

void ImrcController::initPins() {
	m_solenoid1Pin.deInit();
	m_solenoid2Pin.deInit();
	m_hbridgePin1.deInit();
	m_hbridgePin2.deInit();

	auto& cfg = *getCustomPage();

	if (cfg.imrcMode == IMRC_DISABLED) {
		return;
	}

	if (cfg.imrcMode == IMRC_SOLENOID) {
		float freq = cfg.imrcSolenoidFrequency > 0 ? (float)cfg.imrcSolenoidFrequency : 100.0f;
		if (isBrainPinValid(cfg.imrcSolenoid1Pin)) {
			m_solenoid1Pin.initPin("imrc-sol1", cfg.imrcSolenoid1Pin, cfg.imrcSolenoid1PinMode);
			startSimplePwm(&m_solenoid1Pwm, "imrc-sol1", &engine->scheduler, &m_solenoid1Pin, freq, 0);
		}
		if (isBrainPinValid(cfg.imrcSolenoid2Pin)) {
			m_solenoid2Pin.initPin("imrc-sol2", cfg.imrcSolenoid2Pin, cfg.imrcSolenoid2PinMode);
			startSimplePwm(&m_solenoid2Pwm, "imrc-sol2", &engine->scheduler, &m_solenoid2Pin, freq, 0);
		}
		float closedDuty = PERCENT_TO_DUTY(cfg.imrcSolenoidClosedDuty);
		m_solenoid1Pwm.setSimplePwmDutyCycle(closedDuty);
		m_solenoid2Pwm.setSimplePwmDutyCycle(closedDuty);
	} else if (cfg.imrcMode == IMRC_HBRIDGE) {
		float freq = cfg.imrcHBridgeFrequency > 0 ? (float)cfg.imrcHBridgeFrequency : 1000.0f;
		if (isBrainPinValid(cfg.imrcHBridgePin1)) {
			m_hbridgePin1.initPin("imrc-hb1", cfg.imrcHBridgePin1, cfg.imrcHBridgePin1Mode);
			startSimplePwm(&m_hbridgePwm1, "imrc-hb1", &engine->scheduler, &m_hbridgePin1, freq, 0);
		}
		if (isBrainPinValid(cfg.imrcHBridgePin2)) {
			m_hbridgePin2.initPin("imrc-hb2", cfg.imrcHBridgePin2, cfg.imrcHBridgePin2Mode);
			startSimplePwm(&m_hbridgePwm2, "imrc-hb2", &engine->scheduler, &m_hbridgePin2, freq, 0);
		}
		deEnergize();
	}

	m_state = ImrcRunnerState::CLOSED;
}

void ImrcController::onConfigurationChange(engine_configuration_s const* /*previousConfig*/) {
	initPins();
}

bool ImrcController::evaluateTrigger() {
	auto& cfg = *getCustomPage();

	float rpm = Sensor::getOrZero(SensorType::Rpm);
	imrcTriggerRpm = (cfg.imrcRpmThreshold > 0) && (rpm > cfg.imrcRpmThreshold);

	float tps = Sensor::getOrZero(SensorType::Tps1);
	imrcTriggerTps = (cfg.imrcTpsThreshold > 0) && (tps > cfg.imrcTpsThreshold);

	float map = Sensor::getOrZero(SensorType::MapSlow);
	imrcTriggerMap = (cfg.imrcMapKpaThreshold > 0) && (map > cfg.imrcMapKpaThreshold);

	return imrcTriggerRpm || imrcTriggerTps || imrcTriggerMap;
}

void ImrcController::setState(ImrcRunnerState newState) {
	if (m_state == newState) {
		return;
	}

	m_state = newState;
	m_moveTimer.reset();

	auto& cfg = *getCustomPage();

	switch (newState) {
		case ImrcRunnerState::CLOSED:
			deEnergize();
			break;
		case ImrcRunnerState::OPENING:
			m_hbridgePwm2.setSimplePwmDutyCycle(0);
			m_hbridgePwm1.setSimplePwmDutyCycle(PERCENT_TO_DUTY(cfg.imrcHBridgeDutyCycle));
			break;
		case ImrcRunnerState::OPEN:
			deEnergize();
			break;
		case ImrcRunnerState::CLOSING:
			m_hbridgePwm1.setSimplePwmDutyCycle(0);
			m_hbridgePwm2.setSimplePwmDutyCycle(PERCENT_TO_DUTY(cfg.imrcHBridgeDutyCycle));
			break;
	}
}

void ImrcController::onSlowCallback() {
	auto& cfg = *getCustomPage();

	if (cfg.imrcMode == IMRC_DISABLED) {
		m_state = ImrcRunnerState::CLOSED;
		imrcTriggerRpm = false;
		imrcTriggerTps = false;
		imrcTriggerMap = false;
		imrcTriggered = false;
		imrcRunnersOpen = false;
		imrcMoving = false;
		return;
	}

	imrcTriggered = evaluateTrigger();
	bool desiredOpen = cfg.imrcInvert ? !imrcTriggered : imrcTriggered;

	if (cfg.imrcMode == IMRC_SOLENOID) {
		bool currentlyOpen = (m_state == ImrcRunnerState::OPEN);
		if (desiredOpen != currentlyOpen) {
			float duty = desiredOpen
				? PERCENT_TO_DUTY(cfg.imrcSolenoidOpenDuty)
				: PERCENT_TO_DUTY(cfg.imrcSolenoidClosedDuty);
			m_solenoid1Pwm.setSimplePwmDutyCycle(duty);
			m_solenoid2Pwm.setSimplePwmDutyCycle(duty);
			m_state = desiredOpen ? ImrcRunnerState::OPEN : ImrcRunnerState::CLOSED;
		}
	} else {
		// H-Bridge: advance OPENING→OPEN and CLOSING→CLOSED after move duration
		float moveDuration = cfg.imrcHBridgeMoveDurationS;
		bool moveComplete = m_moveTimer.hasElapsedSec(moveDuration);
		if (m_state == ImrcRunnerState::OPENING && moveComplete) {
			setState(ImrcRunnerState::OPEN);
		} else if (m_state == ImrcRunnerState::CLOSING && moveComplete) {
			setState(ImrcRunnerState::CLOSED);
		}

		if (desiredOpen) {
			if (m_state == ImrcRunnerState::CLOSED || m_state == ImrcRunnerState::CLOSING) {
				setState(ImrcRunnerState::OPENING);
			}
		} else {
			if (m_state == ImrcRunnerState::OPEN || m_state == ImrcRunnerState::OPENING) {
				setState(ImrcRunnerState::CLOSING);
			}
		}
	}

	imrcRunnersOpen = (m_state == ImrcRunnerState::OPEN || m_state == ImrcRunnerState::OPENING);
	imrcMoving = (m_state == ImrcRunnerState::OPENING || m_state == ImrcRunnerState::CLOSING);
}

void initImrc() {
	engine->module<ImrcController>()->initPins();
}

#else // !EFI_IMRC

void ImrcController::initPins() { }
void ImrcController::onConfigurationChange(engine_configuration_s const* /*previousConfig*/) { }
void ImrcController::onSlowCallback() { }
void initImrc() { }

#endif // EFI_IMRC
