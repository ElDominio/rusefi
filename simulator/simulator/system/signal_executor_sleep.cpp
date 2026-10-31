/**
 * @file	signal_executor_sleep.cpp
 * @brief   Asynchronous output signal code
 *
 * Here we have the simplest, thread-based implementation of signal executor.
 *
 * @date Feb 10, 2013
 * @author Andrey Belomutskiy, (c) 2012-2020
 *
 * This file is part of rusEfi - see http://rusefi.com
 *
 * rusEfi is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * rusEfi is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "pch.h"

#if defined(EFI_SIMULATOR_POSIX)
// 64-bit POSIX simulator (simulator/posix shim), from EpicEFI epicefi_fw: timer reuse,
// cancellation that cannot fire a replaced action early, and simulation-clock deadlines.

#include "scheduler.h"
#include "main_trigger_callback.h"
#if defined(EFI_SIMULATOR_POSIX)
#include "simulator_event_time.h"
#include <unordered_map>
#include <vector>
#include "sim_clock.h"
#include <string>
#endif

#if EFI_PRINTF_FUEL_DETAILS
bool printSchedulerDebug = true;
#endif // EFI_PRINTF_FUEL_DETAILS

struct CallbackContext
{
	scheduling_s* scheduling = nullptr;
	bool shouldFree = false;
#if defined(EFI_SIMULATOR_POSIX)
	efitick_t deadline = 0;
	action_s action;
	std::string label;
#endif
};

#if defined(EFI_SIMULATOR_POSIX)
// Keep virtual_timer_t/TimerImpl ownership alive and reuse completed one-shot
// timers. Deleting scheduling_s leaked its registered TimerImpl into g_timers,
// making every timer scan progressively slower while the engine was running.
static std::vector<scheduling_s*> freeScheduling;
static std::unordered_map<scheduling_s*, CallbackContext*> activeCallbacks;
static std::string lastLateLabel;
static efitick_t lastLateRequested = 0, lastLateActual = 0;
void simExecutorLateSnapshot(char* label, size_t size, efitick_t& requested, efitick_t& actual) {
	chibios_rt::CriticalSectionLocker lock;
	std::snprintf(label, size, "%s", lastLateLabel.c_str());
	requested = lastLateRequested; actual = lastLateActual;
}
#endif

static void doScheduleForLater(const char* label, scheduling_s *scheduling, efitick_t deadline, int delayUs, action_s const& action);

// Lazy-init the ChibiOS virtual_timer_t embedded in scheduling_s.
// Static scheduling_s objects have the timer field filled with garbage
// (or ChibiOS 0xBE fill) because the field has no C++ default initializer.
static void ensureTimerInitialized(scheduling_s *s) {
	if (!s->timerInitialized) {
#if defined(EFI_SIMULATOR_POSIX)
		s->timer.impl = nullptr;
#endif
		chVTObjectInit(&s->timer);
		s->timerInitialized = true;
	}
}

void SleepExecutor::schedule(const char *msg, scheduling_s* scheduling, efitick_t timeNt, action_s const& action) {
	doScheduleForLater(msg, scheduling, timeNt, NT2US(timeNt) - getTimeNowUs(), action);
}

static void timerCallback(ch_virtual_timer *, CallbackContext* ctx) {
#if defined(EFI_SIMULATOR_POSIX)
	chibios_rt::CriticalSectionLocker csl;
	auto found = activeCallbacks.find(ctx->scheduling);
	if (found == activeCallbacks.end() || found->second != ctx) {
		// It was cancelled/replaced after the timer thread claimed the callback.
		// Do not execute a replacement action early through the old context.
		if (ctx->shouldFree) {
			if (found == activeCallbacks.end()) freeScheduling.push_back(ctx->scheduling);
			else found->second->shouldFree = true;
		}
		delete ctx;
		return;
	}
	activeCallbacks.erase(found);
#endif
#if EFI_PRINTF_FUEL_DETAILS
	if (printSchedulerDebug) {
		if (ctx->scheduling->action.getCallback() == (schfunc_t)&turnInjectionPinLow) {
			printf("executing cb=turnInjectionPinLow p=%d sch=%d now=%d\r\n", (int)ctx->scheduling->action.getArgument(), (int)scheduling,
				time2print(getTimeNowUs()));
		} else {
//		printf("exec cb=%d p=%d\r\n", (int)scheduling->callback, (int)scheduling->param);
		}
	}
#endif // EFI_PRINTF_FUEL_DETAILS

	// Grab the action but clear it in the event so we can reschedule from the action's execution
#if defined(EFI_SIMULATOR_POSIX)
	action_s action = ctx->action;
	const auto deadline = ctx->deadline;
	if (simClockEnabled() && deadline + MS2NT(50) < getTimeNowNt()) {
		lastLateLabel = ctx->label;
		lastLateRequested = deadline;
		lastLateActual = getTimeNowNt();
	}
#else
	action_s action = ctx->scheduling->action;
#endif
	ctx->scheduling->action = {};

	// Clean up any memory we allocated
	if (ctx->shouldFree) {
#if defined(EFI_SIMULATOR_POSIX)
		freeScheduling.push_back(ctx->scheduling);
#else
		delete ctx->scheduling;
#endif
	}
	delete ctx;

	// Lastly, actually execute the action
#if defined(EFI_SIMULATOR_POSIX)
	SimulatorEventTimeScope eventTime(simClockEnabled() ? getTimeNowNt() : deadline);
#endif
	action.execute();
}

static void doScheduleForLater(const char* label, scheduling_s *scheduling, efitick_t deadline, int delayUs, action_s const& action) {
	int delaySt = MY_US2ST(delayUs);
	if (delaySt <= 0) {
		/**
		 * in case of zero or negative delay, use a minimal async delay
		 * instead of executing synchronously to avoid re-entrant nesting
		 * (e.g. PWM timerCallback scheduling itself).
		 */
		delaySt = 1;
	}

	chibios_rt::CriticalSectionLocker csl;

  // ECU firmware does not use heap. this sleep-based executor is used only by simulator
	auto ctx = new CallbackContext;
	if (!scheduling) {
#if defined(EFI_SIMULATOR_POSIX)
		if (!freeScheduling.empty()) {
			scheduling = freeScheduling.back();
			freeScheduling.pop_back();
		} else
#endif
		{
			scheduling = new scheduling_s;
		}
		ctx->shouldFree = true;
	}
	ensureTimerInitialized(scheduling);
	// Match the native executor's scheduling record. Chained actions (HPFP
	// valve close, for example) use this moment as their absolute time origin.
	scheduling->setMomentNt(deadline);
	ctx->scheduling = scheduling;
