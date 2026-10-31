/*
 * In-process ECU for the engine-sim host (EpicEFI Engine Simulator): the ecu_* C ABI
 * the host resolves with dlsym from the library named by ENGINE_SIM_ECU_DLL.
 *
 * Ported from EpicEFI's epicefi_fw simulator/ecu_dll.cpp (GPL-3.0, rusEFI-derived)
 * onto rusEFI's own firmware APIs. The exported names, argument types, return codes
 * and the three shared layouts (EcuSensorFrame, EcuSimDiagnostics, the ESM12
 * actuator frame of ecu_read_events) are the host's ABI and must not change.
 *
 * Contract (from the host):
 *  - Init-once. The firmware threads never exit, so the process is the teardown:
 *    the host never dlclose()s this module. Restart = ecu_halt() + a fresh copy of
 *    the library under another file name.
 *  - ecu_stage_input()/ecu_sensor_frame() + ecu_commit() come from the host physics
 *    thread. ecu_commit() applies the staged inputs all-or-nothing.
 *  - The TunerStudio TCP ports stay live inside the host process.
 *  - Required by the host: ecu_init, ecu_stage_input, ecu_commit, ecu_read_events.
 *    Everything else is optional and probed with dlsym.
 *
 * Differences from the EpicEFI original:
 *  - Sensor names resolve against rusEFI's SensorType names (Sensor::getSensorName),
 *    plus the EngineSim* trigger/start inputs (engine_sim_trigger.cpp).
 *  - Actuator events come from per-cylinder counters (sim_output_counters.h) hooked
 *    into InjectorOutputPin::open() and the spark fall edge, instead of EpicEFI's
 *    injections[]/ignitions[] output channels.
 *  - No acceleration-enrichment instant pulses and no bench-test pulse playback.
 */

#include "pch.h"

#include "rusEfiFunctionalTest.h"
#include "simulator_flash_files.h"
#include "tunerstudio.h"
#include "tunerstudio_io.h"
#include "engine_sim_trigger.h"
#include "ecu_sim_diagnostics.h"
#include "sim_clock.h"
#include "ecu_sensor_frame.h"
#include "sim_output_counters.h"
#include "fw_configuration.h"
#include "flash_main.h"
#include "storage.h"
#include "fuel_pump.h"
#include "boost_control.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#include <strings.h>

#define ECU_DLL_EXPORT __attribute__((visibility("default")))

volatile uint32_t simInjectionOpenCount[MAX_CYLINDER_COUNT];
volatile uint32_t simSparkFireCount[MAX_CYLINDER_COUNT];

// In sim_shell_common.cpp / posix/fatfs_diskio.cpp, shared with the exe shell (main_posix.cpp).
void simFatfsMount();
void startSimConsole();
extern bool main_loop_started;

// Return codes for ecu_init().
#define ECU_INIT_OK            0
#define ECU_INIT_ALREADY      -1
#define ECU_INIT_TS_BIND      -3 // booted, but at least one TS port failed to bind
#define ECU_INIT_FAULT        -4 // the boot threw; contained at this boundary, module left inert

void simExecutorLateSnapshot(char*, size_t, efitick_t&, efitick_t&);

static void appendEcuFaultLog(const char* what, const char* detail) {
	fprintf(stderr, "[ecu_dll] fault in %s: %s\n", what, detail);
	fflush(stderr);
}

// Every export runs inside the host process. In non-production builds firmwareError()
// THROWS (error_handling.cpp), so one bad tune value would otherwise unwind into the
// host and kill it. Contain at the boundary: a log line plus an error return.
template <typename F>
static int ecuGuard(const char* what, F&& f, int failValue) {
	try {
		return f();
	} catch (const std::exception& e) {
		printf("\x1B[31m[ecu_dll] %s: firmware fault contained: %s\x1B[0m\r\n", what, e.what());
		appendEcuFaultLog(what, e.what());
	} catch (...) {
		printf("\x1B[31m[ecu_dll] %s: firmware fault contained (non-standard exception)\x1B[0m\r\n", what);
		appendEcuFaultLog(what, "non-standard exception");
	}
	fflush(stdout);
	return failValue;
}

template <typename F>
static void ecuGuardVoid(const char* what, F&& f) {
	ecuGuard(what, [&] { f(); return 0; }, 0);
}

