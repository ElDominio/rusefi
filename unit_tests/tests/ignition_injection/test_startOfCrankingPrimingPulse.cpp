/*
 * test_startOfCrankingPrimingPulse.cpp
 *
 *  Created on: Mar 4, 2018
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#include "pch.h"
#include "util/injection_crank_helper.h"

TEST(engine, testPlainCrankingWithoutAdvancedFeatures) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setTable(config->injectionPhase, -180.0f);
	engineConfiguration->isFasterEngineSpinUpEnabled = false;
	engine->tdcMarkEnabled = false;
	setTestFuelCrankingTable(12);

	setupSimpleTestEngineWithMafAndTT_ONE_trigger(&eth);
	ASSERT_EQ( 0,  Sensor::getOrZero(SensorType::Rpm)) << "RPM=0";

	eth.fireTriggerEventsWithDuration(/* durationMs */ 200);
	// still no RPM since need to cycles measure cycle duration
	ASSERT_EQ( 0,  Sensor::getOrZero(SensorType::Rpm)) << "start-RPM#1";


	eth.fireRise(/* delayMs */ 200);
	ASSERT_EQ(300, Sensor::getOrZero(SensorType::Rpm));
	// two simultaneous injections
	ASSERT_EQ( 4,  engine->scheduler.size()) << "plain#2";

	auto const startSimultaneousInjectionAction{ action_s::make<startSimultaneousInjection>() };
	auto const endSimultaneousInjectionAction{ action_s::make<endSimultaneousInjection>((InjectionEvent*){})};

	eth.assertEvent5("sim start", 0, startSimultaneousInjectionAction, 100000 - 1625);
	// -1 because ugh floating point math
	eth.assertEvent5("sim end", 1, endSimultaneousInjectionAction, 100000 - 1);
}


TEST(priming, startScheduling) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	ASSERT_EQ( 0,  Sensor::getOrZero(SensorType::Rpm)) << "RPM=0";

	// Turn on the ignition switch!
	engine->module<PrimeController>()->onIgnitionStateChanged(true);

	ASSERT_EQ(1, engine->scheduler.size()) << "prime fuel";
}

TEST(priming, duration) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	MockInjectorModel2 injectorModel;
	engine->module<InjectorModelPrimary>().set(&injectorModel);

	for (size_t i = 0; i < efi::size(engineConfiguration->primeBins); i++) {
		engineConfiguration->primeBins[i] = i * 10;
	}

	EXPECT_CALL(injectorModel, getInjectionDuration(0.075f)).WillOnce(Return(20.0f));
	engineConfiguration->primeValues[5] = 75;	// <-- We test this point

	// With coolant temp, we should see value from mock
	Sensor::setMockValue(SensorType::Clt, 50);
	EXPECT_EQ(20, engine->module<PrimeController>()->getPrimeDuration());

	// Without coolant temp, no prime
	Sensor::resetMockValue(SensorType::Clt);
	EXPECT_EQ(0, engine->module<PrimeController>()->getPrimeDuration());
}

