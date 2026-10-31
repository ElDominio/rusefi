/**
 * Platform crash trace implementation.
 *
 * Keep this file independent of pch.h because windows.h conflicts with FatFS types included by
 * the firmware precompiled header.
 */
#include "sim_backtrace.h"

#include <cstdio>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

void simPrintBacktrace(const char* label) {
	void* addresses[64];
	USHORT count = RtlCaptureStackBackTrace(0, 64, addresses, nullptr);
	fputs(label, stderr);
	fprintf(stderr, "module base %p\n", static_cast<void*>(GetModuleHandleW(nullptr)));
	for (USHORT index = 0; index < count; index++) {
		fprintf(stderr, "  [%02u] %p\n", static_cast<unsigned>(index), addresses[index]);
	}
	fflush(stderr);
}

#else

#include <cstring>
#include <unistd.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#endif

void simPrintBacktrace(const char* label) {
	if (write(STDERR_FILENO, label, strlen(label)) < 0) { /* signal context: nothing to do */ }
#if defined(__GLIBC__)
	void* addresses[64];
	int count = backtrace(addresses, 64);
	backtrace_symbols_fd(addresses, count, STDERR_FILENO);
#endif
}

#endif