// ------------------------------------------------------------------
// Input routing: a host input name -> an EngineSim seam index or a rusEFI sensor.
namespace {
struct MockRoute {
	int engineSimIndex = 0;            // 1..8 = EngineSim* input (engine_sim_trigger.cpp)
	SensorType type = SensorType::Invalid;
};

bool resolveMockRoute(const char* name, MockRoute& route) {
	route = {};
	if (!name || !*name) {
		return false;
	}
	if (int index = engineSimInputIndex(name)) {
		route.engineSimIndex = index;
		return true;
	}
	for (int i = 1; i < static_cast<int>(SensorType::PlaceholderLast); i++) {
		const auto type = static_cast<SensorType>(i);
		const char* sensorName = Sensor::getSensorName(type);
		if (sensorName && strcasecmp(sensorName, name) == 0) {
			route.type = type;
			return true;
		}
	}
	return false;
}

bool validateRoute(const MockRoute& route, float value) {
	if (route.engineSimIndex) {
		return validateEngineSimInputIndex(route.engineSimIndex, value) == 1;
	}
	return route.type != SensorType::Invalid;   // NaN = release, +/-inf = wiring fault, else a value
}

void applyRoute(const MockRoute& route, float value) {
	if (route.engineSimIndex) {
		stageEngineSimInputIndex(route.engineSimIndex, value);
		return;
	}
	const auto type = route.type;
	if (std::isnan(value)) {
		Sensor::resetMockValue(type);
	} else if (std::isinf(value)) {
		// A simulated wiring fault: +/-inf = open / shorted input -> the sensor reads invalid.
		// (rusEFI's invalid mock carries no High/Low code.)
		Sensor::setInvalidMockValue(type);
	} else {
		// A mocked combined TPS / pedal is the plant's already-agreeing pair (drive-by-wire).
		Sensor::setMockValue(type, value, type == SensorType::Tps1 || type == SensorType::AcceleratorPedal);
	}
}

struct ParsedRecord {
	MockRoute route;
	float value;
	bool resetAll;
};

// String records: name NUL-terminated + float LE. "" or "*" resets every mock.
// Returns false (nothing applied) when any record is malformed, unknown or out of range.
bool parseRecords(const char* data, size_t size, std::vector<ParsedRecord>& out) {
	size_t pos = 0;
	while (pos < size) {
		const char* name = data + pos;
		const void* nul = memchr(name, 0, size - pos);
		if (!nul) return false;
		const size_t nameLen = static_cast<const char*>(nul) - name;
		pos += nameLen + 1;
		if (pos + sizeof(float) > size) return false;
		float value;
		memcpy(&value, data + pos, sizeof(value));
		pos += sizeof(value);
		ParsedRecord record{{}, value, nameLen == 0 || (nameLen == 1 && name[0] == '*')};
		if (!record.resetAll && (!resolveMockRoute(name, record.route) || !validateRoute(record.route, value))) {
			return false;
		}
		out.push_back(record);
	}
	return true;
}

// All-or-nothing: validate every dirty slot and string record first, then apply.
int ecuApplyMockFrame(const MockRoute* routes, const float* values, uint64_t dirty, const char* data, size_t size) {
	for (int slot = 0; slot < ECU_SENSOR_FRAME_SLOTS; slot++) {
		if ((dirty >> slot & 1) && !validateRoute(routes[slot], values[slot])) {
			return -1;
		}
	}
	std::vector<ParsedRecord> records;
	if (!parseRecords(data, size, records)) {
		return -1;
	}
	chibios_rt::CriticalSectionLocker lock;
	for (int slot = 0; slot < ECU_SENSOR_FRAME_SLOTS; slot++) {
		if (dirty >> slot & 1) {
			applyRoute(routes[slot], values[slot]);
		}
	}
	for (const auto& record : records) {
		if (record.resetAll) {
			releaseEngineSimInputs();
			Sensor::resetAllMocks();
		} else {
			applyRoute(record.route, record.value);
		}
	}
	return 0;
}
} // namespace

