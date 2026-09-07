#include "pch.h"

#include "logicdata_csv_reader.h"

static void prepare(EngineTestHelper *eth, trigger_type_e trigger) {

	engineConfiguration->isFasterEngineSpinUpEnabled = true;
	engineConfiguration->rpmUpdateMode = rpmUpdateMode_e::RPM_UPDATE_INSTANT;
	engineConfiguration->invertPrimaryTriggerSignal = false;

	engineConfiguration->isPhaseSyncRequiredForIgnition = true;

    eth->setTriggerType(trigger);

    ASSERT_FALSE(engine->triggerCentral.triggerShape.shapeDefinitionError);

	engineConfiguration->vvtMode[0] = VVT_INACTIVE;
}

static void constructTriggerFromRecording(CsvReader *reader) {
	int magic = 20;

	if (reader->lineIndex() == magic) {

		int len = 8;
		double last = reader->history.get(magic - 1);
		printf("last=%f\n", last);

		double time360 = last - reader->history.get(magic - 1 - 8);

		for (int i=len - 1;i>=0;i--) {
			double tooth = last - reader->history.get(magic - 1 - i);
//				printf("index=%d width=%f\n", i, tooth);
			double angle = 360 - (360 * tooth / time360);
			//printf("index=%d, to=%f\n", i, angle);

			bool isRise = (i % 2) == 1;
			const char * front = isRise ? "RISE" : "FALL";

			printf("\ts->addEvent360(%f, TriggerValue::%s);\n", angle, front);
		}

//		printf("time360=%f\n", time360);
	}
}

// only referenced by the currently-disabled (commented-out) real6g72 tests below
[[maybe_unused]] static void runTriggerTest(const char *fileName, uint32_t totalErrors, int syncCounter, float firstRpm) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	prepare(&eth, trigger_type_e::TT_VVT_MITSU_6G72);

	bool gotRpm = false;

	CsvReader reader(/*trigger channels count*/1, /* vvtCount */ 0);
	reader.flipOnRead = true;

	reader.open(fileName);
	while (reader.haveMore()) {
		reader.processLine(&eth);

		constructTriggerFromRecording(&reader);


		auto rpm = Sensor::getOrZero(SensorType::Rpm);
//		printf("rpm %f\n", rpm);

		if (!gotRpm && rpm) {
			gotRpm = true;

//			EXPECT_EQ(eventCount, 7);
			EXPECT_NEAR(rpm, firstRpm, 0.1);
		}
	}
	ASSERT_TRUE(gotRpm);
	ASSERT_EQ(totalErrors,  engine->triggerCentral.triggerState.totalTriggerErrorCounter);
	ASSERT_EQ(syncCounter,  engine->triggerCentral.triggerState.getSynchronizationCounter());
}


/*
TEST(real6g72, data1) {
	runTriggerTest("tests/trigger/resources/3000gt.csv", 0, 15, 195.515f);
}

TEST(real6g72, data2) {
	runTriggerTest("tests/trigger/resources/3000gt_cranking_cam_first_crank_second_only_cam.csv", 2, 9, 157.843f);
}
*/

void generateLog(const char* filename, trigger_type_e crankTriggerType = trigger_type_e::TT_3_TOOTH_CRANK) {
    CsvReader reader(/*triggerCount*/ 1, /* vvtCount */ 1);

    reader.open(filename, NORMAL_ORDER, NORMAL_ORDER);

    EngineTestHelper eth(engine_type_e::TEST_ENGINE);
//    setVerboseTrigger(true);

    engineConfiguration->vvtMode[0] = vvt_mode_e::VVT_MITSUBISHI_6G72;
    eth.setTriggerType(crankTriggerType);

    engineConfiguration->globalTriggerAngleOffset = 125;
    engineConfiguration->isFasterEngineSpinUpEnabled = true;
    engineConfiguration->rpmUpdateMode = rpmUpdateMode_e::RPM_UPDATE_INSTANT;
    engineConfiguration->isPhaseSyncRequiredForIgnition = true;

    int n = 0;
    bool firstRpm = false;
    while (reader.haveMore()) {
        reader.processLine(&eth);
        auto rpm = Sensor::getOrZero(SensorType::Rpm);
        if ((rpm) && (!firstRpm)) {
            printf("Got first RPM %f, at %d\n", rpm, n);
            firstRpm = true;
        }
        n++;
    }
}

TEST(real6g72, sync_3000gt_cranking_rusefi) {
    generateLog("tests/trigger/resources/3000gt_cranking_rusefi.csv");
}

