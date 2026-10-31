/*
 * POSIX simulator main (no ChibiOS RT dependency).
 */

#include "pch.h"
#include "chprintf.h"
#include "rusEfiFunctionalTest.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <cstring>
#include <thread>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include "fw_configuration.h"
#include "board_overrides.h"
#include "perf_trace.h"
#include "sim_backtrace.h"
#include "simulator_flash_files.h"
#include "sim_reboot.h"

// Console thread, printToConsole(), main_loop_started, and verboseMode live in
// sim_shell_common.cpp -- the DLL link excludes this file but needs them too.

static void exitOnAlarm(int) {
	std::_Exit(0);
}

static void exitOnAbort(int) {
	// Dump trace before dying on unexpected abort (e.g. assertion failure).
	signal(SIGABRT, SIG_DFL);
	raise(SIGABRT);
}

/* SIGSEGV/SIGBUS backtrace. Without this a null deref in firmware code is a bare
 * "Segmentation fault" with nothing to go on; the frames name the culprit directly.
 * backtrace()/write() are not strictly async-signal-safe and this only walks the crashing
 * thread, so it is a pointer, not a substitute for reproducing under gdb with `bt all`.
 *
 * Frames print as `build/rusefi_simulator(+0x<offset>)`, not as function names: the link
 * deliberately omits -rdynamic (it would defeat --gc-sections and break the build -- see
 * simulator/Makefile). Resolve a frame with:
 *     addr2line -f -C -e simulator/build/rusefi_simulator 0x<offset>  */
static void faultHandler(int sig) {
	simPrintBacktrace("\n*** SIMULATOR FAULT, backtrace (crashing thread) ***\n");
	signal(sig, SIG_DFL);
	raise(sig);
}

static void armExitTimer(int timeoutSeconds) {
	printf("Running rusEFI simulator for %d seconds, then exiting.\n\n", timeoutSeconds);
#if defined(_WIN32)
	std::thread([timeoutSeconds]() {
		std::this_thread::sleep_for(std::chrono::seconds(timeoutSeconds));
		exitOnAlarm(0);
	}).detach();
#else
	signal(SIGALRM, exitOnAlarm);
	alarm(timeoutSeconds);
#endif
}

extern int isSerialOverTcpReady;

// board override demo:
static inline void myCustomHello(){
	efiPrintf("custom board hello from simulator");
}

void setup_custom_board_overrides(){
	custom_board_boardSayHello = myCustomHello;
}

void simFatfsMount();
void startSimConsole();

#if defined(__linux__)
/* Android sandboxes every app process -- and anything it exec's -- behind a
 * seccomp filter that SIGSYS-kills any syscall outside the app allowlist. A
 * static-musl child has no bionic in it, so nobody logs the death: it is a
 * silent rc=159 on the supervisor. The filter delivers a *catchable* SIGSYS
 * with the offending syscall in siginfo, so name it in our own boot log and
 * then take the default death. Pragmatic like faultHandler below: snprintf
 * is not async-signal-safe, but the process dies on return either way. */
static void sigsysLogger(int sig, siginfo_t *info, void *) {
	char buf[96];
	int len = snprintf(buf, sizeof(buf),
		"\n*** SIGSYS: blocked syscall %d (arch 0x%x) at %p ***\n",
		(int)info->si_syscall, (unsigned)info->si_arch, info->si_call_addr);
	if (len > 0) {
		if (write(STDERR_FILENO, buf, (size_t)len) < 0) { /* dying anyway */ }
	}
	signal(SIGSYS, SIG_DFL);
	raise(sig);
}

/* Android can SIGKILL the whole app (force-stop, update, memory pressure)
 * without running any cleanup hook, orphaning this exec'd simulator to init
 * while it still holds TS1/TS2 -- every later spawn then dies on bind. When
 * the supervisor asks for it (env var), tie our fate to the parent thread so
 * the kernel kills us with it. Opt-in so launchers that daemonize on purpose
 * (rig.sh, nohup, procd) keep today's behavior. */
static void dieWithParentIfAsked() {
	if (getenv("EPICEFI_DIE_WITH_PARENT") == nullptr) {
		return;
	}
	prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0);
	/* PDEATHSIG only covers parent deaths from this moment on; close the
	 * fork-to-prctl race by checking the parent directly (init adopts
	 * orphans, and nothing legitimate has init as its parent here). */
	if (getppid() == 1) {
		fprintf(stderr, "parent already gone at startup\n");
		_exit(1);
	}
}
#endif

int main(int argc, char** argv) {
#if defined(__linux__)
	dieWithParentIfAsked();
	{
		struct sigaction sa = {};
		sa.sa_sigaction = sigsysLogger;
		sa.sa_flags = SA_SIGINFO;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGSYS, &sa, nullptr);
	}
#endif
	simRebootRememberArgs(argc, argv);
	simWritePidFile();
	setbuf(stdout, NULL);
	signal(SIGSEGV, faultHandler);
#ifdef SIGBUS
	signal(SIGBUS, faultHandler);
#endif
	setup_custom_fw_overrides();
	setup_custom_board_overrides();

	printf("=================================== SIMULATOR STARTED ===================================\n");
	halInit();
	chSysInit();
	simFatfsMount();

	signal(SIGABRT, exitOnAbort);

	if (argc == 2) {
		int timeoutSeconds = atoi(argv[1]);
		armExitTimer(timeoutSeconds);
	}

	// Console thread
	startSimConsole();

	simulatorFlashInitOnBoot();
	rusEfiFunctionalTest();

	while (!chThdShouldTerminateX()) {
		simRebootPoll();
		printPendingMessages();
		chThdSleepMilliseconds(1);
	}

	return 0;
}