extern "C" {

static void booted(const char *flashDir, const char *iniDir);
static int ecuReadEvents(void* out, int size);

// Call before ecu_init, so every controller and virtual timer starts on this clock.
ECU_DLL_EXPORT int ecu_enable_simulation_clock(void) {
	return ecuGuard("ecu_enable_simulation_clock", [] {
		return simClockEnable(getTimeNowNt(), US_TO_NT_MULTIPLIER);
	}, -1);
}

ECU_DLL_EXPORT int ecu_step(double dtSeconds, double crankAngle, double crankSpeed) {
	if (!simClockEnabled() || !std::isfinite(dtSeconds) || dtSeconds <= 0 || dtSeconds > 0.01) return -1;
	const auto delta = static_cast<efitick_t>(std::llround(dtSeconds * 1e6 * US_TO_NT_MULTIPLIER));
	if (delta <= 0) return -1;
	return ecuGuard("ecu_step", [=] {
		return engineSimAdvancePlantTo(getTimeNowNt() + delta, crankAngle, crankSpeed);
	}, -1);
}

static bool s_booted = false;

ECU_DLL_EXPORT int ecu_init(const char *flashDir, const char *iniDir) {
	if (s_booted) {
		return ECU_INIT_ALREADY;
	}
	// A fault on a firmware thread cannot unwind into this handler, so name the
	// cause before the process dies instead.
	std::set_terminate([] {
		printf("\x1B[31m[ecu_dll] firmware thread terminated on an uncaught exception\x1B[0m\r\n");
		appendEcuFaultLog("terminate", "a firmware thread died on an uncaught exception");
		fflush(stdout);
		std::abort();
	});
	return ecuGuard("ecu_init", [&] {
		booted(flashDir, iniDir);
		return tsBindFailureReported() ? ECU_INIT_TS_BIND : ECU_INIT_OK;
	}, ECU_INIT_FAULT);
}

// A throw in here leaves s_booted true: the module stays inert and the host restarts
// it as a fresh module copy - never a retry that re-runs init on half-booted firmware.
static void booted(const char *flashDir, const char *iniDir) {
	s_booted = true;

	if (iniDir && *iniDir) {
		printf("[ecu_dll] INI dir (host-side, reserved): %s\n", iniDir);
	}
	if (flashDir && *flashDir) {
		// The tune lives in <flashDir>/simulator_flash (primary.bin/backup.bin/tunes).
		// Pin an absolute root: the host owns the working directory.
		std::error_code ec;
		const std::filesystem::path absolute =
			(std::filesystem::absolute(flashDir, ec) / "simulator_flash").lexically_normal();
		simulatorFlashSetRoot(absolute.string().c_str());
		printf("[ecu_dll] flash root pinned: %s\n", absolute.string().c_str());
	}

	// A TS port bind failure must not terminate the host process.
	setTsBindFailureIsFatal(false);

	setup_custom_fw_overrides();

	printf("=================================== rusEFI ECU LIBRARY STARTED ===================================\n");
	halInit();
	chSysInit();
	simFatfsMount();
	startSimConsole();

	simulatorFlashInitOnBoot();
	rusEfiFunctionalTest();

	// The exe's main loop, minus the reboot poll (re-exec is shell machinery).
	extern volatile int ecuDbgCommits, ecuDbgPolls, ecuDbgEdges, ecuDbgActive;
	extern volatile float ecuDbgSpeed, ecuDbgAngle;
	std::thread([] {
		int lastCommits = -1;
		int beat = 0;
		while (!simHalted()) {
			printPendingMessages();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			// ~2 Hz liveness line for the synthetic wheel. Never print inside the
			// firmware's critical sections.
			if (++beat % 500 == 0 && ecuDbgCommits != lastCommits) {
				printf("[ecu] wheel: commits=%d polls=%d edges=%d active=%d speed=%.1f angle=%.1f rpm=%.0f\n",
					ecuDbgCommits, ecuDbgPolls, ecuDbgEdges, ecuDbgActive, ecuDbgSpeed, ecuDbgAngle,
					(float)Sensor::getOrZero(SensorType::Rpm));
				fflush(stdout);
				lastCommits = ecuDbgCommits;
				char label[80];
				efitick_t requested, actual;
				simExecutorLateSnapshot(label, sizeof(label), requested, actual);
				if (actual) printf("[ecu] late scheduler: %s requested=%.3fus executed=%.3fus\n",
					label, static_cast<double>(requested) / US_TO_NT_MULTIPLIER,
					static_cast<double>(actual) / US_TO_NT_MULTIPLIER);
			}
		}
	}).detach();

	main_loop_started = true;
}

// Staged string records (name NUL-terminated + float LE), applied by ecu_commit.
static std::string s_pendingMocks;

// The shared sensor frame and each slot's name and resolved route.
static EcuSensorFrame s_frame = {ECU_SENSOR_FRAME_ABI, ECU_SENSOR_FRAME_SLOTS, 0, {}};
static MockRoute s_frameRoutes[ECU_SENSOR_FRAME_SLOTS];
static std::string s_frameNames[ECU_SENSOR_FRAME_SLOTS];
static int s_frameSlotCount = 0;

ECU_DLL_EXPORT EcuSensorFrame *ecu_sensor_frame(void) {
	return simHalted() ? nullptr : &s_frame;
}

// The frame slot for an input name, the same slot for the same name.
// -1: unknown name, all slots taken, or a halted module.
ECU_DLL_EXPORT int ecu_sensor_slot(const char *name) {
	if (simHalted() || !name || !*name || !std::strcmp(name, "*")) {
		return -1;
	}
	return ecuGuard("ecu_sensor_slot", [&]() -> int {
		for (int slot = 0; slot < s_frameSlotCount; ++slot) {
			if (s_frameNames[slot] == name) return slot;
		}
		MockRoute route;
		if (s_frameSlotCount == ECU_SENSOR_FRAME_SLOTS || !resolveMockRoute(name, route)) {
			printf("[ecu_dll] input '%s' has no rusEFI route\n", name);
			return -1;
		}
		s_frameRoutes[s_frameSlotCount] = route;
		s_frameNames[s_frameSlotCount] = name;
		return s_frameSlotCount++;
	}, -1);
}

ECU_DLL_EXPORT void ecu_stage_input(const char *name, double value) {
	if (simHalted() || !name) {
		return;
	}
	ecuGuardVoid("ecu_stage_input", [&] {
		// An unknown name would reject the whole frame; drop it here instead (reported once).
		if (*name && std::strcmp(name, "*")) {
			MockRoute route;
			if (!resolveMockRoute(name, route)) {
				static std::string reported;
				const std::string key = std::string("|") + name + "|";
				if (reported.find(key) == std::string::npos && reported.size() < 4096) {
					reported += key;
					printf("[ecu_dll] ignoring input '%s': no rusEFI sensor of that name\n", name);
				}
				return;
			}
		}
		const float v = static_cast<float>(value);
		uint32_t bits;
		memcpy(&bits, &v, sizeof(bits));
		s_pendingMocks.append(name, std::strlen(name) + 1);
		s_pendingMocks.append(reinterpret_cast<const char*>(&bits), sizeof(bits));
	});
}

static bool s_synthRpmArmed = false;

ECU_DLL_EXPORT int ecu_commit(void) {
	if (simHalted()) {
		s_pendingMocks.clear();
		s_frame.dirty = 0;
		return -1;
	}
	// -2 = a firmware fault under the commit, -1 = the frame was rejected.
	return ecuGuard("ecu_commit", [&]() -> int {
		int status = 0;
		const uint64_t dirty = s_frame.dirty;
		if (dirty || !s_pendingMocks.empty()) {
			s_frame.dirty = 0;
			status = ecuApplyMockFrame(s_frameRoutes, s_frame.value, dirty,
				s_pendingMocks.data(), s_pendingMocks.size()) == 0 ? 0 : -1;
			s_pendingMocks.clear();
		}
		if (s_synthRpmArmed) {
			// ECU-only rev test: release the EngineSim seam so the trigger emulator owns the wheel.
			stageEngineSimInput("EngineSimEnable", std::numeric_limits<float>::quiet_NaN());
		}
		commitEngineSimInputs();
		return status;
	}, -2);
}

// Loads <flashDir>/simulator_flash into the RUNNING firmware, the way a burn applies a tune.
// Call it on the thread that drives ecu_step, between steps. Needs the simulation clock
// (sim-clock timers run inside ecu_step, which serializes this against them).
//  0 ok, -1 not booted / halted / no dir, -3 a burn is still queued, -4 no simulation clock,
// -5 the loaded tune failed validation or its trigger shape, -2 firmware fault.
// On any < 0 the host falls back to halt + a fresh module copy.
ECU_DLL_EXPORT int ecu_reload(const char *flashDir) {
	if (!s_booted || simHalted() || !flashDir || !*flashDir) {
		return -1;
	}
	if (!simClockEnabled()) {
		return -4;
	}
	if (getNeedToWriteConfiguration()) {
		return -3;
	}
	return ecuGuard("ecu_reload", [&]() -> int {
		releaseEngineSimInputs();
		Sensor::resetAllMocks();
		s_pendingMocks.clear();
		s_frame.dirty = 0;
		engine->triggerCentral.directSelfStimulation = false;
		engine->OnTriggerSynchronizationLost();

		std::error_code ec;
		const std::filesystem::path absolute =
			(std::filesystem::absolute(flashDir, ec) / "simulator_flash").lexically_normal();
		simulatorFlashSetRoot(absolute.string().c_str());
		simulatorFlashInitOnBoot();
		readFromFlash();
		printf("[ecu_dll] reload: %s\n", absolute.string().c_str());
		if (!validateConfigOnStartUpOrBurn()) {
			return -5;
		}
		incrementGlobalConfigurationVersion("ecu_reload");
		engine->updateTriggerConfiguration();
		if (engine->triggerCentral.triggerShape.shapeDefinitionError) {
			return -5;
		}
		onApplyPreset();
		fflush(stdout);
		return 0;
	}, -2);
}

ECU_DLL_EXPORT void ecu_set_wheel_offset(double deg) {
	// Rotates the synthetic wheel relative to the staged crank phase (aligns the decoder's
	// TDC with the plant's cycle).
	ecuGuardVoid("ecu_set_wheel_offset", [=] {
		engineSimSetWheelOffset(deg);
	});
}

ECU_DLL_EXPORT void ecu_set_synth_rpm(int on) {
	if (on) {
		s_pendingMocks.clear();
		s_frame.dirty = 0;
	}
	s_synthRpmArmed = on != 0;
}

ECU_DLL_EXPORT void ecu_set_synchronous_poll(int on) {
	// The wheel advances synchronously from ecu_commit instead of a self-re-arming timer.
	ecuGuardVoid("ecu_set_synchronous_poll", [=] {
		engineSimSetSynchronousPoll(on != 0);
	});
}

ECU_DLL_EXPORT int ecu_output_block_size(void) {
	return static_cast<int>(sizeof(engine->outputChannels));
}

ECU_DLL_EXPORT int ecu_read_diagnostics(void* out, int size) {
	if (simHalted()) return -1;
	if (!out || size != sizeof(EcuSimDiagnostics)) return -1;
	return ecuGuard("ecu_read_diagnostics", [&] {
		EcuSimDiagnostics snapshot;
		engineSimReadDiagnostics(snapshot);
		if (s_synthRpmArmed) snapshot.flags |= 16;
		std::memcpy(out, &snapshot, sizeof(snapshot));
		return 0;
	}, -1);
}

// rusEFI's output_channels_s layout, NOT EpicEFI's: host features that decode this block
// by EpicEFI field offsets will read wrong values.
ECU_DLL_EXPORT int ecu_read_outputs(uint8_t *buffer, int size) {
	if (simHalted() || !buffer || size < 0) {
		return -1;
	}
	return ecuGuard("ecu_read_outputs", [=] {
		updateTunerStudioState();
		constexpr int blockSize = sizeof(engine->outputChannels);
		const int copy = size < blockSize ? size : blockSize;
		memcpy(buffer, &engine->outputChannels, copy);
		return copy;
	}, -1);
}

// POSIX: the module shares the process stdout, nothing to retarget.
ECU_DLL_EXPORT int ecu_console_redirect(void *handle) {
	(void)handle;
	return -1;
}

ECU_DLL_EXPORT void ecu_shutdown(void) {
	// Partial by design: closes the TunerStudio listeners; process exit is the real teardown.
	ecuGuardVoid("ecu_shutdown", [] {
		stopSerialChannels();
	});
}

// Halts this module for good: closes the TS listeners and parks every firmware thread
// at its next RTOS call. Returns 0, or -1 when the firmware never booted. Idempotent.
ECU_DLL_EXPORT int ecu_halt(void) {
	if (!s_booted) {
		return -1;
	}
	return ecuGuard("ecu_halt", [&] {
		stopSerialChannels();
		simHalt();
		int parked = 0;
		const int live = simHaltedThreads(&parked);
		printf("[ecu_dll] halted: TS listeners closed, %d/%d firmware threads parked\n", parked, live);
		fflush(stdout);
		return 0;
	}, -2);
}

} // extern "C"

