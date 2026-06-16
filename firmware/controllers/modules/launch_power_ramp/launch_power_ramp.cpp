#include "pch.h"
#include "custom_page.h"
#include "launch_power_ramp.h"

#if EFI_LAUNCH_POWER_RAMP

void LaunchPowerRamp::onSlowCallback() {
    auto cfg = getCustomPage();

    if (!cfg->launchPowerRampEnabled) {
        isLaunchPowerRampActive = false;
        launchPowerRampRetard = 0;
        launchPowerRampElapsed = 0;
        m_prevLaunchActive = false;
        return;
    }

    bool launchActive = engine->launchController.isLaunchCondition;
    bool isWot = Sensor::getOrZero(SensorType::DriverThrottleIntent) >= cfg->smWotTpsThreshold;

    // Start the ramp on the launch-release edge (leaving the line) while at WOT. This is
    // when the car actually launches; x=0 of the curve is this moment.
    if (m_prevLaunchActive && !launchActive && isWot) {
        m_rampTimer.reset();
        isLaunchPowerRampActive = true;
    }
    m_prevLaunchActive = launchActive;

    if (isLaunchPowerRampActive) {
        float elapsed = m_rampTimer.getElapsedSeconds();
        launchPowerRampElapsed = elapsed;

        // Pure time-based: once started the ramp runs to completion regardless of throttle.
        float lastBin = cfg->launchPowerRampTimeBins[LAUNCH_POWER_RAMP_CURVE_SIZE - 1];
        if (elapsed > lastBin) {
            isLaunchPowerRampActive = false;
            launchPowerRampRetard = 0;
        } else {
            float retard = interpolate2d(elapsed, cfg->launchPowerRampTimeBins, cfg->launchPowerRampRetardValues);
            // Retard only: never allow the curve to advance timing.
            launchPowerRampRetard = retard < 0 ? 0 : retard;
        }
    } else {
        launchPowerRampRetard = 0;
    }
}

float LaunchPowerRamp::getTimingRetard() const {
    return isLaunchPowerRampActive ? launchPowerRampRetard : 0;
}

#else // !EFI_LAUNCH_POWER_RAMP

// Launch Power Ramp compiled out (board set -DEFI_LAUNCH_POWER_RAMP=FALSE).
void LaunchPowerRamp::onSlowCallback() { isLaunchPowerRampActive = false; }
float LaunchPowerRamp::getTimingRetard() const { return 0; }

#endif // EFI_LAUNCH_POWER_RAMP
