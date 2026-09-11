#include "pch.h"

#include "logicdata_csv_reader.h"

// EXPERIMENTAL - see the VVT_MITSUBISHI_6G75_BETA comment in rusefi_enums.h and
// TriggerCentral::tryMitsu6g75BetaSync()/mitsu6g75BetaObserveCamEdge() in trigger_central.cpp.
//
// VVT_MITSUBISHI_6G75 (the amplitude/gap-ratio cam decoder) is broken on real hardware once
// crank sync is stable (docs/report.md 2026-09-08): it throws CUSTOM_CAM_TOO_MANY_TEETH and
// produces a nonsensical VVT position. BETA never runs that decoder at all - instead it counts
// real cam rise edges per crank revolution, which this capture shows lands cleanly on 3 or 4
// (alternating), never anything else, to resolve 360-vs-720 phase.
//
// IMPORTANT: this test only verifies the counting/classification MECHANISM is clean and that
// full phase sync is achieved - it does NOT verify which physical crank revolution (remainder 0
// vs 1) is correct, since this pure-trigger capture carries no TDC/cylinder-1 ground truth. See
// the big comment above TriggerCentral::mitsu6g75BetaObserveCamEdge() for the required bench
// verification before trusting this for sequential injection/ignition.
TEST(real6g75Beta, camCountSyncsPhase) {
    CsvReader reader(/*triggerCount*/ 1, /* vvtCount */ 1);

    reader.open("tests/trigger/resources/6g75-without-spark-crank.csv", NORMAL_ORDER, NORMAL_ORDER);

    EngineTestHelper eth(engine_type_e::TEST_ENGINE);

    engineConfiguration->vvtMode[0] = VVT_MITSUBISHI_6G75_BETA;
    eth.setTriggerType(trigger_type_e::TT_36_2_1_1_V2);

    TriggerCentral *tc = &engine->triggerCentral;

    while (reader.haveMore()) {
        reader.processLine(&eth);
    }

    // The capture holds ~7 clean crank revolutions after crank sync stabilizes (docs/report.md
    // 2026-09-08 analysis: cam rise counts alternate 3,4,3,4,3,4,3) - full sequential-capable
    // phase sync (hasSynchronizedPhase(), not just hasProvisionalPhase()) should be reached well
    // before the capture ends.
    EXPECT_TRUE(tc->triggerState.hasSynchronizedPhase())
        << "expected full phase sync via VVT_MITSUBISHI_6G75_BETA's cam-edge-count path";

    // Confirms VVT_MITSUBISHI_6G75_BETA never invoked the amplitude/gap-ratio cam decoder (which
    // this same capture would otherwise not exercise meaningfully anyway - the wheel geometry
    // just isn't excited the same way - but the counters must stay untouched either way).
    EXPECT_EQ(0, tc->vvtState[0][0].totalTriggerErrorCounter);
    EXPECT_FALSE(tc->vvtState[0][0].getShaftSynchronized())
        << "the real (amplitude) cam decoder must never run for VVT_MITSUBISHI_6G75_BETA";
}
