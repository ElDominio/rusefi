#include "pch.h"
#include "custom_page.h"
#include "cdv_controller.h"

#if EFI_CLUTCH_DELAY_VALVE

void CdvController::onSlowCallback() {
    if (!engineConfiguration->cdvControlEnabled) {
        enginePins.cdvSolenoid.setValue(false);
        isCdvActive = false;
        m_prevLaunchActive = false;
        m_prevClutchDown = false;
        return;
    }

    bool launchActive = engine->launchController.isLaunchCondition
        || engine->launchController.isPreLaunchCondition;
    bool smartMode = engineConfiguration->cdvSmartMode;

    // Detect LC/pre-LC exit edge and start the exit timer (Simple mode's time-exit reference)
    if (m_prevLaunchActive && !launchActive) {
        m_sinceExitTimer.reset();
    }
    m_prevLaunchActive = launchActive;

    // Smart mode: track whether clutch pressure is currently inside the configured window
    float clutchPressure = Sensor::getOrZero(SensorType::ClutchPressure);
    isCdvPressureInWindow = clutchPressure >= getCustomPage()->cdvSmartMinPressure
        && clutchPressure <= getCustomPage()->cdvSmartMaxPressure;

    // Activate on launch or pre-launch entry; Smart mode additionally requires the clutch
    // pressure to already be inside the window. Keep active until an exit condition fires.
    if (!isCdvActive && launchActive && (!smartMode || isCdvPressureInWindow)) {
        isCdvActive = true;
        m_sinceActiveTimer.reset();
    }

    if (isCdvActive) {
        float exitDelay = getCustomPage()->cdvExitDelay;

        // Time-based exit: Simple mode fires after the configured delay once LC has exited.
        // Smart mode instead treats this as a max-active-duration safety timeout, counted
        // from when the CDV solenoid turned on (independent of launch/window state).
        isCdvTimeExitCondition = smartMode
            ? m_sinceActiveTimer.hasElapsedSec(exitDelay)
            : (!launchActive && m_sinceExitTimer.hasElapsedSec(exitDelay));

        // VSS-based exit: fires when vehicle speed exceeds threshold (0 = disabled)
        float exitVss = getCustomPage()->cdvExitVss;
        if (exitVss > 0) {
            isCdvVssExitCondition = Sensor::getOrZero(SensorType::VehicleSpeed) >= exitVss;
        } else {
            isCdvVssExitCondition = false;
        }

        // Clutch-based exit: fires on the falling edge of clutch-down (pedal released)
        bool clutchDown = engine->engineState.clutchDownState;
        if (engineConfiguration->cdvUseClutchExit) {
            isCdvClutchExitCondition = m_prevClutchDown && !clutchDown;
        } else {
            isCdvClutchExitCondition = false;
        }
        m_prevClutchDown = clutchDown;

        // Window-based exit (Smart mode only): clutch pressure has left the configured window
        isCdvWindowExitCondition = smartMode && !isCdvPressureInWindow;

        if (isCdvTimeExitCondition || isCdvVssExitCondition || isCdvClutchExitCondition || isCdvWindowExitCondition) {
            isCdvActive = false;
        }
    }

    enginePins.cdvSolenoid.setValue(isCdvActive);
}

#else // !EFI_CLUTCH_DELAY_VALVE

// Clutch Delay Valve compiled out (board set -DEFI_CLUTCH_DELAY_VALVE=FALSE).
void CdvController::onSlowCallback() { isCdvActive = false; }

#endif // EFI_CLUTCH_DELAY_VALVE
