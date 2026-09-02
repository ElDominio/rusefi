/**
 * @file can_etb_remote.cpp
 *
 * The "remote ETB" side of the external CH32V203 ETB controller integration (see can_etb.h and
 * external-etb/RUSEFI_SIDE_TODO.md #3.1/#3.2). Two independent mechanisms live here, both gated by
 * engineConfiguration->enableExternalCanEtb:
 *
 *  - #3.1 normal closed-loop operation: sendExternalEtbGains()/sendExternalEtbTarget(), called
 *    periodically from can_tx.cpp's CanWrite::PeriodicTask(). Reads gains from
 *    engineConfiguration->etb and the final blended target from
 *    engine->etbControllers[0]->getSetpoint() (exposed on IEtbController for exactly this) -
 *    deliberately bypasses EtbController/ClosedLoopController entirely, per #1. One broadcast:
 *    a dual-throttle-body engine using this mode is two physical CAN ETB boards on the same
 *    bus/ID, both mirroring throttle 1's fully-computed target (idle, traction control, sport
 *    pedal, antilag, eco mode, quick warmup, Lua, rev limit are all still applied - only the
 *    per-throttle trim/split and local hardware drive are dropped, see electronic_throttle.cpp).
 *  - #3.2 bench-test/auto-calibrate: CanDcMotor, wired in as EtbController::init()'s m_motor for
 *    every configured throttle (electronic_throttle.cpp, both point at the same shared instance)
 *    - startBenchTest()/doAutocal() (electronic_throttle_impl.h) drive it via the ordinary DcMotor
 *    interface, unmodified, from whichever throttle's own TS button/command triggered it.
 *
 * Mutual exclusion between the two (RUSEFI_SIDE_TODO.md #6): sendExternalEtbTarget() checks every
 * configured throttle's isAutocalOrBenchTestActive() and stays off the wire while any of them is
 * driving ETB_TARGET itself - EtbImpl::update() (electronic_throttle_impl.h) already guarantees
 * the normal tick that would otherwise want to send a Normal frame never runs on that throttle at
 * the same time as its own autocal/bench-test phase.
 */

#include "pch.h"
#include "can_etb.h"

#if EFI_CAN_SUPPORT && EFI_EXTERNAL_CAN_ETB
#include "can_msg_tx.h"
#include "electronic_throttle.h"

size_t getExternalEtbBus() {
	return (size_t)engineConfiguration->canEtbBusIndex;
}

// Wire-format structs, matching external-etb/firmware/src/can_bus.c's handle_frame() byte-for-byte
// (native little-endian raw float/int reads via memcpy on both ends - see can_etb.h's offsets).
struct EtbCanGains1Frame {
	float pFactor;
	float iFactor;
};
struct EtbCanGains2Frame {
	float dFactor;
	float offset;
};
struct EtbCanTargetFrame {
	float targetPosition;
	int16_t benchDutyRaw;
	uint8_t mode;
	uint8_t sequence;
};
struct EtbCanCalFrame {
	uint16_t min1;
	uint16_t max1;
	uint16_t min2;
	uint16_t max2;
};

static_assert(sizeof(EtbCanGains1Frame) == 8);
static_assert(sizeof(EtbCanGains2Frame) == 8);
static_assert(sizeof(EtbCanTargetFrame) == 8);
static_assert(sizeof(EtbCanCalFrame) == 8);

CanDcMotor externalEtbCanMotor;

void CanDcMotor::sendTarget() {
	CanTxTyped<EtbCanTargetFrame> m(CanCategory::ETB, CAN_ID_ETB_TARGET, false, getExternalEtbBus());
	m->targetPosition = 0; // unused when mode == OpenLoop
	m->benchDutyRaw = (int16_t)(m_duty * 10000.0f);
	m->mode = (uint8_t)EtbCanMode::OpenLoop;
	m->sequence = m_seq++;
}

bool CanDcMotor::set(float duty) {
	m_duty = duty;
	sendTarget();
	// No CAN-reported fault plumbed back (yet) - the board's own staleness fail-safe (board doc
	// #5.6) is the safety backstop here, not this return value.
	return false;
}

void CanDcMotor::enable() {
	// set() already transmits the command that puts the board in OpenLoop mode; nothing else to do.
}

void CanDcMotor::disable(const char* /*msg*/) {
	m_duty = 0;
	sendTarget();
}

void sendExternalEtbGains() {
	{
		CanTxTyped<EtbCanGains1Frame> m(CanCategory::ETB, CAN_ID_ETB_GAINS_1, false, getExternalEtbBus());
		m->pFactor = engineConfiguration->etb.pFactor;
		m->iFactor = engineConfiguration->etb.iFactor;
	}
	{
		CanTxTyped<EtbCanGains2Frame> m(CanCategory::ETB, CAN_ID_ETB_GAINS_2, false, getExternalEtbBus());
		m->dFactor = engineConfiguration->etb.dFactor;
		m->offset = engineConfiguration->etb.offset;
	}
}

