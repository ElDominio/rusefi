/*
 * Simulator shell pieces shared by the exe (main_posix.cpp) and the DLL
 * (ecu_dll.cpp): the console message thread, printToConsole, and the globals
 * firmware code references. The DLL link excludes main_posix.o (it has
 * main()), so these cannot live there -- and PE/COFF ld does not resolve
 * weak definitions across objects the way ELF does.
 */

#include "pch.h"

#define CONSOLE_WA_SIZE     THD_WORKING_AREA_SIZE(4096)

bool main_loop_started = false;
bool verboseMode = true;

static thread_t *cdtp;

#define cputs(msg) chMsgSend(cdtp, (msg_t)msg)

void printToConsole(const char *p) {
	cputs(p);
}

THD_FUNCTION(console_thread, arg) {
	(void) arg;
	while (!chThdShouldTerminateX()) {
		thread_t *tp = chMsgWait();
		printf("console_thread: got message\n");
		puts((char *) chMsgGet(tp));
		fflush(stdout);
		chMsgRelease(tp, MSG_OK);
	}
}

void startSimConsole() {
	cdtp = chThdCreateFromHeap(NULL, CONSOLE_WA_SIZE,
			"sim",
			NORMALPRIO + 1, console_thread, NULL);
}
