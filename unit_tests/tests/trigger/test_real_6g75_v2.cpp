#include "pch.h"

#include "logicdata_csv_reader.h"

extern int tooManyTeethCounter;

// EXPERIMENTAL - see the TT_36_2_1_1_V2 comment in engine_types.h,
// trigger_mitsubishi.cpp's initialize36_2_1_1_v2(), and the custom sync logic in
// TriggerDecoderBase::isSyncPoint(). TT_36_2_1_1 (the original decoder, GitHub #8827) is left
// untouched - this exercises the separate, still-experimental replacement against the same two
// real captures plus two additional real captures the user supplied later
// (6g75-nofuel-raw-idx.csv / 6g75-yesfuel-raw-idx.csv - converted from dense per-sample logic
// exports with no confirmed real sample rate for this specific pair, so RPM in those two tests
// is illustrative/plausible rather than exact).
//
// All four values here are pinned observed-current-state, not "this is correct" - there is no
// hardware validation yet, only offline replay of these captures. Compare against the fixed
// absolute-ratio-window attempt this replaced (see docs/report.md 2026-09-07): that one produced
// a continuous clean decode on the "without spark plugs" capture but never genuinely synced the
// "with spark plugs"/noisier captures at all (RPM=0 or a stale confident-looking value riding on
// blind index wrapping). This decoder produces a plausible, non-zero, comparatively stable RPM on
// all four captures, including the ones that broke every prior attempt.

TEST(real6g75v2, withoutSparkPlugs) {
    tooManyTeethCounter = 0;
    CsvReader reader(/*triggerCount*/ 1, /* vvtCount */ 0);

    reader.open("tests/trigger/resources/6g75-without-spark-crank.csv", NORMAL_ORDER, NORMAL_ORDER);

    EngineTestHelper eth(engine_type_e::TEST_ENGINE);
    setVerboseTrigger(true);

    engineConfiguration->vvtMode[0] = VVT_INACTIVE;
    eth.setTriggerType(trigger_type_e::TT_36_2_1_1_V2);

    while (reader.haveMore()) {
        reader.processLine(&eth);
    }

    EXPECT_EQ(97, tooManyTeethCounter);

    ASSERT_EQ(3, eth.recentWarnings()->getCount());
    EXPECT_EQ(ObdCode::CUSTOM_Ignition_Coil_Overcharge_2, eth.recentWarnings()->get(0).Code);
    EXPECT_EQ(ObdCode::CUSTOM_Ignition_Coil_Overcharge_3, eth.recentWarnings()->get(1).Code);
    EXPECT_EQ(ObdCode::CUSTOM_Ignition_Coil_Overcharge_4, eth.recentWarnings()->get(2).Code);

    ASSERT_NEAR(164.2, Sensor::getOrZero(SensorType::Rpm), 0.1);
}

TEST(real6g75v2, realWithSparkPlugs) {
    tooManyTeethCounter = 0;
    CsvReader reader(/*triggerCount*/ 1, /* vvtCount */ 0);

    reader.open("tests/trigger/resources/6g75-withsparkplugs-cranking.csv", NORMAL_ORDER, NORMAL_ORDER);

    EngineTestHelper eth(engine_type_e::TEST_ENGINE);
    setVerboseTrigger(true);

    engineConfiguration->vvtMode[0] = VVT_INACTIVE;
    eth.setTriggerType(trigger_type_e::TT_36_2_1_1_V2);

    while (reader.haveMore()) {
        reader.processLine(&eth);
    }

    EXPECT_EQ(77, tooManyTeethCounter);

    ASSERT_EQ(1, eth.recentWarnings()->getCount());
    EXPECT_EQ(ObdCode::CUSTOM_PRIMARY_NOT_ENOUGH_TEETH, eth.recentWarnings()->get(0).Code);

    ASSERT_NEAR(61.2, Sensor::getOrZero(SensorType::Rpm), 0.1);
}

TEST(real6g75v2, nofuel) {
    tooManyTeethCounter = 0;
    CsvReader reader(/*triggerCount*/ 1, /* vvtCount */ 0);

    reader.open("tests/trigger/resources/6g75-nofuel-raw-idx.csv", NORMAL_ORDER, NORMAL_ORDER);
    // Timestamps are raw logic-analyzer sample indices, not seconds - without a scale these are
    // (mis)interpreted as tens of thousands of real seconds between edges, overflowing internal
    // tick handling. 20kHz matches the sample rate confirmed for an earlier nofuel/yesfuel pair
    // from this same investigation (see project_6g75_trigger_sync_issue memory) - unconfirmed
    // for this specific capture, so treat RPM here as illustrative, not exact.
    reader.timestampScale = 1.0 / 20000.0;

    EngineTestHelper eth(engine_type_e::TEST_ENGINE);
    setVerboseTrigger(true);

    engineConfiguration->vvtMode[0] = VVT_INACTIVE;
    eth.setTriggerType(trigger_type_e::TT_36_2_1_1_V2);

    while (reader.haveMore()) {
        reader.processLine(&eth);
    }

    EXPECT_EQ(139, tooManyTeethCounter);

    ASSERT_EQ(2, eth.recentWarnings()->getCount());
    EXPECT_EQ(ObdCode::CUSTOM_OUT_OF_ORDER_COIL, eth.recentWarnings()->get(0).Code);
    EXPECT_EQ(ObdCode::CUSTOM_PRIMARY_NOT_ENOUGH_TEETH, eth.recentWarnings()->get(1).Code);

    ASSERT_NEAR(61.7, Sensor::getOrZero(SensorType::Rpm), 0.1);
}

TEST(real6g75v2, yesfuel) {
    tooManyTeethCounter = 0;
    CsvReader reader(/*triggerCount*/ 1, /* vvtCount */ 0);

    reader.open("tests/trigger/resources/6g75-yesfuel-raw-idx.csv", NORMAL_ORDER, NORMAL_ORDER);
    // See the comment on the same line in the nofuel test above.
    reader.timestampScale = 1.0 / 20000.0;

    EngineTestHelper eth(engine_type_e::TEST_ENGINE);
    setVerboseTrigger(true);

    engineConfiguration->vvtMode[0] = VVT_INACTIVE;
    eth.setTriggerType(trigger_type_e::TT_36_2_1_1_V2);

    while (reader.haveMore()) {
        reader.processLine(&eth);
    }

    EXPECT_EQ(146, tooManyTeethCounter);

    ASSERT_EQ(3, eth.recentWarnings()->getCount());
    EXPECT_EQ(ObdCode::CUSTOM_OUT_OF_ORDER_COIL, eth.recentWarnings()->get(0).Code);
    EXPECT_EQ(ObdCode::CUSTOM_PRIMARY_NOT_ENOUGH_TEETH, eth.recentWarnings()->get(1).Code);
    EXPECT_EQ(ObdCode::CUSTOM_PRIMARY_TOO_MANY_TEETH, eth.recentWarnings()->get(2).Code);

    ASSERT_NEAR(116.5, Sensor::getOrZero(SensorType::Rpm), 0.1);
}
