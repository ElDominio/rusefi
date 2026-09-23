/**
 * @file init_etb_can.cpp
 *
 * Wires up RX for the external CH32V203 ETB controller (see ../../controllers/can/can_etb.h and
 * external-etb/RUSEFI_SIDE_TODO.md #3.1, #7 steps 2-3). Gated by isExternalCanEtbEnabled()
 * (can_etb.h):
 *  - SensorType::Tps1/Tps1Secondary <- ETB_RAW's raw TPS1/TPSB, converted to volts (the board's
 *    fixed 5V/4095-count ADC scale, CAN_ETB_BOARD_ADC_FULL_SCALE_VOLTS in can_etb.h) and fed into
 *    the SAME calibration curve (tpsMin/tpsMax/tps1SecondaryMin/tps1SecondaryMax) and
 *    RedundantPair/RedundantSensor plausibility logic a physically-wired TPS1/TPSB would use -
 *    see init_tps.cpp's "virtual channel" support (TpsConfig::isVirtual) and
 *    postExternalCanEtbRawTps() (tps.h), called from EtbCanRawListener below. This board behaves,
 *    from rusEFI's perspective, exactly like a real ADC pin whose voltage happens to arrive over
 *    CAN instead of a wire - NOT registered as SensorType::Tps2, which means "throttle body 2"
 *    elsewhere (functionToTpsSensor(DC_Throttle2), init_tps.cpp's local RedundantPair tps2), not
 *    "this throttle's secondary channel" (that's SensorType::Tps1Secondary, exactly what TPSB is).
 *    ETB_STATUS's own pre-scaled TPS1/TPSB percent fields are no longer consumed rusEFI-side -
 *    the board still computes and sends them (needed for its own local PID's target tracking,
 *    see ETB_CAL_TPS below), but rusEFI now independently re-derives percent from ETB_RAW instead
 *    of trusting the board's copy, same as any other redundant TPS pair.
 *  - outputChannels.etbStatus.{iTerm,dTerm,pTerm} <- ETB_PID_STATUS (iTerm/dTerm) and
 *    ETB_FEEDFORWARD's piggybacked pTerm bytes (see EtbCanFeedForwardListener below), via plain
 *    CanListeners (step 3)
 *  - outputChannels.etbStatus.output AND outputChannels.etb1DutyCycle <- ETB_STATUS's actualDuty,
 *    via a plain CanListener (step 3) - the latter is the plain "ETB: Duty" gauge, which
 *    EtbController::setOutput() would normally populate but never runs for CAN mode
 *  - outputChannels.canEtbStatus <- ETB_STATUS's status byte (EtbCanStatus/etb_status_t: Fault/
 *    Disabled/Normal/OpenLoop/Autotune), via the same listener as the duty above - the one piece of
 *    telemetry that actually explains a stuck-at-zero duty (board-side fault or CAN staleness
 *    failsafe vs. genuinely computing zero output), previously decoded nowhere on the rusEFI side
 *    despite being on the wire (docs/external-etb-can-followups.md's wire-protocol-gaps section)
 *  - electronic_throttle_s::etbFeedForward (etb1etbFeedForward gauge) <- ETB_FEEDFORWARD, via
 *    IEtbController::setFeedForward() - the board's own applied feedforward, not a local
 *    recompute (docs/external-etb-can-followups.md)
 *  - getExternalEtbRawTps() <- ETB_RAW's raw TPS1/TPSB, for the auto-calibrate sweep (step 5/6,
 *    see electronic_throttle_impl.h's doAutocalExternalCan()) - same raw values also drive the
 *    SensorType::Tps1/Tps1Secondary feed above, from the same decodeFrame() call.
 *  - SensorType::AcceleratorPedalPrimary/Secondary <- ETB_RAW's raw PEDAL1/PEDAL2, converted to
 *    volts (same board ADC scale as TPS1/TPSB) and fed into the pedal's own "virtual channel"
 *    (init_tps.cpp's pedal RedundantPair, postExternalCanEtbRawPedal() in tps.h) - same treatment
 *    as TPS1/TPSB above, and for the same reason: the pedal is wired to the CH32 board in this
 *    design (external-etb/CH32V203_ETB_CONTROLLER.md - PA2/PA3), not rusEFI's own ADC. This
 *    reuses throttlePedalUpVoltage/WOTVoltage/SecondaryUpVoltage/SecondaryWOTVoltage - the same
 *    calibration fields and "Grab Idle/Up"/"Grab WOT/Down" buttons (grabPedalIsUp()/
 *    grabPedalIsWideOpen(), tps.cpp) a physically-wired pedal uses - rather than a separate
 *    raw-ADC-only calibration path, and gets AcceleratorPedalUnfiltered/AcceleratorPedal
 *    (ppsFilterSensor) and the RedundantPair mismatch check for free, same as TPS1/TPSB.
 *  - ETB_AUTOTUNE_STATUS_1/2 -> tsCalibrationSetData(TsCalMode::EtbKp/Ki/Kd, ...), the exact same
 *    mechanism/UI (Start/Stop ETB PID Autotune buttons) the local relay-autotune already uses
 *    (electronic_throttle.cpp's getClosedLoopAutotune()) - NOT YET IMPLEMENTED on the board side,
 *    see can_etb.h's EtbCanMode::Autotune comment.
 *
 * outputChannels.etbStatus is the same pid_status_s struct Pid::postState() (efi_pid.cpp) writes
 * for a *local* PID - there is no live Pid instance here (the CH32 runs its own PID loop), so this
 * populates the same struct a different way, from the board's own reported iTerm/dTerm/pTerm/duty
 * instead of computing them. error/resetCounter still aren't on the wire and are left at their
 * default of 0.
 *
 * Mirrors the ObdCanSensor pattern in init_can_sensors.cpp: when this flag is on, the board's
 * tps1_1AdcChannel/tps2_1AdcChannel should be left unconfigured so initTps() (init_tps.cpp)
 * never registers a local sensor for the same SensorType - registering both would hit the
 * "Duplicate registration" firmwareError in sensor.cpp.
 */