// ------------------------------------------------------------------
// In-process actuator events (the host's ESM12 frame): per-output event histories the
// plant's playback consumes, read from the live firmware every physics step.
#pragma pack(push, 1)
namespace ecuact {
constexpr int OutputCount = 12, EventHistory = 4, CamCount = 4;
constexpr uint32_t Magic = 0x434d5345; // ESMC (ESM12)
constexpr uint32_t InstantInjectionFlag = 128, RedlineFlag = 256, BoostDutyFlag = 512, DwellFlag = 1024,
	EtbDutyFlag = 2048, VvtDutyFlag = 4096, StarterControlFlag = 8192, StarterOnFlag = 16384;
struct OutputEvents {
	uint32_t injectorSequence = 0, sparkSequence = 0;
	struct Injection { float angle = 0, pulseMs = 0; } injection[EventHistory];
	float sparkAngle[EventHistory]{};
};
struct Actuators {
	uint32_t magic = 0x434d5345, sequence = 0, flags = 0, idlePositionMilliPct = 0;
	float rpm = 0, pulseMs = 0, injectionAngle = 360, snapshotAge = 0;
	float advance[12]{};
	OutputEvents outputs[OutputCount];
	float redlineRpm = 0, boostDutyPct = 0;
	float dwellMs = 0;
	float etbDutyPct = 0;
	float vvtDutyPct[CamCount] = {0, 0, 0, 0};
	float trailingSplitDeg = 0;
};
static_assert(sizeof(Actuators) == 788 && sizeof(OutputEvents) == 56);
}
#pragma pack(pop)

