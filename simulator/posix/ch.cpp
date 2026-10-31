// POSIX ChibiOS shim implementation
#include "ch.h"
#include "sim_clock.h"
#include "sim_backtrace.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <queue>
#include <thread>
#include <time.h>
#include <vector>
#ifdef __APPLE__
#include <pthread/qos.h>
#endif
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

struct ThreadImpl {
	std::thread thr;
	std::mutex msgMutex;
	std::condition_variable msgCv;
	std::queue<msg_t> msgs;

	std::mutex evtMutex;
	std::condition_variable evtCv;
	std::atomic<eventmask_t> events{0};

	std::atomic<bool> terminate{false};
	std::atomic<bool> terminated{false};

	thread_t wrapper;
};

struct TimerImpl {
	std::atomic<bool> armed{false};
	sysinterval_t delay = 0;
	vtfunc_t cb = nullptr;
	void* arg = nullptr;
	virtual_timer_t* owner = nullptr;
	std::chrono::steady_clock::time_point deadline{};
	uint64_t virtualDeadlineNt = 0;
};

struct ThreadQueueImpl {
	std::mutex m;
	std::condition_variable cv;
	bool hasMsg = false;
	msg_t msg = MSG_OK;
};

static std::recursive_mutex g_sysLock;
static thread_local ThreadImpl* tls_thread = nullptr;
static ThreadImpl* g_mainThread = nullptr;

static std::mutex g_timerMutex;
static std::condition_variable g_timerCv;
static std::vector<TimerImpl*> g_timers;
static std::atomic<bool> g_timerThreadStarted{false};
static std::atomic<bool> g_virtualClock{false};
static std::atomic<uint64_t> g_virtualNowNt{0};
static uint32_t g_ticksPerUs = 1;
static SimClockStats g_clockStats;
static std::atomic<bool> g_halted{false};
static std::atomic<int> g_firmwareThreads{0};   // live chThdCreate* bodies
static std::atomic<int> g_parkedThreads{0};