TEST(real6g72, sync_3000gt_cranking_rusefi_2) {
    generateLog("tests/trigger/resources/3000gt_cranking_rusefi_2.csv");
}

TEST(real6g72, sync_3000gt_crank_cam_cranking) {
    generateLog("tests/trigger/resources/3000gt_crank_cam_cranking.csv");
}

TEST(real6g72, sync_3000gt_crank_cam_cranking_2) {
    generateLog("tests/trigger/resources/3000gt_crank_cam_cranking_2.csv");
}

TEST(real6g72, sync_3000gt_crank_cam_cranking_idle) {
    generateLog("tests/trigger/resources/3000gt_crank_cam_cranking_idle.csv");
}

// Sanity check for TT_6G72_CRANK (SyncEdge::Rise variant of TT_3_TOOTH_CRANK, both edges
// reach the decoder for finer angle/RPM tracking) against real captures, paired with the
// existing, unmodified TT_VVT_MITSU_6G72 cam decoder. See docs/mitsubishi-6g72-fast-crank-cam-sync.md
TEST(real6g72, sync_3000gt_cranking_rusefi_6g72CrankType) {
    generateLog("tests/trigger/resources/3000gt_cranking_rusefi.csv", trigger_type_e::TT_6G72_CRANK);
}

TEST(real6g72, sync_3000gt_crank_cam_cranking_idle_6g72CrankType) {
    generateLog("tests/trigger/resources/3000gt_crank_cam_cranking_idle.csv", trigger_type_e::TT_6G72_CRANK);
}

// VVT_MITSUBISHI_6G72_BETA: checks that the fast crank-edge/cam-level path reaches
// hasProvisionalPhase() no later than (and typically before) the existing slow gap-decoder
// reaches the confirmed hasSynchronizedPhase(), and that the slow decoder still eventually
// confirms (i.e. the fast path never blocks or breaks the existing confirmation path).
// See docs/mitsubishi-6g72-fast-crank-cam-sync.md
static void checkBetaProvisionalTiming(const char* filename) {
    CsvReader reader(/*triggerCount*/ 1, /* vvtCount */ 1);
    reader.open(filename, NORMAL_ORDER, NORMAL_ORDER);

    EngineTestHelper eth(engine_type_e::TEST_ENGINE);

    engineConfiguration->vvtMode[0] = vvt_mode_e::VVT_MITSUBISHI_6G72_BETA;
    eth.setTriggerType(trigger_type_e::TT_6G72_CRANK);

    engineConfiguration->globalTriggerAngleOffset = 125;
    engineConfiguration->isFasterEngineSpinUpEnabled = true;
    engineConfiguration->rpmUpdateMode = rpmUpdateMode_e::RPM_UPDATE_INSTANT;
    engineConfiguration->isPhaseSyncRequiredForIgnition = true;

    int n = 0;
    int provisionalAt = -1;
    int confirmedAt = -1;
    while (reader.haveMore()) {
        reader.processLine(&eth);
        auto& triggerState = engine->triggerCentral.triggerState;
        if (provisionalAt < 0 && triggerState.hasProvisionalPhase()) {
            provisionalAt = n;
        }
        if (confirmedAt < 0 && triggerState.hasSynchronizedPhase()) {
            confirmedAt = n;
        }
        n++;
    }

    printf("%s: provisionalAt=%d confirmedAt=%d\n", filename, provisionalAt, confirmedAt);

    ASSERT_NE(confirmedAt, -1) << "slow decoder never confirmed - regression";
    ASSERT_NE(provisionalAt, -1) << "fast path never provisionally synced";
    ASSERT_LE(provisionalAt, confirmedAt) << "fast path should never be slower than the slow decoder";
}

TEST(real6g72, beta_cranking_rusefi) {
    checkBetaProvisionalTiming("tests/trigger/resources/3000gt_cranking_rusefi.csv");
}
TEST(real6g72, beta_cranking_rusefi_2) {
    checkBetaProvisionalTiming("tests/trigger/resources/3000gt_cranking_rusefi_2.csv");
}
TEST(real6g72, beta_crank_cam_cranking) {
    checkBetaProvisionalTiming("tests/trigger/resources/3000gt_crank_cam_cranking.csv");
}
TEST(real6g72, beta_crank_cam_cranking_2) {
    checkBetaProvisionalTiming("tests/trigger/resources/3000gt_crank_cam_cranking_2.csv");
}
TEST(real6g72, beta_crank_cam_cranking_idle) {
    checkBetaProvisionalTiming("tests/trigger/resources/3000gt_crank_cam_cranking_idle.csv");
}
