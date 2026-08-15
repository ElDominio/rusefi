/**
 * @file fuel_pump.h
 */

#pragma once

#include "engine_module.h"
#include "fuel_pump_control_generated.h"
#include <rusefi/timer.h>

#if EFI_ADVANCED_FUEL_PUMP
#include "closed_loop_controller.h"
#include "efi_pid.h"

void initFuelPumpPwm();
#endif

class FuelPumpController : public EngineModule,
                           public fuel_pump_control_s
#if EFI_ADVANCED_FUEL_PUMP
                         , public ClosedLoopController<float, percent_t>
#endif
{
public:
#if EFI_ADVANCED_FUEL_PUMP
	FuelPumpController();
#endif

	void onSlowCallback() override;
	void onIgnitionStateChanged(bool ignitionOn) override;

#if EFI_ADVANCED_FUEL_PUMP
	void onFastCallback() override;
	void onConfigurationChange(engine_configuration_s const* prev) override;

	// ClosedLoopController interface — used in PWM mode
	expected<float>     getSetpoint() override;
	expected<float>     observePlant() override;
	expected<percent_t> getOpenLoop(float target) override;
	expected<percent_t> getClosedLoop(float setpoint, float observation) override;
	void                setOutput(expected<percent_t> output) override;
#endif

private:
	Timer m_ignOnTimer;

#if EFI_ADVANCED_FUEL_PUMP
	Pid   m_fuelPumpPid;
	bool  m_secondaryPumpOn = false;
	bool  m_reliefActive = false;

	void updateDualRelay();
#endif
};