void sendExternalEtbTarget() {
	auto controller = engine->etbControllers[0];
	if (!controller || !controller->isEtbMode()) {
		return;
	}

	// Both DC_Throttle1 and DC_Throttle2 (if configured) share this one externalEtbCanMotor -
	// bench-test/autocal can be triggered per-throttle (separate TS buttons/commands), and either
	// one owns ETB_TARGET directly while active, so check every configured slot, not just
	// throttle 1, to avoid racing an OpenLoop frame from whichever one is running (#6). Same
	// reasoning for the fault check: checkStatus() ran for each configured throttle this tick.
	for (int i = 0; i < ETB_COUNT; i++) {
		auto etb = engine->etbControllers[i];
		if (!etb || !etb->isEtbMode()) {
			continue;
		}
		if (etb->isAutocalOrBenchTestActive() || etb->isEtbFaulted()) {
			// checkStatus() (electronic_throttle.cpp) already ran this tick and populated this -
			// mirrors what a local throttle's own m_motor->disable("etb status") would do, except
			// there's no "disable" to send: going quiet lets the board's staleness watchdog take
			// over (can_bus.h).
			return;
		}
	}

	if (engineConfiguration->pauseEtbControl) {
		return;
	}

	auto target = controller->getSetpoint();
	// No valid setpoint this tick (e.g. no pedal map configured yet, same condition
	// getSetpointEtb() itself returns unexpected for) - stay off the wire rather than guess at 0%,
	// same "silence is the signal" reasoning as the fault/pause check above.
	if (!target) {
		return;
	}

	static uint8_t targetSeq = 0;

	// Same gate the local relay-autotune uses (checkStatus()'s m_isAutotune,
	// electronic_throttle.cpp) - only while the engine is off, never while driving. See
	// EtbCanMode::Autotune's comment (can_etb.h) - board-side not yet implemented.
	bool autotuneRequested = engine->etbAutoTune && Sensor::getOrZero(SensorType::Rpm) == 0;

	CanTxTyped<EtbCanTargetFrame> m(CanCategory::ETB, CAN_ID_ETB_TARGET, false, getExternalEtbBus());
	m->targetPosition = target.Value;
	m->benchDutyRaw = 0; // unused when mode == Normal or Autotune
	m->mode = (uint8_t)(autotuneRequested ? EtbCanMode::Autotune : EtbCanMode::Normal);
	m->sequence = targetSeq++;
}

void sendExternalEtbCalTps(uint16_t rawMin1, uint16_t rawMax1, uint16_t rawMin2, uint16_t rawMax2) {
	CanTxTyped<EtbCanCalFrame> m(CanCategory::ETB, CAN_ID_ETB_CAL_TPS, false, getExternalEtbBus());
	m->min1 = rawMin1;
	m->max1 = rawMax1;
	m->min2 = rawMin2;
	m->max2 = rawMax2;
}

void sendExternalEtbCalPedal(uint16_t rawMin1, uint16_t rawMax1, uint16_t rawMin2, uint16_t rawMax2) {
	CanTxTyped<EtbCanCalFrame> m(CanCategory::ETB, CAN_ID_ETB_CAL_PEDAL, false, getExternalEtbBus());
	m->min1 = rawMin1;
	m->max1 = rawMax1;
	m->min2 = rawMin2;
	m->max2 = rawMax2;
}

void sendExternalEtbCalibration() {
	sendExternalEtbCalTps(engineConfiguration->canEtbTps1RawMin, engineConfiguration->canEtbTps1RawMax,
		engineConfiguration->canEtbTpsBRawMin, engineConfiguration->canEtbTpsBRawMax);
	sendExternalEtbCalPedal(engineConfiguration->canEtbPedal1RawMin, engineConfiguration->canEtbPedal1RawMax,
		engineConfiguration->canEtbPedal2RawMin, engineConfiguration->canEtbPedal2RawMax);
}

#else // !(EFI_CAN_SUPPORT && EFI_EXTERNAL_CAN_ETB)

// Stubs so electronic_throttle.cpp/.h don't need their own EFI_CAN_SUPPORT/EFI_EXTERNAL_CAN_ETB
// guards - the board config that leaves enableExternalCanEtb reachable without this feature
// compiled in is a config error, not something these no-ops need to detect themselves.
CanDcMotor externalEtbCanMotor;
size_t getExternalEtbBus() { return 0; }
bool CanDcMotor::set(float) { return false; }
void CanDcMotor::enable() {}
void CanDcMotor::disable(const char*) {}
void CanDcMotor::sendTarget() {}
void sendExternalEtbGains() {}
void sendExternalEtbTarget() {}
void sendExternalEtbCalTps(uint16_t, uint16_t, uint16_t, uint16_t) {}
void sendExternalEtbCalPedal(uint16_t, uint16_t, uint16_t, uint16_t) {}
void sendExternalEtbCalibration() {}

#endif // EFI_CAN_SUPPORT && EFI_EXTERNAL_CAN_ETB
