#pragma once

#include "imrc_controller_generated.h"
#include <rusefi/timer.h>
#include "efi_output.h"
#include "pwm_generator_logic.h"

enum class ImrcRunnerState : uint8_t {
	CLOSED = 0,
	OPENING = 1,
	OPEN = 2,
	CLOSING = 3,
};

class ImrcController : public imrc_controller_s, public EngineModule {
public:
	void onSlowCallback() override;
	void onConfigurationChange(engine_configuration_s const* previousConfig) override;

	void initPins();

private:
	bool evaluateTrigger();
	void setState(ImrcRunnerState newState);
	void deEnergize();

	// Solenoid outputs (up to 2, driven identically)
	OutputPin m_solenoid1Pin;
	OutputPin m_solenoid2Pin;
	SimplePwm m_solenoid1Pwm{"imrc-sol1"};
	SimplePwm m_solenoid2Pwm{"imrc-sol2"};

	// H-Bridge outputs
	OutputPin m_hbridgePin1;
	OutputPin m_hbridgePin2;
	SimplePwm m_hbridgePwm1{"imrc-hb1"};
	SimplePwm m_hbridgePwm2{"imrc-hb2"};

	Timer m_moveTimer;

	ImrcRunnerState m_state = ImrcRunnerState::CLOSED;
};

void initImrc();
