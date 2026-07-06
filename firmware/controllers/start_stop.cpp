/**
 * @file start_stop.cpp
 * @brief Start/Stop button engine control.
 *
 * Implements the push-button start/stop behavior: on a button press it engages the
 * starter to crank the engine (and can stop a running engine), honoring the relevant
 * safety conditions.
 *
 * See test_start_stop.cpp
 */

#include "pch.h"

#include "start_stop.h"
#include "ignition_controller.h"

#if EFI_SHAFT_POSITION_INPUT

// How long a start-button press waits for a delayed Lua/security authorization
// before the request is abandoned. Covers CAN handshakes that grant cranking a
// moment after the button is pressed.
#define START_REQUEST_LATCH_SEC 1.0f
void initStartStopButton() {
	/* startCrankingDuration is efitimesec_t, so we need to multiply it by 1000 to get milliseconds*/
	engine->startStopState.startStopButtonDebounce.init((engineConfiguration->startCrankingDuration*1000),
	  engineConfiguration->startStopButtonPin,
	  engineConfiguration->startStopButtonMode,
	  engineConfiguration->startRequestPinInverted);
}

void doStartCranking() {
		bool wasStarterEngaged = enginePins.starterControl.getAndSet(1);
		if (!wasStarterEngaged) {
		    engine->startStopState.startStopStateLastPush.reset();
		    efiPrintf("Let's crank this engine for up to %d seconds via %s!",
		    		engineConfiguration->startCrankingDuration,
					hwPortname(engineConfiguration->starterControlPin));
		}
}

void startStopButtonToggle() {
	engine->engineState.startStopStateToggleCounter++;

	if (engine->rpmCalculator.isStopped()) {
	  doStartCranking();
	} else if (engine->rpmCalculator.isRunning()) {
		doScheduleStopEngine(StopRequestedReason::StartButton);
	}
}

static void disengageStarterIfNeeded() {
	if (engine->rpmCalculator.isRunning()) {
		// turn starter off once engine is now running!
		bool wasStarterEngaged = enginePins.starterControl.getAndSet(0);
		if (wasStarterEngaged) {
			efiPrintf("Engine runs we can disengage the starter");
		}
	} else {
    	if (engine->startStopState.startStopStateLastPush.hasElapsedSec(engineConfiguration->startCrankingDuration)) {
    		bool wasStarterEngaged = enginePins.starterControl.getAndSet(0);
    		if (wasStarterEngaged) {
    			efiPrintf("Cranking timeout %d seconds", engineConfiguration->startCrankingDuration);
    		}
    	}
    }
}

PUBLIC_API_WEAK bool isCrankingSuppressed() {
  return false;
}

// Returns true when a start-button press must not engage the starter.
// IMPORTANT: this only suppresses the cranking *output* - the button edge is
// still sampled and tracked by the caller so the firmware stays in sync with
// the physical button. We disable the starter, not the check.
static bool isStartCrankingBlocked() {
	if (!engine->rpmCalculator.isStopped()) {
		// engine is already running: a button press stops it, that is never blocked here
		return false;
	}
	if (engineConfiguration->crankingCondition == CC_BRAKE && !engine->brakePedalSwitchedState) {
		return true;
	}
	if (engineConfiguration->crankingCondition == CC_CLUTCH && !engine->clutchUpSwitchedState) {
		return true;
	}
	if (engineConfiguration->crankingCondition == CC_CLUTCH_DOWN && !engine->engineState.clutchDownState) {
		return true;
	}
	if (isCrankingSuppressed()) {
		return true;
	}
	if (engine->startStopState.startDisabledByLua) {
		return true;
	}
	return false;
}

void slowStartStopButtonCallback() {
  if (!isIgnVoltage()) {
    // nothing to crank if we are powered only via USB
    engine->startStopState.timeSinceIgnitionPower.reset();
    return;
  } else if (engine->startStopState.isFirstTime) {
    // initialize when first time with proper power
    engine->startStopState.timeSinceIgnitionPower.reset();
    engine->startStopState.isFirstTime = false;
  }

    if (engine->startStopState.timeSinceIgnitionPower.getElapsedUs() < MS2US(engineConfiguration->startButtonSuppressOnStartUpMs)) {
        // where are odd cases of start button combined with ECU power source button we do not want to crank right on start
        return;
    }

	// Always sample the button and track its edge - even when cranking is suppressed -
	// so engineState.startStopState stays in sync with the physical button. Only the
	// cranking action below is gated by isStartCrankingBlocked().
	bool startStopState = engine->startStopState.startStopButtonDebounce.readPinEvent();

	if (startStopState && !engine->engineState.startStopState) {
		// we are here on transition from 0 to 1
		if (!isStartCrankingBlocked()) {
			startStopButtonToggle();
		} else if (engine->rpmCalculator.isStopped() && engine->startStopState.startDisabledByLua) {
			// Valid start request, but Lua (e.g. a CAN security handshake) has not authorized
			// yet. The authorization often arrives a moment *after* the press, so remember the
			// request for a short window instead of dropping it.
			engine->startStopState.hasPendingStartRequest = true;
			engine->startStopState.pendingStartRequestTimer.reset();
		}
	}
	// todo: we shall extract start_stop.txt from engine_state.txt
	engine->engineState.startStopState = startStopState;
	engine->engineState.startStopPhysicalState = engine->startStopState.startStopButtonDebounce.getPhysicalState();

	// Service a pending (authorization-delayed) start request.
	if (engine->startStopState.hasPendingStartRequest) {
		if (!engine->rpmCalculator.isStopped()
				|| engine->startStopState.pendingStartRequestTimer.hasElapsedSec(START_REQUEST_LATCH_SEC)) {
			// engine already cranking/running, or the latch window expired - give up on this request
			engine->startStopState.hasPendingStartRequest = false;
		} else if (!isStartCrankingBlocked()) {
			// authorization arrived in time - honor the original press
			engine->startStopState.hasPendingStartRequest = false;
			doStartCranking();
		}
	}

    bool isStarterEngaged = enginePins.starterControl.getLogicValue();

	if (isStarterEngaged) {
	    disengageStarterIfNeeded();
   	}
}
#endif // EFI_SHAFT_POSITION_INPUT
