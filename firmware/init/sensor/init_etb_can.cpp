/**
 * @file init_etb_can.cpp
 *
 * Wires up RX for the external CH32V203 ETB controller (see ../../controllers/can/can_etb.h and
 * external-etb/RUSEFI_SIDE_TODO.md #3.1, #7 steps 2-3). Gated by
 * engineConfiguration->enableExternalCanEtb:
 *  - SensorType::Tps1/Tps2 <- ETB_STATUS's TPS1/TPSB, via CanSensor<> (step 2)
 *  - outputChannels.etbStatus.{iTerm,dTerm} <- ETB_PID_STATUS, via a plain CanListener (step 3)
 *  - outputChannels.etbStatus.output <- ETB_STATUS's actualDuty, via a plain CanListener (step 3)
 *  - getExternalEtbRawTps() <- ETB_RAW's raw TPS1/TPSB, for the auto-calibrate sweep (step 5/6,
 *    see electronic_throttle_impl.h's doAutocalExternalCan())
 *  - SensorType::AcceleratorPedalPrimary/Secondary <- ETB_RAW's raw PEDAL1/PEDAL2, scaled by
 *    engineConfiguration->canEtbPedal1/2RawMin/Max, via CanPedalSensor. The pedal is wired to the
 *    CH32 board in this design (external-etb/CH32V203_ETB_CONTROLLER.md - PA2/PA3), not rusEFI's
 *    own ADC, and unlike TPS there is no pre-calibrated-percent frame for it (can_bus.h has no
 *    pedal equivalent of ETB_STATUS) - so rusEFI has to compute percent itself from raw + its own
 *    calibration, the same way the board computes TPS1/TPSB percent from raw + ETB_CAL_TPS.
 *    Feeds the existing SensorType::AcceleratorPedalUnfiltered/AcceleratorPedal pipeline
 *    (init_tps.cpp's RedundantSensor + ppsFilterSensor) unchanged once registered here.
 *  - ETB_AUTOTUNE_STATUS_1/2 -> tsCalibrationSetData(TsCalMode::EtbKp/Ki/Kd, ...), the exact same
 *    mechanism/UI (Start/Stop ETB PID Autotune buttons) the local relay-autotune already uses
 *    (electronic_throttle.cpp's getClosedLoopAutotune()) - NOT YET IMPLEMENTED on the board side,
 *    see can_etb.h's EtbCanMode::Autotune comment.
 *
 * outputChannels.etbStatus is the same pid_status_s struct Pid::postState() (efi_pid.cpp) writes
 * for a *local* PID - there is no live Pid instance here (the CH32 runs its own PID loop), so this
 * populates the same struct a different way, from the board's own reported iTerm/dTerm/duty
 * instead of computing them. pTerm/error/resetCounter aren't on the wire (the CH32 doesn't track
 * pTerm separately, see can_bus.c's can_tx_pid_status()) and are left at their default of 0.
 *
 * Mirrors the ObdCanSensor pattern in init_can_sensors.cpp: when this flag is on, the board's
 * tps1_1AdcChannel/tps2_1AdcChannel should be left unconfigured so initTps() (init_tps.cpp)
 * never registers a local sensor for the same SensorType - registering both would hit the
 * "Duplicate registration" firmwareError in sensor.cpp.
 */

#include "pch.h"

#if EFI_CAN_SUPPORT
#include "can_sensor.h"
#include "can_listener.h"
#include "can_etb.h"
#include "redundant_sensor.h"
#include "tunerstudio_calibration_channel.h"

// Board reports telemetry at 10Hz (external-etb/CH32V203_ETB_CONTROLLER.md #0) - allow a couple of
// missed frames before the sensor is considered stale, same margin as AemXSeriesLambda's wideband.
static constexpr efidur_t etbCanSensorTimeout = MS2NT(3 * 100);

static CanSensor<int16_t, PACK_MULT_PERCENT> externalEtbTps1Sensor(
	CAN_ID_ETB_STATUS, ETB_STATUS_OFFSET_TPS1, SensorType::Tps1, etbCanSensorTimeout);

static CanSensor<int16_t, PACK_MULT_PERCENT> externalEtbTps2Sensor(
	CAN_ID_ETB_STATUS, ETB_STATUS_OFFSET_TPSB, SensorType::Tps2, etbCanSensorTimeout);

// ETB_RAW (0x302)'s PEDAL1/PEDAL2 -> percent, scaled by the "grab" calibration
// (canEtbPedal1/2RawMin/Max, see grabPedalIsUp()/grabPedalIsWideOpen() in tps.cpp). Not a plain
// CanSensor<>: that template assumes the wire value already IS the scaled reading, but here the
// wire only carries raw ADC and the scale (calibration) is rusEFI-side config, not fixed - so this
// does the linear scaling itself in decodeFrame(), like a tiny CanSensor + FunctionalSensor rolled
// into one. getRaw() returns the untouched ADC count for grabPedalIsUp()/WideOpen() to read.
class CanPedalSensor : public CanSensorBase {
public:
	CanPedalSensor(uint8_t rawOffset, bool isSecondaryChannel, SensorType type)
		: CanSensorBase(CAN_ID_ETB_RAW, type, etbCanSensorTimeout)
		, m_rawOffset(rawOffset)
		, m_isSecondaryChannel(isSecondaryChannel)
	{
	}

	float getRaw() const override {
		return m_rawValue;
	}

	bool hasRaw() const override {
		return true;
	}

