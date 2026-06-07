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

private:
	Map3D<TRACTION_CONTROL_ETB_DROP_SLIP_SIZE, TRACTION_CONTROL_ETB_DROP_SPEED_SIZE, int8_t, uint16_t, uint8_t> tcTimingDropTable{"tct"};
	Map3D<TRACTION_CONTROL_ETB_DROP_SLIP_SIZE, TRACTION_CONTROL_ETB_DROP_SPEED_SIZE, int8_t, uint16_t, uint8_t> tcSparkSkipTable{"tcs"};
	Map3D<TRACTION_CONTROL_ETB_DROP_SLIP_SIZE, TRACTION_CONTROL_ETB_DROP_SPEED_SIZE, int8_t, uint16_t, uint8_t> tcEtbDropTable{"tce"};

	Timer tractionTimer;
	float holdTimer = 0.0f;
	float decayTimer = 0.0f;
	float lastWheelSlip = 0.0f;

	float appliedEtbDrop = 0.0f;
	float appliedTimingDrop = 0.0f;
	float appliedSparkSkip = 0.0f;

	float heldEtbDrop = 0.0f;
	float heldTimingDrop = 0.0f;
	float heldSparkSkip = 0.0f;
};