void simHalt() {
	g_halted.store(true);
	g_timerCv.notify_all();
#ifdef _WIN32
	// The timer service may be blocked on its wake event; let it see the halt and exit.
	extern void winTimerWakeForHalt();
	winTimerWakeForHalt();
#endif
	// Let the running threads reach their park point, so nothing of this module is still
	// mid-work (a flash write, a socket call) when the host boots a fresh copy. Bounded:
	// a thread blocked in an infinite RTOS wait never parks, and is inert anyway.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
	while (g_parkedThreads.load() < g_firmwareThreads.load() && std::chrono::steady_clock::now() < deadline) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
bool simHalted() { return g_halted.load(std::memory_order_relaxed); }
int simHaltedThreads(int* parked) {
	if (parked) *parked = g_parkedThreads.load();
	return g_firmwareThreads.load();
}

// Firmware-owned threads only (chThdCreate* bodies): the host thread that booted the
// firmware (chSysInit's "main") and any other host caller keep running. Never inside a
// critical section: a thread that already holds g_sysLock (a nested chSysLock, an "S"
// API) parks at its next RTOS call after leaving it, so a parked thread holds no lock.
static thread_local bool tls_firmwareThread = false;
static thread_local int tls_sysLockDepth = 0;
static void parkIfHalted() {
	if (tls_firmwareThread && tls_sysLockDepth == 0 && g_halted.load(std::memory_order_relaxed)) {
		g_parkedThreads.fetch_add(1);
		for (;;) {
			std::this_thread::sleep_for(std::chrono::hours(1));
		}
	}
}

bool simClockEnabled() { return g_virtualClock.load(std::memory_order_acquire); }
uint64_t simClockNowNt() { return g_virtualNowNt.load(std::memory_order_relaxed); }
SimClockStats simClockStats() {
	std::lock_guard<std::recursive_mutex> lock(g_sysLock);
	return g_clockStats;
}
int simClockEnable(uint64_t startNt, uint32_t ticksPerUs) {
	std::lock_guard<std::recursive_mutex> sys(g_sysLock);
	std::lock_guard<std::mutex> timers(g_timerMutex);
	if (!ticksPerUs || simClockEnabled() || g_timerThreadStarted.load()) return -1;
	g_ticksPerUs = ticksPerUs;
	g_virtualNowNt.store(startNt);
	g_virtualClock.store(true, std::memory_order_release);
	return 0;
}
int simClockAdvanceTo(uint64_t targetNt) {
	if (simHalted()) return -1;
	std::lock_guard<std::recursive_mutex> sys(g_sysLock);
	if (!simClockEnabled() || targetNt < simClockNowNt()) return -1;
	++g_clockStats.advances;
	// Callbacks may re-arm themselves, replace other timers, or cancel timers.
	// Claim under the timer mutex, execute outside it, then reselect the earliest.
	for (unsigned work = 0; work < 100000; ++work) {
		TimerImpl* next = nullptr;
		vtfunc_t cb = nullptr;
		void* arg = nullptr;
		uint64_t deadline = 0;
		{
			std::lock_guard<std::mutex> timers(g_timerMutex);
			for (auto* timer : g_timers) {
				if (timer && timer->armed.load() && timer->virtualDeadlineNt <= targetNt
					&& (!next || timer->virtualDeadlineNt < next->virtualDeadlineNt)) next = timer;
			}
			if (next) {
				deadline = next->virtualDeadlineNt;
				next->armed.store(false);
				cb = next->cb; arg = next->arg;
			}
		}
		if (!next) {
			g_virtualNowNt.store(targetNt);
			return 0;
		}
		const uint64_t now = simClockNowNt();
		if (deadline < now) {
			++g_clockStats.lateCallbacks;
			g_clockStats.maxLatenessNt = std::max(g_clockStats.maxLatenessNt, now - deadline);
		} else g_virtualNowNt.store(deadline);
		++g_clockStats.callbacks;
		if (cb) cb(next->owner, arg);
	}
	++g_clockStats.budgetHits;
	return -2;
}

void chVTSetAbsoluteNtI(virtual_timer_t* vtp, uint64_t deadlineNt, vtfunc_t cb, void* arg) {
	chVTObjectInit(vtp);
	auto* timer = static_cast<TimerImpl*>(vtp->impl);
	std::lock_guard<std::mutex> lock(g_timerMutex);
	timer->virtualDeadlineNt = deadlineNt;
	timer->cb = cb; timer->arg = arg;
	timer->armed.store(true);
}

#ifdef _WIN32
/*
 * std::condition_variable timeouts on Windows follow the coarse system timer. A 60-2
 * wheel at 1200 RPM needs an edge about every 430 us, so a normal 15.6 ms wakeup makes
 * the simulated crank unusable. A waitable timer supplies the deadline, while the wake
 * event interrupts it whenever another thread arms a nearer virtual timer.
 */
static HANDLE g_winTimer;
static HANDLE g_winWake;

static void winTimerInit() {
	g_winTimer = CreateWaitableTimerExW(nullptr, nullptr,
		CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	if (!g_winTimer) {
		g_winTimer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
	}
	g_winWake = CreateEventW(nullptr, false, false, nullptr);
	timeBeginPeriod(1);
}

static void winTimerWake() {
	if (g_winWake) {
		SetEvent(g_winWake);
	}
}

void winTimerWakeForHalt() {
	winTimerWake();
}

static void winTimerWaitUntil(std::chrono::steady_clock::time_point deadline) {
	auto remaining = deadline - std::chrono::steady_clock::now();
	if (remaining <= std::chrono::steady_clock::duration::zero()) {
		return;
	}
	if (!g_winTimer || !g_winWake) {
		std::this_thread::sleep_for(std::chrono::microseconds(100));
		return;
	}

	// SetWaitableTimer uses 100 ns units; a negative value is relative to now.
	LARGE_INTEGER due{};
	due.QuadPart = -static_cast<LONGLONG>(
		std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count() / 100);
	if (due.QuadPart == 0) {
		due.QuadPart = -1;
	}
	if (!SetWaitableTimer(g_winTimer, &due, 0, nullptr, nullptr, false)) {
		std::this_thread::sleep_for(std::chrono::microseconds(100));
		return;
	}

	HANDLE handles[] = {g_winTimer, g_winWake};
	WaitForMultipleObjects(2, handles, false, INFINITE);
	CancelWaitableTimer(g_winTimer);
}
#endif

static bool anyTimerArmed() {
	for (auto* timer : g_timers) {
		if (timer && timer->armed.load(std::memory_order_relaxed)) {
			return true;
		}
	}
	return false;
}

static void timerServiceLoop() {
#ifdef __APPLE__
	// macOS demotes an ordinary thread's QoS and will throttle it; the timer thread is the
	// one place where being descheduled shows up directly as late fuel/spark events.
	pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
#ifdef _WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
	std::unique_lock<std::mutex> lk(g_timerMutex);
	for (;;) {
		if (simHalted()) {
			return;   // halted: no more callbacks into the firmware
		}
		TimerImpl* next = nullptr;
		auto nextDeadline = std::chrono::steady_clock::time_point::max();

		for (auto* timer : g_timers) {
			if (!timer || !timer->armed.load(std::memory_order_relaxed)) {
				continue;
			}
			if (timer->deadline < nextDeadline) {
				nextDeadline = timer->deadline;
				next = timer;
			}
		}

		if (!next) {
#ifdef _WIN32
			if (g_winWake) {
				lk.unlock();
				WaitForSingleObject(g_winWake, INFINITE);
				lk.lock();
			} else {
				g_timerCv.wait(lk, [] { return anyTimerArmed() || simHalted(); });
			}
#else
			g_timerCv.wait(lk, [] { return anyTimerArmed() || simHalted(); });
#endif
			continue;
		}

		/* NOTE: feat/simulator-obwb replaces this wait with "sleep to within 1ms of the
		 * deadline, then busy-spin". Measured here, that is a regression for THIS build: it
		 * pegs a core and takes the trigger-decode warning count over a 12s run from 3-4 to
		 * 21-39, because while spinning on one deadline the thread ignores a nearer timer
		 * armed in the meantime -- and the internal trigger emulator arms its next edge from
		 * inside the previous callback. That branch does not feel it because its trigger
		 * edges come from sim_trigger.cpp's own thread, not from this timer. Don't port it
		 * without re-measuring. */
#ifdef _WIN32
		lk.unlock();
		winTimerWaitUntil(nextDeadline);
		lk.lock();
#else
		g_timerCv.wait_until(lk, nextDeadline);
#endif

		auto now = std::chrono::steady_clock::now();
		if (!simHalted() && next->armed.load(std::memory_order_relaxed) && next->deadline <= now) {
			next->armed.store(false, std::memory_order_relaxed);
			auto cb = next->cb;
			auto arg = next->arg;
			auto owner = next->owner;
			lk.unlock();
			if (cb) {
				cb(owner, arg);
			}
			lk.lock();
		}
	}
}

static void ensureTimerService() {
	if (simClockEnabled()) return;
	bool expected = false;
	if (g_timerThreadStarted.compare_exchange_strong(expected, true)) {
#ifdef _WIN32
		winTimerInit();
#endif
		std::thread(timerServiceLoop).detach();
	}
}

static ThreadImpl* getCurrentThread() {
	return tls_thread;
}

static ThreadImpl* getImpl(thread_t* tp) {
	return tp ? static_cast<ThreadImpl*>(tp->impl) : nullptr;
}

static TimerImpl* getImpl(virtual_timer_t* vtp) {
	return vtp ? static_cast<TimerImpl*>(vtp->impl) : nullptr;
}

static void ensureMainThread() {
	if (!g_mainThread) {
		g_mainThread = new ThreadImpl();
		g_mainThread->wrapper.impl = g_mainThread;
		g_mainThread->wrapper.threadId = 0;
		g_mainThread->wrapper.name = "main";
		g_mainThread->wrapper.wabase = nullptr;
		g_mainThread->wrapper.time = 0;
		tls_thread = g_mainThread;
	}
}

 

namespace { struct ShimThreadExit {}; }

// Drops every level of g_sysLock this thread holds for a blocking wait, restores them after.
namespace {
struct SysLockReleaseForWait {
	int depth;
	SysLockReleaseForWait() : depth(tls_sysLockDepth) {
		tls_sysLockDepth = 0;
		for (int i = 0; i < depth; i++) g_sysLock.unlock();
	}
	~SysLockReleaseForWait() {
		for (int i = 0; i < depth; i++) g_sysLock.lock();
		tls_sysLockDepth = depth;
	}
	SysLockReleaseForWait(const SysLockReleaseForWait&) = delete;
	SysLockReleaseForWait& operator=(const SysLockReleaseForWait&) = delete;
};
}

extern "C" {

void chThdExit(msg_t) {
	throw ShimThreadExit{};
}

sysinterval_t chVTTimeElapsedSinceX(systime_t start) {
	return (sysinterval_t)(chVTGetSystemTimeX() - start);
}

void chMtxObjectInit(mutex_t* mp) {
	mp->impl = new std::recursive_mutex();
}

static std::recursive_mutex* mtxImpl(mutex_t* mp) {
	// A zero-initialized global mutex_t that never saw chMtxObjectInit still works.
	static std::mutex initGuard;
	std::lock_guard<std::mutex> guard(initGuard);
	if (!mp->impl) mp->impl = new std::recursive_mutex();
	return static_cast<std::recursive_mutex*>(mp->impl);
}

void chMtxLock(mutex_t* mp) {
	mtxImpl(mp)->lock();
}

void chMtxUnlock(mutex_t* mp) {
	mtxImpl(mp)->unlock();
}

bool chMtxTryLock(mutex_t* mp) {
	return mtxImpl(mp)->try_lock();
}

void chSysInit(void) {
	ensureMainThread();
}

// RUSEFI_SIM_LOCK_DEBUG=1: report critical sections held > 5 ms (with the holder's stack).
static bool lockDebugEnabled() {
	static const bool enabled = std::getenv("RUSEFI_SIM_LOCK_DEBUG") != nullptr;
	return enabled;
}
static thread_local std::chrono::steady_clock::time_point tls_lockTakenAt;

void chSysLock(void) {
	parkIfHalted();
	g_sysLock.lock();
	if (++tls_sysLockDepth == 1 && lockDebugEnabled()) {
		tls_lockTakenAt = std::chrono::steady_clock::now();
	}
}

void chSysUnlock(void) {
	if (tls_sysLockDepth == 1 && lockDebugEnabled()) {
		const auto held = std::chrono::steady_clock::now() - tls_lockTakenAt;
		const auto heldUs = std::chrono::duration_cast<std::chrono::microseconds>(held).count();
		static std::atomic<int> reports{0};
		if (heldUs > 5000 && reports.fetch_add(1) < 20) {
			fprintf(stderr, "[sim-lock] critical section held %lld us\n", (long long)heldUs);
			simPrintBacktrace("[sim-lock] holder stack:\n");
		}
	}
	--tls_sysLockDepth;
	g_sysLock.unlock();
}

void chSchRescheduleS(void) {
	// no-op for POSIX shim
}

thread_t* chThdCreateFromHeap(void*, size_t, const char*, tprio_t, void (*func)(void*), void* arg) {
	auto impl = new ThreadImpl();
	impl->wrapper.impl = impl;
	impl->wrapper.threadId = 0;
	impl->wrapper.name = "thread";
	impl->wrapper.wabase = nullptr;
	impl->wrapper.time = 0;

	impl->thr = std::thread([impl, func, arg]() {
		tls_thread = impl;
		tls_firmwareThread = true;
		g_firmwareThreads.fetch_add(1);
#ifdef __cplusplus
		try {
			func(arg);
		} catch (const ShimThreadExit&) {
			// chThdExit()
		}
#else
		func(arg);
#endif
		g_firmwareThreads.fetch_sub(1);
		impl->terminated = true;
	});
	return &impl->wrapper;
}

thread_t* chThdCreateStatic(void* warea, size_t size, tprio_t prio, void (*func)(void*), void* arg) {
	(void)warea;
	return chThdCreateFromHeap(nullptr, size, "static", prio, func, arg);
}

bool chThdShouldTerminateX(void) {
	auto t = getCurrentThread();
	return t ? t->terminate.load() : false;
}

thread_t* chThdGetSelfX(void) {
	auto t = getCurrentThread();
	return t ? &t->wrapper : nullptr;
}

void chThdSleepMilliseconds(uint32_t ms) {
	parkIfHalted();
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	parkIfHalted();
}

void chThdSleepMicroseconds(uint32_t us) {
	parkIfHalted();
	std::this_thread::sleep_for(std::chrono::microseconds(us));
	parkIfHalted();
}

systime_t chThdSleepUntilWindowed(systime_t /*prev*/, systime_t next) {
	systime_t now = chVTGetSystemTimeX();
	if (next > now) {
		sysinterval_t diff = next - now;
		uint64_t us = (static_cast<uint64_t>(diff) * 1000000ULL) / CH_CFG_ST_FREQUENCY;
		chThdSleepMicroseconds(static_cast<uint32_t>(us));
	}
	return next;
}

void chThdRequestTerminate(thread_t* tp) {
	auto t = getImpl(tp);
	if (!t) {
		return;
	}
	t->terminate = true;
}

void chThdWait(thread_t* tp) {
	auto t = getImpl(tp);
	if (!t) {
		return;
	}
	auto& thr = t->thr;
	if (thr.joinable()) {
		thr.join();
	}
}

bool chThdTerminated(thread_t* tp) {
	auto t = getImpl(tp);
	if (!t) {
		return true;
	}
	return t->terminated.load();
}

void chRegSetThreadName(const char*) {
	// no-op
}

thread_t* chRegFirstThread(void) {
	return nullptr;
}

thread_t* chRegNextThread(thread_t*) {
	return nullptr;
}

thread_t* chMsgWait(void) {
	parkIfHalted();
	auto t = getCurrentThread();
	if (!t) {
		return nullptr;
	}
	std::unique_lock<std::mutex> lk(t->msgMutex);
	t->msgCv.wait(lk, [&] { return !t->msgs.empty(); });
	return &t->wrapper;
}

msg_t chMsgGet(thread_t* tp) {
	auto t = getImpl(tp);
	if (!t) {
		return 0;
	}
	std::lock_guard<std::mutex> lk(t->msgMutex);
	if (t->msgs.empty()) {
		return 0;
	}
	auto msg = t->msgs.front();
	t->msgs.pop();
	return msg;
}

void chMsgRelease(thread_t*, msg_t) {
	// no-op
}

msg_t chMsgSend(thread_t* tp, msg_t msg) {
	auto t = getImpl(tp);
	if (!t) {
		return MSG_TIMEOUT;
	}
	{
		std::lock_guard<std::mutex> lk(t->msgMutex);
		t->msgs.push(msg);
	}
	t->msgCv.notify_one();
	return MSG_OK;
}

void chEvtRegister(event_source_t* source, event_listener_t* el, eventid_t id) {
	if (!source || !el) {
		return;
	}
	el->id = id;
	el->mask = EVENT_MASK(id);
	el->source = source;
	el->flags = 0;
	if (source->count < 8) {
		source->listeners[source->count++] = el;
	}
}

void chEvtUnregister(event_source_t* source, event_listener_t* el) {
	if (!source || !el) {
		return;
	}
	for (size_t i = 0; i < source->count; i++) {
		if (source->listeners[i] == el) {
			for (size_t j = i + 1; j < source->count; j++) {
				source->listeners[j - 1] = source->listeners[j];
			}
			source->count--;
			break;
		}
	}
}

eventflags_t chEvtGetAndClearFlags(event_listener_t* el) {
	if (!el) {
		return 0;
	}
	auto f = el->flags;
	el->flags = 0;
	return f;
}

eventflags_t chEvtGetAndClearFlagsI(event_listener_t* el) {
	return chEvtGetAndClearFlags(el);
}

static eventmask_t waitMask(eventmask_t mask, sysinterval_t timeout) {
	parkIfHalted();
	auto t = getCurrentThread();
	if (!t) {
		return 0;
	}
	std::unique_lock<std::mutex> lk(t->evtMutex);

	auto pred = [&] { return (t->events.load() & mask) != 0; };

	if (timeout == TIME_IMMEDIATE) {
		if (!pred()) {
			return 0;
		}
	} else if (timeout == TIME_INFINITE) {
		t->evtCv.wait(lk, pred);
	} else {
		auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(
			(static_cast<uint64_t>(timeout) * 1000ULL) / CH_CFG_ST_FREQUENCY
		);
		if (!t->evtCv.wait_until(lk, deadline, pred)) {
			return 0;
		}
	}

	auto result = t->events.load() & mask;
	t->events.store(t->events.load() & ~mask);
	return result;
}

eventmask_t chEvtWaitOne(eventmask_t mask) { return waitMask(mask, TIME_INFINITE); }
eventmask_t chEvtWaitAny(eventmask_t mask) { return waitMask(mask, TIME_INFINITE); }
eventmask_t chEvtWaitAll(eventmask_t mask) { return waitMask(mask, TIME_INFINITE); }

eventmask_t chEvtWaitOneTimeout(eventmask_t mask, sysinterval_t timeout) { return waitMask(mask, timeout); }
eventmask_t chEvtWaitAnyTimeout(eventmask_t mask, sysinterval_t timeout) { return waitMask(mask, timeout); }
eventmask_t chEvtWaitAllTimeout(eventmask_t mask, sysinterval_t timeout) { return waitMask(mask, timeout); }

eventmask_t chEvtGetAndClearEvents(eventmask_t mask) {
	auto t = getCurrentThread();
	if (!t) {
		return 0;
	}
	auto result = t->events.load() & mask;
	t->events.store(t->events.load() & ~mask);
	return result;
}

eventmask_t chEvtAddEvents(eventmask_t mask) {
	auto t = getCurrentThread();
	if (!t) {
		return 0;
	}
	t->events.store(t->events.load() | mask);
	return t->events.load();
}

void chEvtSignal(thread_t* tp, eventmask_t mask) {
	auto t = getImpl(tp);
	if (!t) {
		return;
	}
	{
		std::lock_guard<std::mutex> lk(t->evtMutex);
		t->events.store(t->events.load() | mask);
	}
	t->evtCv.notify_all();
}

void chEvtSignalI(thread_t* tp, eventmask_t mask) {
	chEvtSignal(tp, mask);
}

void chEvtBroadcastFlags(event_source_t* source, eventflags_t flags) {
	if (!source) {
		return;
	}
	for (size_t i = 0; i < source->count; i++) {
		source->listeners[i]->flags |= flags;
	}
}

void chEvtBroadcastFlagsI(event_source_t* source, eventflags_t flags) {
	chEvtBroadcastFlags(source, flags);
}

void chEvtDispatch(const evhandler_t* handlers, eventmask_t mask) {
	for (uint32_t i = 0; mask != 0; i++, mask >>= 1) {
		if ((mask & 1U) && handlers[i]) {
			handlers[i](i);
		}
	}
}

void osalThreadQueueObjectInit(threads_queue_t* tq) {
	if (!tq) {
		return;
	}
	if (!tq->impl) {
		tq->impl = new ThreadQueueImpl();
	}
}

msg_t osalThreadEnqueueTimeoutS(threads_queue_t* tq, sysinterval_t timeout) {
	parkIfHalted();
	if (!tq || !tq->impl) {
		return MSG_TIMEOUT;
	}
	auto* impl = static_cast<ThreadQueueImpl*>(tq->impl);
	// An "S" API: called inside chSysLock(). Real ChibiOS releases the kernel lock while the
	// thread sleeps; holding g_sysLock across this wait stalled every other thread (and the
	// in-process host's ecu_commit) for the whole timeout - rusEFI's CAN TX service waits here.
	// Declared before lk so it re-takes the kernel lock only after impl->m is released.
	SysLockReleaseForWait release;
	std::unique_lock<std::mutex> lk(impl->m);

	if (timeout == TIME_IMMEDIATE) {
		return MSG_TIMEOUT;
	}

	if (timeout == TIME_INFINITE) {
		impl->cv.wait(lk, [&] { return impl->hasMsg; });
	} else {
		auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(
			(static_cast<uint64_t>(timeout) * 1000ULL) / CH_CFG_ST_FREQUENCY
		);
		if (!impl->cv.wait_until(lk, deadline, [&] { return impl->hasMsg; })) {
			return MSG_TIMEOUT;
		}
	}

	impl->hasMsg = false;
	return impl->msg;
}

void osalThreadDequeueNextI(threads_queue_t* tq, msg_t msg) {
	if (!tq || !tq->impl) {
		return;
	}
	auto* impl = static_cast<ThreadQueueImpl*>(tq->impl);
	{
		std::lock_guard<std::mutex> lk(impl->m);
		impl->msg = msg;
		impl->hasMsg = true;
	}
	impl->cv.notify_one();
}

void osalThreadDequeueAllI(threads_queue_t* tq, msg_t msg) {
	if (!tq || !tq->impl) {
		return;
	}
	auto* impl = static_cast<ThreadQueueImpl*>(tq->impl);
	{
		std::lock_guard<std::mutex> lk(impl->m);
		impl->msg = msg;
		impl->hasMsg = true;
	}
	impl->cv.notify_all();
}

void chVTObjectInit(virtual_timer_t* vtp) {
	if (!vtp) {
		return;
	}
	if (!vtp->impl) {
		auto impl = new TimerImpl();
		impl->owner = vtp;
		vtp->impl = impl;
		{
			std::lock_guard<std::mutex> lk(g_timerMutex);
			g_timers.push_back(impl);
		}
		ensureTimerService();
	}
}

void chVTSetI(virtual_timer_t* vtp, sysinterval_t delay, vtfunc_t vtfunc, void* arg) {
	chVTSetAny(vtp, delay, vtfunc, arg);
}

void chVTSetAny(virtual_timer_t* vtp, sysinterval_t delay, vtfunc_t vtfunc, void* arg) {
	if (!vtp) {
		return;
	}
	if (simClockEnabled()) {
		const uint64_t ticks = (static_cast<uint64_t>(delay) * 1000000ULL * g_ticksPerUs) / CH_CFG_ST_FREQUENCY;
		chVTSetAbsoluteNtI(vtp, simClockNowNt() + ticks, vtfunc, arg);
		return;
	}
	chVTObjectInit(vtp);
	auto impl = getImpl(vtp);
	if (!impl) {
		return;
	}
	auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(
		(static_cast<uint64_t>(delay) * 1000000ULL) / CH_CFG_ST_FREQUENCY
	);
	{
		std::lock_guard<std::mutex> lk(g_timerMutex);
		impl->delay = delay;
		impl->cb = vtfunc;
		impl->arg = arg;
		impl->deadline = deadline;
		impl->armed.store(true, std::memory_order_relaxed);
	}
	g_timerCv.notify_all();
#ifdef _WIN32
	winTimerWake();
#endif
}

void* chVTResetAndTakeArgI(virtual_timer_t* vtp) {
	auto impl = getImpl(vtp);
	if (!impl) {
		return nullptr;
	}
	void* cancelledArg = nullptr;
	{
		std::lock_guard<std::mutex> lk(g_timerMutex);
		if (impl->armed.load(std::memory_order_relaxed)) cancelledArg = impl->arg;
		impl->armed.store(false, std::memory_order_relaxed);
	}
	g_timerCv.notify_all();
#ifdef _WIN32
	winTimerWake();
#endif
	return cancelledArg;
}

void chVTResetI(virtual_timer_t* vtp) {
	(void)chVTResetAndTakeArgI(vtp);
}

bool chVTIsArmedI(const virtual_timer_t* vtp) {
	if (!vtp || !vtp->impl) {
		return false;
	}
	return static_cast<TimerImpl*>(vtp->impl)->armed.load(std::memory_order_relaxed);
}

systime_t chVTGetSystemTimeX(void) {
	if (simClockEnabled()) {
		return static_cast<systime_t>(simClockNowNt() * CH_CFG_ST_FREQUENCY / (1000000ULL * g_ticksPerUs));
	}
	uint64_t us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
	return (systime_t)((us * CH_CFG_ST_FREQUENCY) / 1000000ULL);
}

void chHeapObjectInit(memory_heap_t* hp, void* buffer, size_t size) {
	if (!hp) {
		return;
	}
	hp->buffer = static_cast<uint8_t*>(buffer);
	hp->size = size;
}

struct HeapBlockHeader {
	size_t size;
};

void* chHeapAlloc(memory_heap_t*, size_t size) {
	auto* mem = static_cast<HeapBlockHeader*>(malloc(sizeof(HeapBlockHeader) + size));
	if (!mem) {
		return nullptr;
	}
	mem->size = size;
	return mem + 1;
}

void chHeapFree(void* p) {
	if (!p) {
		return;
	}
	auto* mem = reinterpret_cast<HeapBlockHeader*>(p) - 1;
	free(mem);
}

void chHeapStatus(memory_heap_t* hp, size_t* totalp, size_t* largestp) {
	size_t total = hp ? hp->size : 0;
	if (totalp) {
		*totalp = total;
	}
	if (largestp) {
		*largestp = total;
	}
}

size_t chHeapGetSize(void* p) {
	if (!p) {
		return 0;
	}
	auto* mem = reinterpret_cast<HeapBlockHeader*>(p) - 1;
	return mem->size;
}

size_t chCoreGetStatusX(void) {
	return 0;
}

} // extern "C"
