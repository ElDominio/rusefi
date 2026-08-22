#include "pch.h"
#include "traction_control.h"

void TractionControlController::init() {
	// columnBins must match TColNum (SPEED_SIZE), rowBins must match TRowNum (SLIP_SIZE) -- see the
	// Map3D declarations in traction_control.h.
	tcEtbDropTable.initTable(engineConfiguration->tractionControlEtbDrop, engineConfiguration->tractionControlSpeedBins, engineConfiguration->tractionControlSlipBins);
	tcTimingDropTable.initTable(engineConfiguration->tractionControlTimingDrop, engineConfiguration->tractionControlSpeedBins, engineConfiguration->tractionControlSlipBins);
	tcSparkSkipTable.initTable(engineConfiguration->tractionControlIgnitionSkip, engineConfiguration->tractionControlSpeedBins, engineConfiguration->tractionControlSlipBins);

	tractionTimer.reset(getTimeNowNt());
}

void TractionControlController::update() {
	efitick_t nowNt = getTimeNowNt();
	float dt = tractionTimer.getElapsedSecondsAndReset(nowNt);
	// Clamp dt to 100ms max to prevent large spikes at startup or after code pauses
	dt = clampF(0.0f, dt, 0.1f);

	float vehicleSpeed = Sensor::getOrZero(SensorType::VehicleSpeed);
	float wheelSlip = Sensor::getOrZero(SensorType::WheelSlipRatio);

	float yAxisValue;
	if (engineConfiguration->tractionControlYAxisSource == 0) {
		yAxisValue = wheelSlip;
	} else {
		yAxisValue = engine->rpmCalculator.getRpmAcceleration() / 100.0f;
	}
	engine->engineState.tractionControlYAxisValue = yAxisValue;

	// Tables store positive magnitudes (% throttle removed, degrees retarded, % sparks skipped).
	// getValue(xColumn, yRow): xColumn pairs with the speed bins (TColNum), yRow with the slip
	// bins (TRowNum) -- see the Map3D declarations in traction_control.h.
	float rawEtbDrop = tcEtbDropTable.getValue(vehicleSpeed, yAxisValue);
	float rawTimingDrop = tcTimingDropTable.getValue(vehicleSpeed, yAxisValue);
	float rawSparkSkip = tcSparkSkipTable.getValue(vehicleSpeed, yAxisValue) / 100.0f;

	float multiplier = 1.0f;
	if (engineConfiguration->tractionControlUseLuaGauge) {
		SensorType luaGaugeSensor = SensorType(int(SensorType::LuaGauge1) + engineConfiguration->tractionControlLuaGauge);
		float luaValue = Sensor::getOrZero(luaGaugeSensor);
		multiplier = interpolate2d(luaValue, engineConfiguration->tractionControlLuaMultBins, engineConfiguration->tractionControlLuaMultValues);
	}

	rawEtbDrop *= multiplier;
	rawTimingDrop *= multiplier;
	rawSparkSkip *= multiplier;

	// Below the configured minimum vehicle speed or minimum driver demand (accelerator pedal),
	// traction control is fully disabled -- treat the table lookup as zero so the existing
	// hold/decay state machine smoothly relaxes any already-applied correction rather than
	// snapping it off. 0 disables the respective gate (preserves old-tune behavior).
	float driverDemand = Sensor::getOrZero(SensorType::AcceleratorPedal);
	bool belowMinVss = (engineConfiguration->tractionControlMinVss != 0) && (vehicleSpeed < engineConfiguration->tractionControlMinVss);
	bool belowMinDriverDemand = (engineConfiguration->tractionControlMinDriverDemand != 0) && (driverDemand < engineConfiguration->tractionControlMinDriverDemand);
	if (belowMinVss || belowMinDriverDemand) {
		rawEtbDrop = 0.0f;
		rawTimingDrop = 0.0f;
		rawSparkSkip = 0.0f;
	}

	// ETB and timing corrections only ever remove throttle/advance, never add them: convert the
	// positive table magnitude to a subtractive correction here, and clamp so a negative multiplier
	// (or any other upstream sign mistake) can never flip these positive and add throttle/timing.
	rawEtbDrop = minF(-rawEtbDrop, 0.0f);
	rawTimingDrop = minF(-rawTimingDrop, 0.0f);

	bool isTractionActive = (rawEtbDrop != 0.0f) || (rawTimingDrop != 0.0f) || (rawSparkSkip != 0.0f);

	// Re-check "is slip increasing" only every tractionControlSlipCheckRateMs rather than every
	// fast-loop tick: WheelSlipRatio is noisy at 5ms resolution, so comparing tick-to-tick makes the
	// hold trigger re-arm on single-sample jitter instead of a real trend. Floored to the fast-loop
	// period, so a value below that (including 0, from an older tune) reproduces the original
	// every-tick behavior.
	slipCheckTimer -= dt;
	if (slipCheckTimer <= 0.0f) {
		float checkRateMs = engineConfiguration->tractionControlSlipCheckRateMs;
		if (checkRateMs < FAST_CALLBACK_PERIOD_MS) {
			checkRateMs = FAST_CALLBACK_PERIOD_MS;
		}
		slipCheckTimer = checkRateMs / 1000.0f;

		// If wheel slip (Y-axis) increased since the last check, reset hold timer and update held values
		if (isTractionActive && wheelSlip > lastCheckedWheelSlip) {
			holdTimer = (float)engineConfiguration->tractionControlHoldTime / 1000.0f; // ms to seconds
			heldEtbDrop = rawEtbDrop;
			heldTimingDrop = rawTimingDrop;
			heldSparkSkip = rawSparkSkip;
			decayTimer = 0.0f;
		}

		lastCheckedWheelSlip = wheelSlip;
	}

	float targetEtb = rawEtbDrop;
	float targetTiming = rawTimingDrop;
	float targetSpark = rawSparkSkip;

	float timeForDecay = 0.0f;
	if (holdTimer > 0.0f) {
		if (dt >= holdTimer) {
			timeForDecay = dt - holdTimer;
			holdTimer = 0.0f;
		} else {
			holdTimer -= dt;
			timeForDecay = 0.0f;
			appliedEtbDrop = heldEtbDrop;
			appliedTimingDrop = heldTimingDrop;
			appliedSparkSkip = heldSparkSkip;
			decayTimer = 0.0f;
		}
	} else {
		timeForDecay = dt;
	}

	if (holdTimer <= 0.0f) {
		float decayTimeSec = (float)engineConfiguration->tractionControlDecayTime / 1000.0f;
		if (decayTimeSec > 0.0f) {
			decayTimer += timeForDecay;
			float ratio = clampF(0.0f, decayTimer / decayTimeSec, 1.0f);
			appliedEtbDrop = heldEtbDrop + (targetEtb - heldEtbDrop) * ratio;
			appliedTimingDrop = heldTimingDrop + (targetTiming - heldTimingDrop) * ratio;
			appliedSparkSkip = heldSparkSkip + (targetSpark - heldSparkSkip) * ratio;

			if (ratio >= 1.0f) {
				heldEtbDrop = targetEtb;
				heldTimingDrop = targetTiming;
				heldSparkSkip = targetSpark;
			}
		} else {
			appliedEtbDrop = targetEtb;
			appliedTimingDrop = targetTiming;
			appliedSparkSkip = targetSpark;
			heldEtbDrop = targetEtb;
			heldTimingDrop = targetTiming;
			heldSparkSkip = targetSpark;
			decayTimer = 0.0f;
		}
	}
}
