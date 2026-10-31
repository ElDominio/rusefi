#pragma once

// Per-cylinder actuator event counters for the in-process engine-sim ECU
// (simulator/ecu_dll.cpp ecu_read_events): the plant replays one injection per
// opening (by injector) and one spark per ignition event (by cylinder). 64-bit POSIX simulator only; defined in
// simulator/ecu_dll.cpp.
#if defined(EFI_SIMULATOR_POSIX)
#include <cstdint>
extern volatile uint32_t simInjectionOpenCount[MAX_CYLINDER_COUNT];
extern volatile uint32_t simSparkFireCount[MAX_CYLINDER_COUNT];
#endif