#include "pch.h"

#if EFI_CAN_SUPPORT && EFI_EXTERNAL_CAN_ETB
#include "can_listener.h"
#include "electronic_throttle.h"
#include "can_etb.h"
#include "tunerstudio_calibration_channel.h"
#include "tps.h"

// Board reports telemetry at 10Hz (external-etb/CH32V203_ETB_CONTROLLER.md #0) - allow a couple of
// missed frames before the sensor is considered stale, same margin as AemXSeriesLambda's wideband.
static constexpr efidur_t etbCanSensorTimeout = MS2NT(3 * 100);

// PID autotune status (NOT YET IMPLEMENTED on the board - see can_etb.h's EtbCanMode::Autotune).
// STATUS_1 (pFactor, iFactor) and STATUS_2 (dFactor) are expected back-to-back, so STATUS_2's
// handler is where the combined result actually gets reported to TS.
static float externalEtbAutotunePFactor = 0;
static float externalEtbAutotuneIFactor = 0;

class EtbCanAutotuneStatus1Listener : public CanListener {
public:
	EtbCanAutotuneStatus1Listener() : CanListener(CAN_ID_ETB_AUTOTUNE_STATUS_1) {}

	void decodeFrame(const CANRxFrame& frame, efitick_t /*nowNt*/) override {
		memcpy(&externalEtbAutotunePFactor, &frame.data8[ETB_AUTOTUNE_STATUS_1_OFFSET_PFACTOR], sizeof(float));
		memcpy(&externalEtbAutotuneIFactor, &frame.data8[ETB_AUTOTUNE_STATUS_1_OFFSET_IFACTOR], sizeof(float));
	}
};

