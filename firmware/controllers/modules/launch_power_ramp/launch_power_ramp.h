#pragma once

#include "launch_power_ramp_state_generated.h"
#include <rusefi/timer.h>

// Number of points in the time-vs-retard ramp curve. Must match the array sizes
// declared for launchPowerRampTimeBins / launchPowerRampRetardValues in config_page_5.txt.
#define LAUNCH_POWER_RAMP_CURVE_SIZE 8

class LaunchPowerRamp : public launch_power_ramp_state_s, public EngineModule {
public:
    using interface_t = LaunchPowerRamp;

    void onSlowCallback() override;

    // Ignition timing (degrees) currently being pulled by the ramp. 0 when inactive.
    float getTimingRetard() const;

private:
    Timer m_rampTimer;
    bool m_prevLaunchActive = false;
};