// With flexCranking enabled and a flex sensor present, the priming mass comes from the 2D
// coolant-vs-ethanol table (primeFlexTable) instead of the 1D primeValues curve.
TEST(priming, flexTableDuration) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	MockInjectorModel2 injectorModel;
	engine->module<InjectorModelPrimary>().set(&injectorModel);

	// Linear coolant axis; the table is flat across coolant so only the ethanol axis matters.
	for (size_t i = 0; i < efi::size(engineConfiguration->primeBins); i++) {
		engineConfiguration->primeBins[i] = i * 10;
	}

	const uint8_t ethanolBins[] = { 0, 35, 65, 100 };
	copyArray(engineConfiguration->primeFlexBins, ethanolBins);

	const uint16_t rowMassMg[] = { 100, 200, 300, 400 };	// mg per ethanol row (multiples of the 5 mg LSB)
	for (size_t row = 0; row < efi::size(engineConfiguration->primeFlexTable); row++) {
		for (size_t col = 0; col < PRIME_CURVE_COUNT; col++) {
			engineConfiguration->primeFlexTable[row][col] = rowMassMg[row];
		}
	}

	engineConfiguration->flexCranking = true;
	Sensor::setMockValue(SensorType::Clt, 50);

	// E35 -> row 1 -> 200 mg -> 0.2 g
	EXPECT_CALL(injectorModel, getInjectionDuration(::testing::FloatNear(0.2f, 1e-4f))).WillOnce(Return(22.0f));
	Sensor::setMockValue(SensorType::FuelEthanolPercent, 35);
	EXPECT_EQ(22, engine->module<PrimeController>()->getPrimeDuration());

	// E50 -> halfway between rows at 35 (200 mg) and 65 (300 mg) -> 250 mg -> 0.25 g
	EXPECT_CALL(injectorModel, getInjectionDuration(::testing::FloatNear(0.25f, 1e-4f))).WillOnce(Return(28.0f));
	Sensor::setMockValue(SensorType::FuelEthanolPercent, 50);
	EXPECT_EQ(28, engine->module<PrimeController>()->getPrimeDuration());
}

// With primeOnTriggerTeeth enabled, the prime pulse fires after the configured number of raw
// primary trigger teeth since ignition-on, instead of after a fixed delay - and only once.
TEST(priming, triggerToothScheduling) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	MockInjectorModel2 injectorModel;
	engine->module<InjectorModelPrimary>().set(&injectorModel);
	ON_CALL(injectorModel, getInjectionDuration(::testing::_)).WillByDefault(Return(2.0f));

	for (size_t i = 0; i < efi::size(engineConfiguration->primeBins); i++) {
		engineConfiguration->primeBins[i] = i * 10;
	}
	for (auto& massMg : engineConfiguration->primeValues) {
		massMg = 50; // nonzero mass so the pulse is not skipped as zero-duration
	}
	Sensor::setMockValue(SensorType::Clt, 50);

	engineConfiguration->primeOnTriggerTeeth = true;
	engineConfiguration->primingTriggerTeeth = 5;

	auto prime = engine->module<PrimeController>();

	prime->onIgnitionStateChanged(true);
	// Tooth-counted mode: nothing scheduled yet, no fixed-delay prime pending
	EXPECT_EQ(0, engine->scheduler.size());
	EXPECT_FALSE(prime->isPriming());

	for (int i = 0; i < 4; i++) {
		prime->onPrimeTriggerTooth();
	}
	EXPECT_FALSE(prime->isPriming()) << "should not fire before the Nth tooth";
	EXPECT_EQ(0, engine->scheduler.size());

	prime->onPrimeTriggerTooth(); // 5th tooth
	EXPECT_TRUE(prime->isPriming());
	EXPECT_EQ(1, engine->scheduler.size()) << "prime end event scheduled";

	// Further teeth must not re-fire a second pulse (still single-shot, like fixed-delay mode)
	prime->onPrimeTriggerTooth();
	EXPECT_EQ(1, engine->scheduler.size()) << "no duplicate firing on further teeth";
}

// Turning the ignition off before the Nth tooth arrives must disarm the pending tooth-counted
// prime so it doesn't carry over (and keep counting) into an unrelated future key cycle.
TEST(priming, triggerToothDisarmedOnIgnitionOff) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->primeOnTriggerTeeth = true;
	engineConfiguration->primingTriggerTeeth = 3;

	auto prime = engine->module<PrimeController>();

	prime->onIgnitionStateChanged(true);
	prime->onPrimeTriggerTooth();
	prime->onPrimeTriggerTooth();

	prime->onIgnitionStateChanged(false);

	// This would have been the 3rd tooth had priming stayed armed
	prime->onPrimeTriggerTooth();
	EXPECT_FALSE(prime->isPriming());
	EXPECT_EQ(0, engine->scheduler.size());
}