static EtbCanAutotuneStatus1Listener externalEtbAutotuneStatus1Listener;

class EtbCanAutotuneStatus2Listener : public CanListener {
public:
	EtbCanAutotuneStatus2Listener() : CanListener(CAN_ID_ETB_AUTOTUNE_STATUS_2) {}

	void decodeFrame(const CANRxFrame& frame, efitick_t /*nowNt*/) override {
		if (!engine->etbAutoTune) {
			// Ignore stray frames outside an active autotune request - defense in depth, since the
			// board should only be sending these while it's actually in Autotune mode.
			return;
		}

		float dFactor;
		memcpy(&dFactor, &frame.data8[ETB_AUTOTUNE_STATUS_2_OFFSET_DFACTOR], sizeof(float));

#if EFI_TUNER_STUDIO
		// Cycle P->I->D for TS display, same idea as the local relay-autotune's own
		// m_autotuneCurrentParam (electronic_throttle.cpp): only one calibrationMode/Value slot
		// exists at a time, so show one parameter at a time rather than only ever showing the last.
		switch (m_cycleCounter++ % 3) {
			case 0: tsCalibrationSetData(TsCalMode::EtbKp, externalEtbAutotunePFactor); break;
			case 1: tsCalibrationSetData(TsCalMode::EtbKi, externalEtbAutotuneIFactor); break;
			case 2: tsCalibrationSetData(TsCalMode::EtbKd, dFactor); break;
		}
#endif // EFI_TUNER_STUDIO
	}

private:
	uint8_t m_cycleCounter = 0;
};

static EtbCanAutotuneStatus2Listener externalEtbAutotuneStatus2Listener;

// Duty portion of ETB_STATUS (CAN_ETB_BASE_ID+0) -> outputChannels.etbStatus.output. Separate listener from
// the TPS CanSensors above: multiple CanListeners can share one CAN ID (serviceCanSubscribers()
// in can_rx.cpp walks the whole list per frame), each just reads a different byte range.
class EtbCanDutyListener : public CanListener {
public:
	EtbCanDutyListener() : CanListener(CAN_ID_ETB_STATUS) {}

	void decodeFrame(const CANRxFrame& frame, efitick_t /*nowNt*/) override {
		const auto dutyRaw = reinterpret_cast<const scaled_channel<int16_t, 10000>*>(
			&frame.data8[ETB_STATUS_OFFSET_DUTY]);
		float duty = *dutyRaw; // -1.0 .. +1.0

#if EFI_TUNER_STUDIO
		// outputChannels.etbStatus.output is in the same percent space Pid::postState() uses
		// locally (ETB_PERCENT_TO_DUTY's inverse: percent = duty * 100).
		float dutyPercent = duty * 100.0f;
		engine->outputChannels.etbStatus.output = dutyPercent;

		// etb1DutyCycle (the plain "ETB: Duty" gauge, etbDutyCycleGauge in gauge_declarations.ini)
		// is a separate field only ever written by EtbController::setOutput() - a path CAN mode's
		// update() early-returns before reaching (docs/external-etb-can-followups.md's "dead/stale
		// telemetry" table). Mirror it here too, same value/units, so the gauge isn't stuck at 0.
		// No per-throttle guard needed: this protocol has no per-throttle addressing yet (one CAN
		// ETB board = "throttle 1"), same assumption checkStatus()'s postState() call already makes.
		engine->outputChannels.etb1DutyCycle = dutyPercent;

		// Board's own status byte (Fault/Disabled/Normal/OpenLoop/Autotune) - the only way to tell
		// from TunerStudio *why* duty/feedforward read zero: genuinely computed zero output (Normal)
		// vs. the board's h-bridge being forced off (Fault, or Disabled from ETB_TARGET going stale -
		// failsafe.h's 200ms watchdog). Raw enum value, same convention as wideband_state_s's
		// stateCode - no TS combo lookup wired up, just the number.
		engine->outputChannels.canEtbStatus = frame.data8[ETB_STATUS_OFFSET_STATUS];
#endif // EFI_TUNER_STUDIO
	}
};

