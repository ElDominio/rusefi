#pragma once
#include <cstdint>

// Versioned C ABI. Snapshot is copied under the firmware critical section.
// Keep the plant's include/ecu_sim_diagnostics.h byte-for-byte identical.
struct EcuSimDiagnostics {
    uint32_t version = 2, size = sizeof(EcuSimDiagnostics);
    uint32_t flags = 0; // 1 active, 2 synchronous, 4 shaft sync, 8 phase sync, 16 emulator, 32 host clock
    uint32_t configuredType = 0, decoderType = 0, expectedPrimary = 0, shapeSize = 0;
    uint32_t triggerErrors = 0, orderingErrors = 0, syncLosses = 0;
    uint64_t commits = 0, polls = 0, crankEdges = 0, camEdges = 0;
    uint64_t nonMonotonicEdges = 0, batchLimitHits = 0, skippedAdvances = 0;
    double ecuWallUs = 0, sampleAgeUs = 0, stagedAngle = 0, wheelPhase = 0;
    double wheelRpm = 0, decodedRpm = 0, phaseErrorDeg = 0;
    double commitIntervalUs = 0, maxCommitIntervalUs = 0, lastRiseIntervalUs = 0;
    uint64_t clockAdvances = 0, timerCallbacks = 0, lateCallbacks = 0, timerBudgetHits = 0;
    double maxTimerLatenessUs = 0;
};
static_assert(sizeof(EcuSimDiagnostics) == 216);