#if defined(EFI_SIMULATOR_POSIX)
	ctx->action = action;
	ctx->deadline = deadline;
	ctx->label = label ? label : "";

	if (auto found = activeCallbacks.find(scheduling); found != activeCallbacks.end()) {
		ctx->shouldFree = ctx->shouldFree || found->second->shouldFree;
	}
	// The timer service owns a claimed context; it will observe the replacement
	// and dispose it. An unclaimed context transfers back to us on cancellation.
	delete static_cast<CallbackContext*>(chVTResetAndTakeArgI(&scheduling->timer));
	activeCallbacks[scheduling] = ctx;
#endif

	scheduling->action = action;
	int isArmed = chVTIsArmedI(&scheduling->timer);
	if (isArmed) {
		/**
		 * timer reuse is normal for example in case of sudden RPM increase
		 */
		chVTResetI(&scheduling->timer);
	}

#if defined(EFI_SIMULATOR_POSIX)
	if (simClockEnabled()) {
		// Preserve the native-tick deadline instead of rounding through the
		// ChibiOS 10-us virtual-timer interval or a Windows wake-up.
		chVTSetAbsoluteNtI(&scheduling->timer, deadline, (vtfunc_t)timerCallback, ctx);
		return;
	}
#endif
	chVTSetI(&scheduling->timer, delaySt, (vtfunc_t)timerCallback, ctx);
}

