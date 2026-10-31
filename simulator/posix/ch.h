#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Reuse simulator chconf.h for timing constants
#include "chconf.h"

#ifdef CH_DBG_SYSTEM_STATE_CHECK
#undef CH_DBG_SYSTEM_STATE_CHECK
#endif
#define CH_DBG_SYSTEM_STATE_CHECK 0

#ifdef __cplusplus
extern "C" {
#endif

typedef intptr_t msg_t;
typedef uint32_t eventflags_t;
typedef uint32_t eventid_t;
typedef uint32_t eventmask_t;
typedef uint32_t systime_t;
typedef uint32_t sysinterval_t;
typedef uint32_t time_msecs_t;
typedef int32_t tprio_t;

typedef struct thread_t {
	void* impl;
	int threadId;
	const char* name;
	void* wabase;
	uint32_t time;
} thread_t;

typedef struct virtual_timer_t {
	void* impl;
} virtual_timer_t;

typedef void (*evhandler_t)(eventid_t);

#define MSG_OK 0
#define MSG_TIMEOUT -1
#define MSG_RESET -2

#ifndef TIME_INFINITE
#define TIME_INFINITE ((sysinterval_t)0xFFFFFFFFU)
#endif

#ifndef TIME_IMMEDIATE
#define TIME_IMMEDIATE ((sysinterval_t)0)
#endif

#ifndef CH_CFG_ST_FREQUENCY
#define CH_CFG_ST_FREQUENCY 100000
#endif

#define TIME_MS2I(ms) ((sysinterval_t)(((uint64_t)(ms) * CH_CFG_ST_FREQUENCY) / 1000U))
#define TIME_US2I(us) ((sysinterval_t)(((uint64_t)(us) * CH_CFG_ST_FREQUENCY) / 1000000U))
#define TIME_I2MS(t) ((time_msecs_t)(((uint64_t)(t) * 1000U) / CH_CFG_ST_FREQUENCY))

#define EVENT_MASK(eid) ((eventmask_t)1U << (eid))
#define ALL_EVENTS ((eventmask_t)0xFFFFFFFFU)

#define THD_FUNCTION(name, arg) void name(void *arg)
#define THD_WORKING_AREA_SIZE(x) (x)
#define THD_WORKING_AREA(name, size) uint8_t name[(size)]

#ifndef NORMALPRIO
#define NORMALPRIO 0
#endif
#ifndef LOWPRIO
#define LOWPRIO 0
#endif

// Event listener/source (minimal)
typedef struct event_listener_t {
	eventflags_t flags;
	eventid_t id;
	eventmask_t mask;
	struct event_source_t* source;
} event_listener_t;

typedef struct event_source_t {
	event_listener_t* listeners[8];
	size_t count;
} event_source_t;

// Virtual timer callback type
typedef void (*vtfunc_t)(virtual_timer_t*, void*);
typedef virtual_timer_t ch_virtual_timer;

// System control
void chSysInit(void);
void chSysLock(void);
void chSysUnlock(void);
void chSchRescheduleS(void);
#define chSysLockFromISR() chSysLock()
#define chSysUnlockFromISR() chSysUnlock()

// Threads
thread_t* chThdCreateFromHeap(void* heap, size_t size, const char* name, tprio_t prio, void (*func)(void*), void* arg);
thread_t* chThdCreateStatic(void* warea, size_t size, tprio_t prio, void (*func)(void*), void* arg);
bool chThdShouldTerminateX(void);
thread_t* chThdGetSelfX(void);
void chThdSleepMilliseconds(uint32_t ms);
void chThdSleepMicroseconds(uint32_t us);
static inline void chThdSleep(sysinterval_t ticks) {
	if (ticks != TIME_IMMEDIATE) {
		chThdSleepMilliseconds((ticks * 1000U) / CH_CFG_ST_FREQUENCY);
	}
}
void chThdRequestTerminate(thread_t* tp);
void chThdWait(thread_t* tp);
bool chThdTerminated(thread_t* tp);
void chRegSetThreadName(const char* name);
thread_t* chRegFirstThread(void);
thread_t* chRegNextThread(thread_t* tp);

// Messages
thread_t* chMsgWait(void);
msg_t chMsgGet(thread_t* tp);
void chMsgRelease(thread_t* tp, msg_t msg);
msg_t chMsgSend(thread_t* tp, msg_t msg);

// Events
void chEvtRegister(event_source_t* source, event_listener_t* el, eventid_t id);
void chEvtUnregister(event_source_t* source, event_listener_t* el);
eventflags_t chEvtGetAndClearFlags(event_listener_t* el);
eventflags_t chEvtGetAndClearFlagsI(event_listener_t* el);
eventmask_t chEvtWaitOne(eventmask_t mask);
eventmask_t chEvtWaitAny(eventmask_t mask);
eventmask_t chEvtWaitAll(eventmask_t mask);
eventmask_t chEvtWaitOneTimeout(eventmask_t mask, sysinterval_t timeout);
eventmask_t chEvtWaitAnyTimeout(eventmask_t mask, sysinterval_t timeout);
eventmask_t chEvtWaitAllTimeout(eventmask_t mask, sysinterval_t timeout);
eventmask_t chEvtGetAndClearEvents(eventmask_t mask);
eventmask_t chEvtAddEvents(eventmask_t mask);
void chEvtSignal(thread_t* tp, eventmask_t mask);
void chEvtSignalI(thread_t* tp, eventmask_t mask);
void chEvtBroadcastFlags(event_source_t* source, eventflags_t flags);
void chEvtBroadcastFlagsI(event_source_t* source, eventflags_t flags);
void chEvtDispatch(const evhandler_t* handlers, eventmask_t mask);

// Virtual timers
void chVTObjectInit(virtual_timer_t* vtp);
void chVTSetI(virtual_timer_t* vtp, sysinterval_t delay, vtfunc_t vtfunc, void* arg);
void chVTSetAny(virtual_timer_t* vtp, sysinterval_t delay, vtfunc_t vtfunc, void* arg);
void chVTResetI(virtual_timer_t* vtp);
// Cancel an unclaimed callback and transfer its argument back to its owner.
// nullptr means the timer service already claimed it (or it was not armed).
void* chVTResetAndTakeArgI(virtual_timer_t* vtp);
bool chVTIsArmedI(const virtual_timer_t* vtp);
systime_t chVTGetSystemTimeX(void);

// Common aliases used by firmware
static inline systime_t chVTGetSystemTime(void) { return chVTGetSystemTimeX(); }
static inline systime_t chTimeAddX(systime_t a, sysinterval_t b) { return a + b; }
systime_t chThdSleepUntilWindowed(systime_t prev, systime_t next);

// OSAL thread queues (minimal)
typedef struct threads_queue_t {
	void* impl;
} threads_queue_t;

void osalThreadQueueObjectInit(threads_queue_t* tq);
msg_t osalThreadEnqueueTimeoutS(threads_queue_t* tq, sysinterval_t timeout);
void osalThreadDequeueNextI(threads_queue_t* tq, msg_t msg);
void osalThreadDequeueAllI(threads_queue_t* tq, msg_t msg);

static inline void osalThreadSleep(sysinterval_t timeout) {
	if (timeout != TIME_IMMEDIATE) {
		chThdSleepMilliseconds((timeout * 1000U) / CH_CFG_ST_FREQUENCY);
	}
}
static inline void osalThreadSleepMilliseconds(uint32_t ms) {
	chThdSleepMilliseconds(ms);
}

// Minimal heap API
typedef struct memory_heap_t {
	uint8_t* buffer;
	size_t size;
} memory_heap_t;

#define CH_HEAP_AREA(name, size) static uint8_t name[(size)]

void chHeapObjectInit(memory_heap_t* hp, void* buffer, size_t size);
void* chHeapAlloc(memory_heap_t* hp, size_t size);
void chHeapFree(void* p);
void chHeapStatus(memory_heap_t* hp, size_t* totalp, size_t* largestp);
size_t chHeapGetSize(void* p);
size_t chCoreGetStatusX(void);

// rusEFI master uses these beyond EpicEFI's original shim surface.
#define CH_KERNEL_VERSION "posix-shim"
#ifndef PORT_IDLE_THREAD_STACK_SIZE
#define PORT_IDLE_THREAD_STACK_SIZE 0
#endif
static inline bool port_is_isr_context(void) { return false; }
static inline void osalOsRescheduleS(void) { }
static inline sysinterval_t chTimeDiffX(systime_t start, systime_t end) { return (sysinterval_t)(end - start); }
sysinterval_t chVTTimeElapsedSinceX(systime_t start);
// Ends the calling firmware thread (unwinds to the shim's thread entry).
void chThdExit(msg_t msg);

// Debug assert stub
#define chDbgAssert(...) do { } while (0)

#ifndef CH_DBG_STACK_FILL_VALUE
#define CH_DBG_STACK_FILL_VALUE 0x55
#endif

#ifndef SKIP_WIDEBAND
#define SKIP_WIDEBAND 0
#endif

#ifdef __cplusplus
}
#endif

#include "chmtx.h"