namespace {
struct EcuEventState {
	bool initialized = false;
	int order[12] = {};            // firing slot per cylinder
	double offset[12] = {};        // odd-fire trim per cylinder, deg
	uint16_t previousFuel[12] = {}, previousSpark[12] = {};
	ecuact::OutputEvents output[12];
};
EcuEventState ecuEventState;
float ecuWrapDeg(double a) {
	a = std::fmod(a, 720.0);
	return static_cast<float>(a < 0 ? a + 720 : a);
}
template <typename Writer>
void ecuAppend(int count, uint32_t& sequence, Writer write) {
	if (count > ecuact::EventHistory) {
		sequence += ecuact::EventHistory + 1; // explicit overrun, no fabricated burst
		return;
	}
	while (count-- > 0) {
		write(sequence % ecuact::EventHistory);
		++sequence;
	}
}
float finiteOr(float value, float fallback) {
	return std::isfinite(value) ? value : fallback;
}
}

extern "C" ECU_DLL_EXPORT int ecu_read_events(void* out, int size) {
	if (simHalted() || !out || size < (int)sizeof(ecuact::Actuators)) {
		return -1;
	}
	// -5 = a firmware fault under the read, contained here.
	return ecuGuard("ecu_read_events", [=] {
		return ecuReadEvents(out, size);
	}, -5);
}

