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

    bool launchActive = engine->launchController.isLaunchCondition;

    // Detect LC exit edge and start the exit timer
    if (m_prevLaunchActive && !launchActive) {
        m_sinceExitTimer.reset();
    }
    m_prevLaunchActive = launchActive;

    // Activate on launch entry; keep active until an exit condition fires
    if (launchActive) {
        isCdvActive = true;
    }

    if (isCdvActive) {
        // Time-based exit: fires after configured delay once LC has exited
        float exitDelay = getCustomPage()->cdvExitDelay;
        isCdvTimeExitCondition = !launchActive && m_sinceExitTimer.hasElapsedSec(exitDelay);

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

        if (isCdvTimeExitCondition || isCdvVssExitCondition || isCdvClutchExitCondition) {
            isCdvActive = false;
        }
    }

    enginePins.cdvSolenoid.setValue(isCdvActive);
}

#else // !EFI_CLUTCH_DELAY_VALVE

// Clutch Delay Valve compiled out (board set -DEFI_CLUTCH_DELAY_VALVE=FALSE).
void CdvController::onSlowCallback() { isCdvActive = false; }

#endif // EFI_CLUTCH_DELAY_VALVE
