#pragma once
/*
 * Shared sensor frame: the in-process ECU's plant -> firmware sensor memory
 * (Docs/INPROCESS-ECU-SHARED-MEMORY-PLAN.md phase 2 in the dyno game tree).
 *
 * The DLL owns one frame (ecu_sensor_frame()). The plant resolves each input
 * name to a slot ONCE (ecu_sensor_slot(name)), then per commit writes
 * value[slot] and sets bit slot in `dirty`; ecu_commit() validates and applies
 * the dirty slots through the pre-resolved routes (same validation, NaN release,
 * +/-inf wiring faults and redundancy rules as the string mock records) and
 * clears `dirty`. No names, strings or parsing on the hot path.
 *
 * Single producer: the plant writes the frame and calls ecu_commit() on the same
 * thread, so no lock or seqlock is needed. A rejected commit (-1) applies nothing
 * and still clears `dirty`; the plant re-sends.
 *
 * ABI: the layout is copied verbatim into the plant (engine-sim
 * include/ecu_sensor_frame.h). Change it only together with ECU_SENSOR_FRAME_ABI.
 */
#include <stdint.h>

#define ECU_SENSOR_FRAME_ABI 1
#define ECU_SENSOR_FRAME_SLOTS 64

struct EcuSensorFrame {
	uint32_t abi;       // ECU_SENSOR_FRAME_ABI
	uint32_t slots;     // ECU_SENSOR_FRAME_SLOTS
	uint64_t dirty;     // bit i: value[i] written since the last ecu_commit
	float value[ECU_SENSOR_FRAME_SLOTS];
};

static_assert(sizeof(EcuSensorFrame) == 16 + 4 * ECU_SENSOR_FRAME_SLOTS, "EcuSensorFrame layout is ABI");