static EtbCanDutyListener externalEtbDutyListener;

// ETB_FEEDFORWARD (CAN_ETB_BASE_ID+15) -> electronic_throttle_s::etbFeedForward (etb1etbFeedForward gauge),
// via IEtbController::setFeedForward() - the board reports what it actually applied each tick
// rather than rusEFI recomputing interpolate2d() locally, since the board is the authority on
// what it actually used (docs/external-etb-can-followups.md). Throttle-1-only, same "no per-
// throttle addressing yet" assumption checkStatus()'s postState() call already makes.
//
// Also carries the board's own pTerm (bytes 2-3, can_etb.h) -> outputChannels.etbStatus.pTerm -
// this was previously always 0 (see this file's top header comment): pid_state_t never persisted
// it and ETB_PID_STATUS (base+1) had no spare bytes for it, so pTerm was simply never on the wire
// at all. Piggybacking it on ETB_FEEDFORWARD's previously-unused reserved bytes gets it onto the
// same struct EtbCanPidStatusListener below populates, same as a local Pid::postState() would.
class EtbCanFeedForwardListener : public CanListener {
public:
	EtbCanFeedForwardListener() : CanListener(CAN_ID_ETB_FEEDFORWARD) {}

	void decodeFrame(const CANRxFrame& frame, efitick_t /*nowNt*/) override {
		const auto feedForwardRaw = reinterpret_cast<const scaled_channel<int16_t, 100>*>(
			&frame.data8[ETB_FEEDFORWARD_OFFSET_VALUE]);

		if (auto controller = engine->etbControllers[0]) {
			controller->setFeedForward(*feedForwardRaw);
		}

#if EFI_TUNER_STUDIO
		const auto pTermRaw = reinterpret_cast<const scaled_channel<int16_t, 100>*>(
			&frame.data8[ETB_FEEDFORWARD_OFFSET_PTERM]);
		engine->outputChannels.etbStatus.pTerm = *pTermRaw;
#endif // EFI_TUNER_STUDIO
	}
};

static EtbCanFeedForwardListener externalEtbFeedForwardListener;

// ETB_PID_STATUS (CAN_ETB_BASE_ID+1) -> outputChannels.etbStatus.{iTerm,dTerm}
class EtbCanPidStatusListener : public CanListener {
public:
	EtbCanPidStatusListener() : CanListener(CAN_ID_ETB_PID_STATUS) {}

	void decodeFrame(const CANRxFrame& frame, efitick_t /*nowNt*/) override {
		const auto iTermRaw = reinterpret_cast<const scaled_channel<int16_t, 100>*>(
			&frame.data8[ETB_PID_STATUS_OFFSET_ITERM]);
		const auto dTermRaw = reinterpret_cast<const scaled_channel<int16_t, 100>*>(
			&frame.data8[ETB_PID_STATUS_OFFSET_DTERM]);

#if EFI_TUNER_STUDIO
		engine->outputChannels.etbStatus.iTerm = *iTermRaw;
		engine->outputChannels.etbStatus.dTerm = *dTermRaw;
#endif // EFI_TUNER_STUDIO
	}
};

static EtbCanPidStatusListener externalEtbPidStatusListener;

// ETB_RAW (CAN_ETB_BASE_ID+2) -> latest raw TPS1/TPSB, for the auto-calibrate sweep only (step
// 5/6, getExternalEtbRawTps()). Not a CanSensor/SensorType itself - these are pre-conversion raw
// counts, kept only for the sweep's own endpoint-detection math. The SAME raw values also drive
// SensorType::Tps1/Tps1Secondary, via postExternalCanEtbRawTps() below - see this file's header.
struct EtbCanRawTps {
	uint16_t tps1 = 0;
	uint16_t tpsB = 0;
	efitick_t lastUpdate = 0;
};
static EtbCanRawTps externalEtbRawTps;

