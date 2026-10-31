#pragma once
#include <cstdint>
#include "ch.h"

// Opt-in before firmware boot. One host physics thread owns advancement; TS and
// console threads keep their wall-time I/O. Clock mode cannot change after boot.
struct SimClockStats {
    uint64_t advances = 0, callbacks = 0, lateCallbacks = 0, budgetHits = 0;
    uint64_t maxLatenessNt = 0;
};
bool simClockEnabled();
uint64_t simClockNowNt();
int simClockEnable(uint64_t startNt, uint32_t ticksPerUs);
int simClockAdvanceTo(uint64_t targetNt);
SimClockStats simClockStats();
void chVTSetAbsoluteNtI(virtual_timer_t*, uint64_t deadlineNt, vtfunc_t, void*);

// In-process halt (ecu_halt): from now on every firmware-owned thread parks for good at
// its next RTOS call outside a critical section (sleep, wait, chSysLock), holding no lock, and the clock and
// timer service stop dispatching. Host threads that never ran a firmware thread body are
// never parked. One-way: a halted module is inert and is never resumed; a host that
// wants the ECU back boots a fresh copy of the module.
void simHalt();      // returns once the running firmware threads parked (bounded, 250 ms)
bool simHalted();
int simHaltedThreads(int* parked);   // live firmware threads; *parked = how many parked
