/**
 * @file	framework.cpp
 *
 * @date Sep 25, 2014
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#include "pch.h"
#include "mpu_util.h"

#if defined(EFI_SIMULATOR_POSIX)
#include <atomic>
#include <chrono>
#include "sim_clock.h"
#endif

// Since all the time logic in the firmware is centered around this function, we only provide this
// function in the firmware.  It forces us to exercise the functions that build on this one.
uint32_t getTimeNowLowerNt(void) {
#if defined(EFI_SIMULATOR_POSIX)
	if (simClockEnabled()) return static_cast<uint32_t>(simClockNowNt());
	static std::atomic<uint64_t> startUs{0};
	uint64_t nowUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());

	uint64_t start = startUs.load(std::memory_order_relaxed);
	if (start == 0) {
		startUs.store(nowUs, std::memory_order_relaxed);
		start = nowUs;
	}

	uint64_t elapsedUs = nowUs - start;
	return static_cast<uint32_t>(US2NT(elapsedUs));
#else
	return US2NT(chVTGetSystemTimeX() * (1000000 / CH_CFG_ST_FREQUENCY));
#endif
}

#if HAL_USE_CAN
CANDriver* detectCanDevice(brain_pin_e pinRx, brain_pin_e pinTx);
#endif // HAL_USE_CAN

bool mcuCanFlashWhileRunning() { return true; }
//void causeHardFault() { }


Reset_Cause_t getMCUResetCause() {
	return Reset_Cause_Unknown;
}

const char *getMCUResetCause(Reset_Cause_t) {
	return "Unknown";
}


// we use 'int' for compatibility with addConsoleActionI()
// can be called multiple times to change the timeout
void startWatchdog(int) { }

// Can be called for debug reasons to test the watchdog
void setWatchdogResetPeriod(int /*resetMs*/) { }

// A reset is done only if enough time has passed since the last reset.
void tryResetWatchdog() { }
