#include "pch.h"
#include "traction_control.h"

void TractionControlController::init() {
	tcEtbDropTable.initTable(engineConfiguration->tractionControlEtbDrop, engineConfiguration->tractionControlSlipBins, engineConfiguration->tractionControlSpeedBins);
	tcTimingDropTable.initTable(engineConfiguration->tractionControlTimingDrop, engineConfiguration->tractionControlSlipBins, engineConfiguration->tractionControlSpeedBins);
	tcSparkSkipTable.initTable(engineConfiguration->tractionControlIgnitionSkip, engineConfiguration->tractionControlSlipBins, engineConfiguration->tractionControlSpeedBins);

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
	float rawEtbDrop = tcEtbDropTable.getValue(yAxisValue, vehicleSpeed);
	float rawTimingDrop = tcTimingDropTable.getValue(yAxisValue, vehicleSpeed);
	float rawSparkSkip = tcSparkSkipTable.getValue(yAxisValue, vehicleSpeed) / 100.0f;

	float multiplier = 1.0f;
	if (engineConfiguration->tractionControlUseLuaGauge) {
		SensorType luaGaugeSensor = SensorType(int(SensorType::LuaGauge1) + engineConfiguration->tractionControlLuaGauge);
		float luaValue = Sensor::getOrZero(luaGaugeSensor);
		multiplier = interpolate2d(luaValue, engineConfiguration->tractionControlLuaMultBins, engineConfiguration->tractionControlLuaMultValues);
	}

	rawEtbDrop *= multiplier;
	rawTimingDrop *= multiplier;
	rawSparkSkip *= multiplier;

	// ETB and timing corrections only ever remove throttle/advance, never add them: convert the
	// positive table magnitude to a subtractive correction here, and clamp so a negative multiplier
	// (or any other upstream sign mistake) can never flip these positive and add throttle/timing.
	rawEtbDrop = minF(-rawEtbDrop, 0.0f);
	rawTimingDrop = minF(-rawTimingDrop, 0.0f);

	bool isTractionActive = (rawEtbDrop != 0.0f) || (rawTimingDrop != 0.0f) || (rawSparkSkip != 0.0f);

	if (isTractionActive) {
		// If wheel slip (Y-axis) increases, reset hold timer and update held values
		if (wheelSlip > lastWheelSlip) {
			holdTimer = (float)engineConfiguration->tractionControlHoldTime / 1000.0f; // ms to seconds
			heldEtbDrop = rawEtbDrop;
			heldTimingDrop = rawTimingDrop;
			heldSparkSkip = rawSparkSkip;
			decayTimer = 0.0f;
		}
	}

	lastWheelSlip = wheelSlip;

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