// raw-ADC-counts-to-volts conversion shared by TPS1/TPSB and PEDAL1/PEDAL2 below - all four are
// the same board ADC (CAN_ETB_BOARD_ADC_FULL_SCALE_VOLTS/MAX_COUNT, can_etb.h).
static float boardRawToVolts(uint16_t raw) {
	return raw / CAN_ETB_BOARD_ADC_MAX_COUNT * CAN_ETB_BOARD_ADC_FULL_SCALE_VOLTS;
}

class EtbCanRawListener : public CanListener {
public:
	EtbCanRawListener() : CanListener(CAN_ID_ETB_RAW) {}

	void decodeFrame(const CANRxFrame& frame, efitick_t nowNt) override {
		memcpy(&externalEtbRawTps.tps1, &frame.data8[ETB_RAW_OFFSET_TPS1], sizeof(uint16_t));
		memcpy(&externalEtbRawTps.tpsB, &frame.data8[ETB_RAW_OFFSET_TPSB], sizeof(uint16_t));
		externalEtbRawTps.lastUpdate = nowNt;

		// Feed rusEFI's own TPS1/TPSB calibration curve, exactly as a physical ADC pin would -
		// see init_tps.cpp's "virtual channel" support and this file's header comment.
		postExternalCanEtbRawTps(boardRawToVolts(externalEtbRawTps.tps1), boardRawToVolts(externalEtbRawTps.tpsB), nowNt);

		// Same treatment for the pedal - also wired to the board, not rusEFI's own ADC.
		uint16_t rawPedal1, rawPedal2;
		memcpy(&rawPedal1, &frame.data8[ETB_RAW_OFFSET_PEDAL1], sizeof(uint16_t));
		memcpy(&rawPedal2, &frame.data8[ETB_RAW_OFFSET_PEDAL2], sizeof(uint16_t));
		postExternalCanEtbRawPedal(boardRawToVolts(rawPedal1), boardRawToVolts(rawPedal2), nowNt);
	}
};

static EtbCanRawListener externalEtbRawListener;

bool getExternalEtbRawTps(uint16_t& tps1, uint16_t& tpsB) {
	if (externalEtbRawTps.lastUpdate == 0
			|| getTimeNowNt() - externalEtbRawTps.lastUpdate > etbCanSensorTimeout) {
		return false;
	}

	tps1 = externalEtbRawTps.tps1;
	tpsB = externalEtbRawTps.tpsB;
	return true;
}

void initExternalCanEtbSensors() {
	if (!isExternalCanEtbEnabled()) {
		return;
	}

	// Same guard initLambda() (init_lambda.cpp) uses for CAN wideband - without both directions
	// enabled, none of this (sensors, gains/target TX, bench-test/autocal, autotune) can reach the
	// board at all, so fail loudly here rather than leave the throttle silently non-functional.
	if (!engineConfiguration->canWriteEnabled || !engineConfiguration->canReadEnabled) {
		criticalError("CAN read and write are required to use external CAN ETB.");
		return;
	}

	registerCanListener(externalEtbDutyListener);
	registerCanListener(externalEtbPidStatusListener);
	registerCanListener(externalEtbRawListener);
	registerCanListener(externalEtbFeedForwardListener);

	registerCanListener(externalEtbAutotuneStatus1Listener);
	registerCanListener(externalEtbAutotuneStatus2Listener);
}
#else // !(EFI_CAN_SUPPORT && EFI_EXTERNAL_CAN_ETB)
void initExternalCanEtbSensors() {}
bool getExternalEtbRawTps(uint16_t&, uint16_t&) { return false; }
#endif // EFI_CAN_SUPPORT && EFI_EXTERNAL_CAN_ETB
