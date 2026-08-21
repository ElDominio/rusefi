#pragma once

#include "table_helper.h"
#include "rusefi/timer.h"

class TractionControlController {
public:
	void init();
	void update();

	float getAppliedEtbDrop() const { return appliedEtbDrop; }
	float getAppliedTimingDrop() const { return appliedTimingDrop; }
	float getAppliedSparkSkip() const { return appliedSparkSkip; }

	// True whenever traction control is actively pulling torque (a non-zero ETB,
	// timing, or spark-skip correction is being applied). Consumed by the Engine
	// State Machine to raise its Torque Reduction overlay.
	bool isActive() const {
		return appliedEtbDrop != 0.0f || appliedTimingDrop != 0.0f || appliedSparkSkip != 0.0f;
	}

private:
	// TColNum, TRowNum here follow the struct dimension order in rusefi_config.txt:
	// [SLIP_SIZE x SPEED_SIZE], i.e. table[TRowNum=SLIP_SIZE][TColNum=SPEED_SIZE] --
	// matches interpolate3d's documented "TS defines tables as [y_row_count x x_column_count]"
	// convention, since yBins=tractionControlSlipBins and xBins=tractionControlSpeedBins.
	Map3D<TRACTION_CONTROL_ETB_DROP_SPEED_SIZE, TRACTION_CONTROL_ETB_DROP_SLIP_SIZE, int8_t, uint8_t, uint16_t> tcTimingDropTable{"tct"};
	Map3D<TRACTION_CONTROL_ETB_DROP_SPEED_SIZE, TRACTION_CONTROL_ETB_DROP_SLIP_SIZE, int8_t, uint8_t, uint16_t> tcSparkSkipTable{"tcs"};
	Map3D<TRACTION_CONTROL_ETB_DROP_SPEED_SIZE, TRACTION_CONTROL_ETB_DROP_SLIP_SIZE, int8_t, uint8_t, uint16_t> tcEtbDropTable{"tce"};

	Timer tractionTimer;
	float holdTimer = 0.0f;
	float decayTimer = 0.0f;
	// Slip value as of the last "is slip increasing" check, and a countdown to the next check --
	// see tractionControlSlipCheckRateMs. Re-checking every fast-loop tick makes the hold trigger
	// oversensitive to sensor noise; both default to 0 so an unset/legacy tune re-checks every tick
	// (floored to FAST_CALLBACK_PERIOD_MS), reproducing the original behavior.
	float lastCheckedWheelSlip = 0.0f;
	float slipCheckTimer = 0.0f;

	float appliedEtbDrop = 0.0f;
	float appliedTimingDrop = 0.0f;
	float appliedSparkSkip = 0.0f;

	float heldEtbDrop = 0.0f;
	float heldTimingDrop = 0.0f;
	float heldSparkSkip = 0.0f;
};
