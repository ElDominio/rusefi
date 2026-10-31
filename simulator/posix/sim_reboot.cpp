#include "pch.h"

#include "sim_reboot.h"
#include "sim_net.h"
#include "storage.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

extern void stopSerialChannels();

static char** s_argv = nullptr;
static std::atomic<bool> s_requested{false};

static constexpr const char* kPidFileName = "simulator.pid";

static long simCurrentPid() {
#if defined(_WIN32)
	return static_cast<long>(_getpid());
#else
	return static_cast<long>(getpid());
#endif
}

void simWritePidFile() {
	FILE* file = std::fopen(kPidFileName, "w");
	if (file == nullptr) {
		return;
	}
	std::fprintf(file, "%ld\n", simCurrentPid());
	std::fclose(file);
}

static bool s_arming = false;
static bool s_sawPending = false;
static std::chrono::steady_clock::time_point s_armedAt;

// Storage polls every 100 ms. Wait long enough for a burn that was queued
// just before the reboot request to raise flashWritePending, then for that
// write to finish. Execing in the middle truncates primary.bin.
static constexpr std::chrono::milliseconds kFlashGrace{500};
static constexpr std::chrono::seconds kFlashGiveUp{30};

void simRebootRememberArgs(int argc, char** argv) {
	(void)argc;
	s_argv = argv;
}

void simRequestReboot() {
	bool already = s_requested.exchange(true);
	if (already) {
		return;
	}
	printf("[sim] reboot requested\n");
	fflush(stdout);
	efiPrintf("Rebooting (POSIX simulator re-exec after flash is idle)...");
}

static void abortReboot(const char* why) {
	printf("[sim] reboot aborted: %s\n", why);
	fflush(stdout);
	{
	}
	s_arming = false;
	s_sawPending = false;
	s_requested.store(false);
}

static void performReboot() {
	if (s_argv == nullptr || s_argv[0] == nullptr) {
		abortReboot("process arguments were not recorded");
		return;
	}
	printf("[sim] re-exec\n");
	fflush(stdout);
	// Drop the listen sockets before the image is replaced. CLOEXEC on the
	// sockets covers any fd a racing accept still holds.
	stopSerialChannels();
	if (simReexec(s_argv) != 0) {
		printf("[sim] re-exec failed: %s\n", std::strerror(errno));
		fflush(stdout);
		// The listen sockets are already closed. Continuing would look like
		// a hung ECU. Exit instead of spinning.
		std::_Exit(1);
	}
}

void simRebootPoll() {
	if (!s_requested.load()) {
		return;
	}
	if (!s_arming) {
		s_arming = true;
		s_sawPending = false;
		s_armedAt = std::chrono::steady_clock::now();
	}

	bool pending = getNeedToWriteConfiguration() || storageIsBusy();
	auto elapsed = std::chrono::steady_clock::now() - s_armedAt;
	if (pending) {
		s_sawPending = true;
		if (elapsed > kFlashGiveUp) {
			abortReboot("flash write still pending");
		}
		return;
	}
	if (!s_sawPending && elapsed < kFlashGrace) {
		return;
	}
	performReboot();
}
