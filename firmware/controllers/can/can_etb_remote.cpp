/**
 * @file can_etb_remote.cpp
 *
 * The "remote ETB" side of the external CH32V203 ETB controller integration (see can_etb.h and
 * external-etb/RUSEFI_SIDE_TODO.md #3.1/#3.2). Two independent mechanisms live here, both gated by
 * isExternalCanEtbEnabled() (can_etb.h):
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
#include "tps.h"

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
struct EtbCanLimitsFrame {
	int16_t iTermMin;
	int16_t iTermMax;
	int16_t minValue;
	int16_t maxValue;
};
struct EtbCanBiasFrame {
	int16_t binA;
	int16_t valueA;
	int16_t binB;
	int16_t valueB;
};

static_assert(sizeof(EtbCanGains1Frame) == 8);
static_assert(sizeof(EtbCanGains2Frame) == 8);
static_assert(sizeof(EtbCanTargetFrame) == 8);
static_assert(sizeof(EtbCanCalFrame) == 8);
static_assert(sizeof(EtbCanLimitsFrame) == 8);
static_assert(sizeof(EtbCanBiasFrame) == 8);

CanDcMotor externalEtbCanMotor;

void CanDcMotor::sendTarget() {
	CanTxTyped<EtbCanTargetFrame> m(CanCategory::ETB, CAN_ID_ETB_TARGET, true, getExternalEtbBus());
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
		CanTxTyped<EtbCanGains1Frame> m(CanCategory::ETB, CAN_ID_ETB_GAINS_1, true, getExternalEtbBus());
		m->pFactor = engineConfiguration->etb.pFactor;
		m->iFactor = engineConfiguration->etb.iFactor;
	}
	{
		CanTxTyped<EtbCanGains2Frame> m(CanCategory::ETB, CAN_ID_ETB_GAINS_2, true, getExternalEtbBus());
		m->dFactor = engineConfiguration->etb.dFactor;
		m->offset = engineConfiguration->etb.offset;
	}
}

// iTerm/output limits (RUSEFI_SIDE_TODO.md/can_etb.h's ETB_LIMITS) - the board hardcodes both to
// its own PID's output clamp until this arrives (electronic_throttle_impl.h's "just PID" gap #2).
// engineConfiguration->etb.minValue/maxValue already default to the same +-100 the board hardcodes,
// but etb_iTermMin/etb_iTermMax default to a much tighter +-30 - this is the one that actually
// changes behavior once transmitted.
void sendExternalEtbLimits() {
	CanTxTyped<EtbCanLimitsFrame> m(CanCategory::ETB, CAN_ID_ETB_LIMITS, true, getExternalEtbBus());
	m->iTermMin = (int16_t)(engineConfiguration->etb_iTermMin * 100.0f);
	m->iTermMax = (int16_t)(engineConfiguration->etb_iTermMax * 100.0f);
	m->minValue = (int16_t)(engineConfiguration->etb.minValue * 100.0f);
	m->maxValue = (int16_t)(engineConfiguration->etb.maxValue * 100.0f);
}

// Feedforward/bias curve (can_etb.h's ETB_BIAS_1..4) - mirrors EtbController::getOpenLoop()'s
// interpolate2d(target, etbBiasBins, etbBiasValues) onto the board, which was previously running
// PID alone with no feedforward at all (electronic_throttle_impl.h's "dead ETB actuator-page
// controls" gap #1 in docs/external-etb-can-followups.md).
void sendExternalEtbBiasCurve() {
	static_assert(ETB_BIAS_CURVE_LENGTH == 8, "ETB_BIAS_1..4 assumes exactly 8 points, 2 per frame");

	uint32_t frameIds[4] = { CAN_ID_ETB_BIAS_1, CAN_ID_ETB_BIAS_2, CAN_ID_ETB_BIAS_3, CAN_ID_ETB_BIAS_4 };
	for (int frame = 0; frame < 4; frame++) {
		int a = frame * 2;
		int b = a + 1;
		CanTxTyped<EtbCanBiasFrame> m(CanCategory::ETB, frameIds[frame], true, getExternalEtbBus());
		m->binA = (int16_t)(config->etbBiasBins[a] * 100.0f);
		m->valueA = (int16_t)(config->etbBiasValues[a] * 100.0f);
		m->binB = (int16_t)(config->etbBiasBins[b] * 100.0f);
		m->valueB = (int16_t)(config->etbBiasValues[b] * 100.0f);
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

	CanTxTyped<EtbCanTargetFrame> m(CanCategory::ETB, CAN_ID_ETB_TARGET, true, getExternalEtbBus());
	m->targetPosition = target.Value;
	m->benchDutyRaw = 0; // unused when mode == Normal or Autotune
	m->mode = (uint8_t)(autotuneRequested ? EtbCanMode::Autotune : EtbCanMode::Normal);
	m->sequence = targetSeq++;
}

void sendExternalEtbCalTps(uint16_t rawMin1, uint16_t rawMax1, uint16_t rawMin2, uint16_t rawMax2) {
	CanTxTyped<EtbCanCalFrame> m(CanCategory::ETB, CAN_ID_ETB_CAL_TPS, true, getExternalEtbBus());
	m->min1 = rawMin1;
	m->max1 = rawMax1;
	m->min2 = rawMin2;
	m->max2 = rawMax2;
}

void sendExternalEtbCalPedal(uint16_t rawMin1, uint16_t rawMax1, uint16_t rawMin2, uint16_t rawMax2) {
	CanTxTyped<EtbCanCalFrame> m(CanCategory::ETB, CAN_ID_ETB_CAL_PEDAL, true, getExternalEtbBus());
	m->min1 = rawMin1;
	m->max1 = rawMax1;
	m->min2 = rawMin2;
	m->max2 = rawMax2;
}

// engineConfiguration->tpsMin/tpsMax/tps1SecondaryMin/tps1SecondaryMax are the same fields a
// physically-wired TPS1/TPSB calibration would use (see init_tps.cpp's "virtual channel" support) -
// converted to the board's raw ADC counts here, at the CAN TX boundary, using its known fixed
// 5V/4095-count scale (can_etb.h's CAN_ETB_BOARD_ADC_FULL_SCALE_VOLTS/MAX_COUNT). There is no
// separate canEtbTps1RawMin-style persisted field anymore - these are the single source of truth,
// same as a real pin, whether set manually in TunerStudio or by doAutocalExternalCan()'s sweep
// (electronic_throttle_impl.h).
//
// tps_limit_t (tps.h) is a plain int16_t packed at TPS_TS_CONVERSION counts/volt, NOT already
// volts and NOT the board's own ADC scale - divide by TPS_TS_CONVERSION first to get real volts,
// exactly like LinearSensorUnit's LinearFunc (divideInput=TPS_TS_CONVERSION) already does when
// unpacking the same fields for the local calibration curve.
static float tpsPackedToVolts(int16_t packed) {
	return packed / TPS_TS_CONVERSION;
}

static uint16_t voltsToCanEtbBoardRaw(float volts) {
	float raw = volts / CAN_ETB_BOARD_ADC_FULL_SCALE_VOLTS * CAN_ETB_BOARD_ADC_MAX_COUNT;
	return (uint16_t)clampF(0, raw, CAN_ETB_BOARD_ADC_MAX_COUNT);
}

void sendExternalEtbCalibration() {
	// tpsMin/tpsMax etc default to 0, so before calibration has ever happened this forwards a
	// degenerate rawMax<=rawMin span - the board's raw_to_percent() (can_bus.c) deliberately
	// guards that by reporting a flat 0% rather than a fabricated identity-scaled guess, so
	// TPS1/TPSB correctly read 0% until real calibration exists. That's intentional, not a bug: an
	// uncalibrated percent would look plausible while being mechanically meaningless. Use the
	// "Raw TPS"/"Raw Pedal" V gauges (status_loop.cpp's updateRawSensors(), sourced from ETB_RAW
	// directly, independent of calibration) to verify sensor wiring/movement before calibrating -
	// see docs/report.md.
	sendExternalEtbCalTps(
		voltsToCanEtbBoardRaw(tpsPackedToVolts(engineConfiguration->tpsMin)),
		voltsToCanEtbBoardRaw(tpsPackedToVolts(engineConfiguration->tpsMax)),
		voltsToCanEtbBoardRaw(tpsPackedToVolts(engineConfiguration->tps1SecondaryMin)),
		voltsToCanEtbBoardRaw(tpsPackedToVolts(engineConfiguration->tps1SecondaryMax)));

	// throttlePedalUpVoltage/WOTVoltage/SecondaryUpVoltage/SecondaryWOTVoltage are plain floats
	// (already real volts, unlike tps_limit_t - no unpacking needed) - the same fields a
	// physically-wired pedal calibration would use (init_tps.cpp's "virtual channel" support).
	sendExternalEtbCalPedal(
		voltsToCanEtbBoardRaw(engineConfiguration->throttlePedalUpVoltage),
		voltsToCanEtbBoardRaw(engineConfiguration->throttlePedalWOTVoltage),
		voltsToCanEtbBoardRaw(engineConfiguration->throttlePedalSecondaryUpVoltage),
		voltsToCanEtbBoardRaw(engineConfiguration->throttlePedalSecondaryWOTVoltage));
}

#else // !(EFI_CAN_SUPPORT && EFI_EXTERNAL_CAN_ETB)

// Stubs so electronic_throttle.cpp/.h don't need their own EFI_CAN_SUPPORT/EFI_EXTERNAL_CAN_ETB
// guards. Nothing here is reachable in this build anyway: isExternalCanEtbEnabled() (can_etb.h)
// folds the feature flag in, so the config bit reads as false and every caller short-circuits
// before it gets this far.
CanDcMotor externalEtbCanMotor;
size_t getExternalEtbBus() { return 0; }
bool CanDcMotor::set(float) { return false; }
void CanDcMotor::enable() {}
void CanDcMotor::disable(const char*) {}
void CanDcMotor::sendTarget() {}
void sendExternalEtbGains() {}
void sendExternalEtbTarget() {}
void sendExternalEtbLimits() {}
void sendExternalEtbBiasCurve() {}
void sendExternalEtbCalTps(uint16_t, uint16_t, uint16_t, uint16_t) {}
void sendExternalEtbCalPedal(uint16_t, uint16_t, uint16_t, uint16_t) {}
void sendExternalEtbCalibration() {}

#endif // EFI_CAN_SUPPORT && EFI_EXTERNAL_CAN_ETB
