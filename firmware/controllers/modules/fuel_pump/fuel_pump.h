/**
 * @file fuel_pump.h
 */

#pragma once

#include "engine_module.h"
#include "fuel_pump_control_generated.h"
#include "closed_loop_controller.h"
#include "efi_pid.h"
#include <rusefi/timer.h>

void initFuelPumpPwm();

class FuelPumpController : public EngineModule,
                           public fuel_pump_control_s,
                           public ClosedLoopController<float, percent_t> {
public:
	FuelPumpController();

	void onSlowCallback() override;
	void onFastCallback() override;
	void onIgnitionStateChanged(bool ignitionOn) override;
	void onConfigurationChange(engine_configuration_s const* prev) override;

	// ClosedLoopController interface — used in PWM mode
	expected<float>     getSetpoint() override;
	expected<float>     observePlant() override;
	expected<percent_t> getOpenLoop(float target) override;
	expected<percent_t> getClosedLoop(float setpoint, float observation) override;
	void                setOutput(expected<percent_t> output) override;

private:
	Timer m_ignOnTimer;
	Pid   m_fuelPumpPid;
	bool  m_secondaryPumpOn = false;

	void updateDualRelay();
};
