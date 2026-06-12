#include "pch.h"
#include "custom_page.h"
#include "burst_knock.h"
#include "accel_enrichment.h"

// Burst Knock: on a TPS-rate transient (reuses the TPS accel-enrichment delta/threshold) pull
// ignition timing per the TPS-rate(Y) x RPM(X) table, hold it for the duration of the stab, then
// decay the pull linearly back to zero over burstKnockDecayTime.

static BurstKnock& getBurst() {
	return engine->module<BurstKnock>().unmock();
}

static TpsAccelEnrichment& getAe() {
	return engine->module<TpsAccelEnrichment>().unmock();
}

// Fill the whole retard table with a flat value so any rate/rpm cell yields the same pull.
static void setupBurst(float flatRetardDeg, float decaySec) {
	auto* d = getCustomPage();
	d->burstKnockEnabled = true;
	d->burstKnockDecayTime = decaySec;

	for (size_t i = 0; i < efi::size(d->burstKnockRpmBins); i++) {
		d->burstKnockRpmBins[i] = i * 1000; // 0..7000 rpm
	}
	for (size_t i = 0; i < efi::size(d->burstKnockTpsRateBins); i++) {
		d->burstKnockTpsRateBins[i] = i * 200; // 0..1400 %/s
	}
	for (size_t y = 0; y < efi::size(d->burstKnockTpsRateBins); y++) {
		for (size_t x = 0; x < efi::size(d->burstKnockRpmBins); x++) {
			d->burstKnockRetardTable[y][x] = flatRetardDeg;
		}
	}

	// lookback = 1s makes the table Y value (tpsRate = deltaTps / lookback) equal deltaTps.
	engineConfiguration->tpsAccelLookback = 1.0f;
	Sensor::setMockValue(SensorType::Rpm, 3000);
}

// Drive the accel-enrichment state the burst module reads, then run the slow callback.
static void feedTransient(float deltaTps, bool aboveThreshold) {
	auto& ae = getAe();
	ae.deltaTps = deltaTps;
	ae.isAboveAccelThreshold = aboveThreshold;
	getBurst().onSlowCallback();
}

TEST(BurstKnock, pullsTimingOnTransientThenDecays) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBurst(/*retard*/ 10.0f, /*decay*/ 2.0f);

	// Throttle stab above threshold -> full table pull, held.
	feedTransient(/*deltaTps*/ 300, /*above*/ true);
	EXPECT_TRUE(getBurst().isBurstKnockActive);
	EXPECT_NEAR(getBurst().getTimingRetard(), 10.0f, 0.1f);

	// Transient ends; decay begins. Halfway through (1s of 2s) -> ~5 deg.
	feedTransient(/*deltaTps*/ 0, /*above*/ false);
	eth.moveTimeForwardSec(1.0f);
	getBurst().onSlowCallback();
	EXPECT_NEAR(getBurst().getTimingRetard(), 5.0f, 0.2f);

	// Past the decay window (>2s) -> deactivates, no pull.
	eth.moveTimeForwardSec(1.5f);
	getBurst().onSlowCallback();
	EXPECT_FALSE(getBurst().isBurstKnockActive);
	EXPECT_NEAR(getBurst().getTimingRetard(), 0.0f, 1e-3);
}

TEST(BurstKnock, holdsPeakWhileTransientPersists) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBurst(10.0f, 2.0f);

	feedTransient(300, true);
	// Time passes but the stab is still in progress: the timer stays pinned, retard held at peak.
	eth.moveTimeForwardSec(5.0f);
	feedTransient(300, true);
	EXPECT_TRUE(getBurst().isBurstKnockActive);
	EXPECT_NEAR(getBurst().getTimingRetard(), 10.0f, 0.1f);
}

TEST(BurstKnock, disabledDoesNothing) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBurst(10.0f, 2.0f);
	getCustomPage()->burstKnockEnabled = false;

	feedTransient(300, true);
	EXPECT_FALSE(getBurst().isBurstKnockActive);
	EXPECT_NEAR(getBurst().getTimingRetard(), 0.0f, 1e-3);
}

TEST(BurstKnock, zeroTableCellNeverActivates) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBurst(/*retard*/ 0.0f, /*decay*/ 2.0f);

	feedTransient(300, true);
	EXPECT_FALSE(getBurst().isBurstKnockActive);
	EXPECT_NEAR(getBurst().getTimingRetard(), 0.0f, 1e-3);
}