void SleepExecutor::cancel(scheduling_s* s) {
	chibios_rt::CriticalSectionLocker csl;

	// Lazy-initialize the timer if it has never been used.  Static
	// scheduling_s objects may contain garbage in the virtual_timer_t
	// because only the simulator build embeds one (EFI_SIMULATOR).
	ensureTimerInitialized(s);

#if defined(EFI_SIMULATOR_POSIX)
	activeCallbacks.erase(s);
	auto cancelled = static_cast<CallbackContext*>(chVTResetAndTakeArgI(&s->timer));
	if (cancelled && cancelled->shouldFree) freeScheduling.push_back(s);
	delete cancelled;
#endif

	if (chVTIsArmedI(&s->timer)) {
		chVTResetI(&s->timer);
	}

	s->action = {};
}

#else // the 32-bit ChibiOS simulator: rusEFI original

#include "scheduler.h"
#include "main_trigger_callback.h"

#if EFI_PRINTF_FUEL_DETAILS
bool printSchedulerDebug = true;
#endif // EFI_PRINTF_FUEL_DETAILS

struct CallbackContext
{
	scheduling_s* scheduling = nullptr;
	bool shouldFree = false;
};

static void doScheduleForLater(scheduling_s *scheduling, int delayUs, action_s const& action);

void SleepExecutor::schedule(const char * /*msg*/, scheduling_s* scheduling, efitick_t timeNt, action_s const& action) {
	doScheduleForLater(scheduling, NT2US(timeNt) - getTimeNowUs(), action);
}

static void timerCallback(ch_virtual_timer *, CallbackContext* ctx) {
#if EFI_PRINTF_FUEL_DETAILS
	if (printSchedulerDebug) {
		if (ctx->scheduling->action.getCallback() == (schfunc_t)&turnInjectionPinLow) {
			printf("executing cb=turnInjectionPinLow p=%d sch=%d now=%d\r\n", (int)ctx->scheduling->action.getArgument(), (int)scheduling,
				time2print(getTimeNowUs()));
		} else {
//		printf("exec cb=%d p=%d\r\n", (int)scheduling->callback, (int)scheduling->param);
		}
	}
#endif // EFI_PRINTF_FUEL_DETAILS

	// Grab the action but clear it in the event so we can reschedule from the action's execution
	action_s action = ctx->scheduling->action;
	ctx->scheduling->action = {};

	// Clean up any memory we allocated
	if (ctx->shouldFree) {
		delete ctx->scheduling;
	}
	delete ctx;

	// Lastly, actually execute the action
	action.execute();
}

static void doScheduleForLater(scheduling_s *scheduling, int delayUs, action_s const& action) {
	int delaySt = MY_US2ST(delayUs);
	if (delaySt <= 0) {
		/**
		 * in case of zero delay, we should invoke the callback
		 */
		action.execute();
		return;
	}

	chibios_rt::CriticalSectionLocker csl;

  // ECU firmware does not use heap. this sleep-based executor is used only by simulator
  // todo: probably move this executor to 'simulator' folder to make it official?
	auto ctx = new CallbackContext;
	if (!scheduling) {
		scheduling = new scheduling_s;
		chVTObjectInit(&scheduling->timer);
		ctx->shouldFree = true;
	}
	ctx->scheduling = scheduling;

	scheduling->action = action;
	int isArmed = chVTIsArmedI(&scheduling->timer);
	if (isArmed) {
		/**
		 * timer reuse is normal for example in case of sudden RPM increase
		 */
		chVTResetI(&scheduling->timer);
	}

	chVTSetI(&scheduling->timer, delaySt, (vtfunc_t)timerCallback, ctx);
}

void SleepExecutor::cancel(scheduling_s* s) {
	chibios_rt::CriticalSectionLocker csl;

	if (chVTIsArmedI(&s->timer)) {
		chVTResetI(&s->timer);
	}

	s->action = {};
}

#endif // EFI_SIMULATOR_POSIX