	void decodeFrame(const CANRxFrame& frame, efitick_t nowNt) override {
		uint16_t raw;
		memcpy(&raw, &frame.data8[m_rawOffset], sizeof(raw));
		m_rawValue = raw;

		uint16_t rawMin = m_isSecondaryChannel
			? engineConfiguration->canEtbPedal2RawMin : engineConfiguration->canEtbPedal1RawMin;
		uint16_t rawMax = m_isSecondaryChannel
			? engineConfiguration->canEtbPedal2RawMax : engineConfiguration->canEtbPedal1RawMax;

		// Not yet calibrated (both fields still 0, or a nonsensical min>=max) - leave the value
		// invalid (times out) rather than report a meaningless percent. getSanitizedPedal()
		// (electronic_throttle.cpp) already treats an invalid pedal as 0% (closed/idle), which is
		// the safe direction to fail in for an uncalibrated pedal.
		if (rawMax <= rawMin) {
			return;
		}

		float percent = 100.0f * (raw - (float)rawMin) / (rawMax - rawMin);
		setValidValue(clampPercentValue(percent), nowNt);
	}

private:
	const uint8_t m_rawOffset;
	const bool m_isSecondaryChannel;
	float m_rawValue = 0;
};

static CanPedalSensor externalEtbPedal1Sensor(
	ETB_RAW_OFFSET_PEDAL1, false, SensorType::AcceleratorPedalPrimary);

static CanPedalSensor externalEtbPedal2Sensor(
	ETB_RAW_OFFSET_PEDAL2, true, SensorType::AcceleratorPedalSecondary);

// Combines the two into AcceleratorPedalUnfiltered, same as RedundantPair does for the local ADC
// pedal in init_tps.cpp - init_tps.cpp's own pedal.init() never registers this SensorType when
// throttlePedalPositionAdcChannel is left unconfigured (matching the "leave local channels
// unconfigured" convention this file's header already documents for TPS), so it's free here.
// initTps()'s ppsFilterSensor (unconditional, feeds SensorType::AcceleratorPedal with the usual
// ppsExpAverage smoothing) picks this up automatically - no further wiring needed.
static RedundantSensor externalEtbPedalUnfiltered(
	SensorType::AcceleratorPedalUnfiltered,
	SensorType::AcceleratorPedalPrimary,
	SensorType::AcceleratorPedalSecondary);

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

// Duty portion of ETB_STATUS (0x300) -> outputChannels.etbStatus.output. Separate listener from
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
		engine->outputChannels.etbStatus.output = duty * 100.0f;
#endif // EFI_TUNER_STUDIO
	}
};

static EtbCanDutyListener externalEtbDutyListener;

// ETB_PID_STATUS (0x301) -> outputChannels.etbStatus.{iTerm,dTerm}
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

// ETB_RAW (0x302) -> latest raw TPS1/TPSB, for the auto-calibrate sweep only (step 5/6). Not a
// CanSensor/SensorType: these raw counts aren't a value anything else should consume, and there's
// no natural SensorType for them - a plain latched struct plus an accessor keeps this internal to
// the one caller that needs it (doAutocalExternalCan()).
struct EtbCanRawTps {
	uint16_t tps1 = 0;
	uint16_t tpsB = 0;
	efitick_t lastUpdate = 0;
};
static EtbCanRawTps externalEtbRawTps;

class EtbCanRawListener : public CanListener {
public:
	EtbCanRawListener() : CanListener(CAN_ID_ETB_RAW) {}

	void decodeFrame(const CANRxFrame& frame, efitick_t nowNt) override {
		memcpy(&externalEtbRawTps.tps1, &frame.data8[ETB_RAW_OFFSET_TPS1], sizeof(uint16_t));
		memcpy(&externalEtbRawTps.tpsB, &frame.data8[ETB_RAW_OFFSET_TPSB], sizeof(uint16_t));
		externalEtbRawTps.lastUpdate = nowNt;
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
	if (!engineConfiguration->enableExternalCanEtb) {
		return;
	}

	// Same guard initLambda() (init_lambda.cpp) uses for CAN wideband - without both directions
	// enabled, none of this (sensors, gains/target TX, bench-test/autocal, autotune) can reach the
	// board at all, so fail loudly here rather than leave the throttle silently non-functional.
	if (!engineConfiguration->canWriteEnabled || !engineConfiguration->canReadEnabled) {
		criticalError("CAN read and write are required to use external CAN ETB.");
		return;
	}

	registerCanSensor(externalEtbTps1Sensor);
	registerCanSensor(externalEtbTps2Sensor);
	registerCanListener(externalEtbDutyListener);
	registerCanListener(externalEtbPidStatusListener);
	registerCanListener(externalEtbRawListener);

	registerCanSensor(externalEtbPedal1Sensor);
	registerCanSensor(externalEtbPedal2Sensor);
	// Same tolerance the local ADC pedal's RedundantPair uses (init_tps.cpp) - not pedal-specific,
	// every redundant pair in this codebase shares this one config value.
	externalEtbPedalUnfiltered.configure(engineConfiguration->etbSplit, /*ignoreSecondSensor*/false);
	externalEtbPedalUnfiltered.Register();

	registerCanListener(externalEtbAutotuneStatus1Listener);
	registerCanListener(externalEtbAutotuneStatus2Listener);
}
#else // EFI_CAN_SUPPORT
void initExternalCanEtbSensors() {}
bool getExternalEtbRawTps(uint16_t&, uint16_t&) { return false; }
#endif // EFI_CAN_SUPPORT