static int ecuReadEvents(void* out, int /*size*/) {
	auto& events = *static_cast<ecuact::Actuators*>(out);
	events = {};
	auto& oc = engine->outputChannels;
	const int cylinders = engineConfiguration->cylindersCount;
	if (cylinders < 1 || cylinders > 12) {
		return -2;
	}
	static efitick_t nextStateUpdate = 0;
	if (getTimeNowNt() >= nextStateUpdate) {
		nextStateUpdate = getTimeNowNt() + MS2NT(10);
		updateTunerStudioState();
	}
	events.magic = ecuact::Magic;
	events.flags = 1 | 32 | 64; // live snapshot, idle command, counted events
	const float idle = engine->module<IdleController>().unmock().currentIdlePosition;
	events.idlePositionMilliPct = static_cast<uint32_t>(std::lround(std::clamp(finiteOr(idle, 0.0f), 0.0f, 100.0f) * 1000));
	events.rpm = oc.RPMValue;
	if (engine->triggerCentral.triggerState.hasSynchronizedPhase()) events.flags |= 2;
	if (engine->module<FuelPumpController>()->isFuelPumpOn) events.flags |= 16;
	if (!oc.hasCriticalError) {
		if (oc.fuelCutReason == 0) events.flags |= 4;
		if (oc.sparkCutReason == 0) events.flags |= 8;
	}
	events.pulseMs = oc.actualLastInjection;
	events.injectionAngle = oc.injectionOffset;
	for (int i = 0; i < cylinders; ++i) {
		events.advance[i] = oc.ignitionAdvanceCyl[i];
	}
	events.boostDutyPct = std::clamp(finiteOr((float)engine->module<BoostController>()->boostOutput, 0.0f), 0.0f, 100.0f);
	events.flags |= ecuact::BoostDutyFlag;
	const float dwell = engine->ignitionState.sparkDwell;
	if (std::isfinite(dwell) && dwell >= 0 && dwell <= 30) {
		events.dwellMs = dwell;
		events.flags |= ecuact::DwellFlag;
	}
	events.etbDutyPct = std::clamp(finiteOr((float)oc.etb1DutyCycle, 0.0f), -100.0f, 100.0f);
	events.flags |= ecuact::EtbDutyFlag;
	for (int cam = 0; cam < ecuact::CamCount; ++cam) {
		events.vvtDutyPct[cam] = std::clamp(finiteOr((float)oc.vvtStatus[cam].output, 0.0f), 0.0f, 100.0f);
	}
	events.flags |= ecuact::VvtDutyFlag;
	// starterState is the output's logic value; only a wired relay turns the motor.
	if (isBrainPinValid(engineConfiguration->starterControlPin)) {
		events.flags |= ecuact::StarterControlFlag;
		if (oc.starterState) events.flags |= ecuact::StarterOnFlag;
	}
	const float redline = engineConfiguration->rpmHardLimit;
	if (redline >= 500 && redline <= 30000) {
		events.redlineRpm = redline;
		events.flags |= ecuact::RedlineFlag;
	}
	auto& st = ecuEventState;
	for (int slot = 0; slot < cylinders; ++slot) {
		st.order[getCylinderNumberAtIndex(slot)] = slot;
	}
	for (int i = 0; i < cylinders; ++i) {
		st.offset[i] = engineConfiguration->timing_offset_cylinder[i];
	}
	auto& injectorModel = engine->module<InjectorModelPrimary>().unmock();
	const float deadtime = injectorModel.getDeadtime();
	const float mass1 = std::max(0.0f, (float)engine->engineState.injectionMass[0]);
	const float scheduled = oc.actualLastInjection;
	// No instant (asynchronous AE) pulses in rusEFI: the flag says the counters are exact.
	events.flags |= ecuact::InstantInjectionFlag;
	if (engineConfiguration->ignitionMode == IM_WASTED_SPARK) events.flags |= 65536;
	if (engineConfiguration->enableTrailingSparks) {
		events.flags |= 32768;
		events.trailingSplitDeg = std::clamp(finiteOr((float)engine->ignitionState.trailingSparkAngle, 0.0f), 0.0f, 180.0f);
	}
	// Event angles are decoder-phase; the playback matches them against the plant's
	// crank angle, so translate them back by the wheel's slew drift first.
	const float drift = engineSimGetWheelPhaseDrift();
	for (int i = 0; i < ecuact::OutputCount; ++i) {
		auto& history = st.output[i];
		const auto fuelCount = static_cast<uint16_t>(i < MAX_CYLINDER_COUNT ? simInjectionOpenCount[i] : 0);
		const auto sparkCount = static_cast<uint16_t>(i < MAX_CYLINDER_COUNT ? simSparkFireCount[i] : 0);
		const uint16_t fuelDelta = fuelCount - st.previousFuel[i];
		const uint16_t sparkDelta = sparkCount - st.previousSpark[i];
		st.previousFuel[i] = fuelCount;
		st.previousSpark[i] = sparkCount;
		if (!st.initialized || i >= cylinders) {
			continue;
		}
		const double reference = st.order[i] * 720.0 / cylinders + st.offset[i];
		const float mass = std::max(0.0f, (float)engine->engineState.injectionMass[i]);
		// Scale the scheduled cylinder-1 duration by each cylinder's mass.
		float width = mass > 0 ? (float)injectorModel.getInjectionDuration(mass) : 0;
		if (mass > 0 && mass1 > 0 && scheduled > deadtime) {
			width = (scheduled - deadtime) * mass / mass1 + deadtime;
		}
		if (!std::isfinite(width) || width < 0 || width > 1000) {
			return -4;
		}
		ecuAppend(fuelDelta, history.injectorSequence, [&](unsigned slot) {
			history.injection[slot] = {ecuWrapDeg(reference + oc.injectionOffset - drift), (float)width};
		});
		ecuAppend(sparkDelta, history.sparkSequence, [&](unsigned slot) {
			history.sparkAngle[slot] = ecuWrapDeg(reference - (float)oc.ignitionAdvanceCyl[st.order[i]] - drift);
		});
	}
	st.initialized = true;
	std::memcpy(&events.outputs, &st.output, sizeof(events.outputs));
	return 0;
}
