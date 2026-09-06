#include "pch.h"

#include "adc_subscription.h"
#include "functional_sensor.h"
#include "redundant_sensor.h"
#include "redundant_ford_tps.h"
#include "proxy_sensor.h"
#include "linear_func.h"
#include "tps.h"
#include "can_etb.h"
#include "auto_generated_sensor.h"
#include "defaults.h"
#include "board_overrides.h"

struct TpsConfig {
	adc_channel_e channel;
	float closed;
	float open;
	float min;
	float max;
	// True for a "virtual" channel fed by postRawValue() from somewhere other than a physical ADC
	// pin (currently: the external CAN ETB board's TPS1/TPSB telemetry, see can_etb.h). Lets
	// LinearSensorUnit build the same calibration curve and register the same sensor a physical
	// pin would, while skipping the AdcSubscription hookup there's no real channel for.
	bool isVirtual = false;
};

std::optional<setup_custom_get_float_type> custom_board_getFuncPairAllowedSplit;

float getFuncPairAllowedSplit() {
	if (custom_board_getFuncPairAllowedSplit.has_value()) {
		return custom_board_getFuncPairAllowedSplit.value()();
	}
	return 0.5f;
}

/**
 * @brief LinearSensorUnit ties together a LinearFunc and a FunctionalSensor for sensor initialization.
 *
 * This class handles the setup of a single sensor that maps a raw ADC voltage to a physical value (like TPS percentage)
 * using a linear function. It manages ADC subscription, sensor registration, and validation of calibration values.
 */
class LinearSensorUnit {
public:
	AdcSubscriptionEntry* adc = nullptr;
	LinearSensorUnit(float divideInput, SensorType type)
		: m_func(divideInput)
		, m_sens(type, MS2NT(10)) {
		m_sens.setFunction(m_func);
	}

	bool init(const TpsConfig& cfg) {
		// If the configuration was invalid, don't continue to configure the sensor
		if (!configure(cfg)) {
			return false;
		}

		// A virtual channel has no physical ADC to subscribe to - its raw value arrives via an
		// external postRawValue() call instead (see LinearSensorUnit::postRawValue() below).
		if (!cfg.isVirtual) {
			adc = AdcSubscription::SubscribeSensor(m_sens, cfg.channel, /*lowpassCutoffHz*/ 200);
		} else {
			// The constructor's MS2NT(10) timeout assumes a physical ADC pin, refreshed every
			// conversion (sub-millisecond). A CAN-fed virtual channel only updates as fast as the
			// board transmits (external CAN ETB: 10Hz/100ms, see main.c's TELEMETRY_DIVIDER) - at
			// a 10ms timeout the sensor reads as timed-out ~90% of the time, which RedundantSensor
			// then reports as an inconsistent/invalid TPS. Give it headroom for that period plus
			// jitter, matching init_etb_can.cpp's etbCanSensorTimeout margin (3x the CAN period).
			m_sens.setTimeout(300);
		}

		return m_sens.Register();
	}

	void deinit() {
		AdcSubscription::UnsubscribeSensor(m_sens);
	}

	SensorType type() const {
		return m_sens.type();
	}

	const char* name() const {
		return m_sens.getSensorName();
	}

	// Feeds a raw value (volts) into this sensor's calibration curve from a virtual channel,
	// exactly as AdcSubscription would for a physical one - see TpsConfig::isVirtual.
	void postRawValue(float volts, efitick_t nowNt) {
		m_sens.postRawValue(volts, nowNt);
	}

private:
	bool configure(const TpsConfig& cfg) {
		// Only configure if we have a channel, or this is a virtual (CAN-fed) one
		if (!isAdcChannelValid(cfg.channel) && !cfg.isVirtual) {
#if EFI_UNIT_TEST
			printf("Configured NO hardware %s\n", name());
#endif
			return false;
		}

		float scaledClosed = cfg.closed / m_func.getDivideInput();
		float scaledOpen = cfg.open / m_func.getDivideInput();

		float split = std::abs(scaledOpen - scaledClosed);

		// If the voltage for closed vs. open is very near, something is wrong with your calibration
		if (split < getFuncPairAllowedSplit()) {
			firmwareError(
					ObdCode::OBD_TPS_Configuration,
					"\"%s\" problem: open %.2f/closed %.2f cal values are too close together. Check your calibration "
					"and wiring!",
					name(),
					cfg.open,
					cfg.closed);
			return false;
		}

		m_func.configure(cfg.closed, 0, cfg.open, POSITION_FULLY_OPEN, cfg.min, cfg.max);

#if EFI_UNIT_TEST
		printf("Configured YES %s\n", name());
#endif
		return true;
	}

	LinearFunc m_func;
	FunctionalSensor m_sens;
};

/**
 * @brief RedundantPair manages a pair of sensors for redundant input systems like TPS or PPS.
 *
 * This class coordinates two `LinearSensorUnit` instances (primary and secondary) and a `RedundantSensor` logic.
 * It provides validation to ensure that the two sensors are not electrically identical (to catch wiring errors)
 * and handles the registration of either a standard redundant sensor or a specialized Ford TPS redundant sensor.
 */
struct RedundantPair {
public:
	RedundantPair(LinearSensorUnit& pri, LinearSensorUnit& sec, SensorType outputType)
		: m_pri(pri)
		, m_sec(sec)
		, m_redund(outputType, m_pri.type(), m_sec.type()) {}

	void
	init(bool isFordTps,
		 RedundantFordTps* fordTps,
		 float secondaryMaximum,
		 const TpsConfig& primary,
		 const TpsConfig& secondary,
		 bool allowIdenticalSensors = false) {
		bool hasFirst = m_pri.init(primary);
		if (!hasFirst) {
			// no input if we have no first channel
			return;
		}

		if (!allowIdenticalSensors) {
			// Check that the primary and secondary aren't too close together - if so, the user may have done
			// an unsafe thing where they wired a single sensor to both inputs. Don't do that!
			bool hasBothSensors = (isAdcChannelValid(primary.channel) || primary.isVirtual)
				&& (isAdcChannelValid(secondary.channel) || secondary.isVirtual);
			bool tooCloseClosed = std::abs(primary.closed - secondary.closed) < 0.2f;
			bool tooCloseOpen = std::abs(primary.open - secondary.open) < 0.2f;

			if (hasBothSensors && tooCloseClosed && tooCloseOpen) {
				firmwareError(
						ObdCode::OBD_TPS_Configuration,
						"Configuration for redundant pair %s/%s are too similar - did you wire one sensor to both "
						"inputs...?",
						m_pri.name(),
						m_sec.name());
				return;
			}
		}

		bool hasSecond = m_sec.init(secondary);

		if (engineConfiguration->etbSplit <= 0 || engineConfiguration->etbSplit > MAX_TPS_PPS_DISCREPANCY) {
			engineConfiguration->etbSplit = 5;
		}

		if (isFordTps && fordTps) {
			// we have a secondary
			fordTps->configure(engineConfiguration->etbSplit, secondaryMaximum);
			fordTps->Register();
		} else {
			// not ford TPS
			m_redund.configure(engineConfiguration->etbSplit, !hasSecond);
#if EFI_UNIT_TEST
			printf("init m_redund.Register() %s\n", getSensorType(m_redund.type()));
#endif
			m_redund.Register();
		}
	}

	void deinit(bool isFordTps, RedundantFordTps* fordTps) {
		m_pri.deinit();
		m_sec.deinit();

		if (isFordTps && fordTps) {
			fordTps->unregister();
		} else {
			m_redund.unregister();
		}
	}

	// technical debt: oop violation: this method is specific to PPS usage
	void updateUnfilteredRawValues() {
		engine->outputChannels.rawRawPpsPrimary = m_pri.adc == nullptr ? 0 : m_pri.adc->sensorVolts;
		engine->outputChannels.rawRawPpsSecondary = m_sec.adc == nullptr ? 0 : m_sec.adc->sensorVolts;
	}

	// Feeds both halves of a virtual (CAN-fed) redundant pair - see TpsConfig::isVirtual.
	void postRawValues(float priVolts, float secVolts, efitick_t nowNt) {
		m_pri.postRawValue(priVolts, nowNt);
		m_sec.postRawValue(secVolts, nowNt);
	}

private:
	LinearSensorUnit& m_pri;
	LinearSensorUnit& m_sec;

	RedundantSensor m_redund;
};

static LinearSensorUnit tps1p(TPS_TS_CONVERSION, SensorType::Tps1Primary);
static LinearSensorUnit tps1s(TPS_TS_CONVERSION, SensorType::Tps1Secondary);
static LinearSensorUnit tps2p(TPS_TS_CONVERSION, SensorType::Tps2Primary);
static LinearSensorUnit tps2s(TPS_TS_CONVERSION, SensorType::Tps2Secondary);

// Used in case of "normal", non-Ford ETB TPS
static RedundantPair analogTps1(tps1p, tps1s, SensorType::Tps1);
static RedundantPair tps2(tps2p, tps2s, SensorType::Tps2);

#if EFI_SENT_SUPPORT
SentTps sentTps;
#endif

// Used only in case of weird Ford-style ETB TPS
static RedundantFordTps fordTps1(SensorType::Tps1, SensorType::Tps1Primary, SensorType::Tps1Secondary);
static RedundantFordTps fordTps2(SensorType::Tps2, SensorType::Tps2Primary, SensorType::Tps2Secondary);
static RedundantFordTps
		fordPps(SensorType::AcceleratorPedalUnfiltered,
				SensorType::AcceleratorPedalPrimary,
				SensorType::AcceleratorPedalSecondary);

// Pedal sensors and redundancy
static LinearSensorUnit pedalPrimary(1, SensorType::AcceleratorPedalPrimary);
static LinearSensorUnit pedalSecondary(1, SensorType::AcceleratorPedalSecondary);
static RedundantPair pedal(pedalPrimary, pedalSecondary, SensorType::AcceleratorPedalUnfiltered);

void updateUnfilteredRawPedal() {
	pedal.updateUnfilteredRawValues();
}

// This sensor indicates the driver's throttle intent - Pedal if we have one, TPS if not.
static ProxySensor driverIntent(SensorType::DriverThrottleIntent);
static ProxySensor ppsFilterSensor(SensorType::AcceleratorPedal);

// These sensors are TPS-like, so handle them in here too
static LinearSensorUnit wastegate(1, SensorType::WastegatePosition);
static LinearSensorUnit idlePos(PACK_MULT_VOLTAGE, SensorType::IdlePosition);

void initTps() {
	criticalAssertVoid(engineConfiguration != nullptr, "null engineConfiguration");
	percent_t minTpsPps = engineConfiguration->tpsErrorDetectionTooLow;
	percent_t maxTpsPps = engineConfiguration->tpsErrorDetectionTooHigh;

	if (!engineConfiguration->consumeObdSensors) {
		bool isFordTps = engineConfiguration->useFordRedundantTps;
		bool isFordPps = engineConfiguration->useFordRedundantPps;

		float tpsSecondaryMaximum = engineConfiguration->tpsSecondaryMaximum;
		if (tpsSecondaryMaximum < 20) {
			// don't allow <20% split point
			tpsSecondaryMaximum = 20;
		}

#if EFI_SENT_SUPPORT
		if (isDigitalTps1()) {
			sentTps.Register();
		} else
#endif
		{
			// Under external CAN ETB mode, TPS1/TPSB are "virtual" channels: no local ADC pin
			// (tps1_1AdcChannel/tps1_2AdcChannel stay unconfigured, see init_etb_can.cpp's file
			// header), fed instead by postExternalCanEtbRawTps() below from the board's ETB_RAW
			// telemetry. Same calibration curve (tpsMin/tpsMax/tps1SecondaryMin/Max), same
			// RedundantPair fault detection, as a physically-wired TPS1/TPSB would get.
			bool tps1IsVirtual = isExternalCanEtbEnabled();
			analogTps1.init(
					isFordTps,
					&fordTps1,
					tpsSecondaryMaximum,
					{engineConfiguration->tps1_1AdcChannel,
					 (float)engineConfiguration->tpsMin,
					 (float)engineConfiguration->tpsMax,
					 minTpsPps,
					 maxTpsPps,
					 tps1IsVirtual},
					{engineConfiguration->tps1_2AdcChannel,
					 (float)engineConfiguration->tps1SecondaryMin,
					 (float)engineConfiguration->tps1SecondaryMax,
					 minTpsPps,
					 maxTpsPps,
					 tps1IsVirtual});
		}

		tps2.init(
				isFordTps,
				&fordTps2,
				tpsSecondaryMaximum,
				{engineConfiguration->tps2_1AdcChannel,
				 (float)engineConfiguration->tps2Min,
				 (float)engineConfiguration->tps2Max,
				 minTpsPps,
				 maxTpsPps},
				{engineConfiguration->tps2_2AdcChannel,
				 (float)engineConfiguration->tps2SecondaryMin,
				 (float)engineConfiguration->tps2SecondaryMax,
				 minTpsPps,
				 maxTpsPps});

		float ppsSecondaryMaximum = engineConfiguration->ppsSecondaryMaximum;
		if (ppsSecondaryMaximum < 20) {
			// don't allow <20% split point
			ppsSecondaryMaximum = 20;
		}

		// Pedal sensors - under external CAN ETB mode, the pedal is wired to the board (not
		// rusEFI's own ADC) too, so it's a "virtual channel" exactly like TPS1/TPSB above: no local
		// ADC pin, fed instead by postExternalCanEtbRawPedal() from the board's ETB_RAW telemetry.
		bool pedalIsVirtual = isExternalCanEtbEnabled();
		pedal.init(
				isFordPps,
				&fordPps,
				ppsSecondaryMaximum,
				{engineConfiguration->throttlePedalPositionAdcChannel,
				 engineConfiguration->throttlePedalUpVoltage,
				 engineConfiguration->throttlePedalWOTVoltage,
				 minTpsPps,
				 maxTpsPps,
				 pedalIsVirtual},
				{engineConfiguration->throttlePedalPositionSecondAdcChannel,
				 engineConfiguration->throttlePedalSecondaryUpVoltage,
				 engineConfiguration->throttlePedalSecondaryWOTVoltage,
				 minTpsPps,
				 maxTpsPps,
				 pedalIsVirtual},
				engineConfiguration->allowIdenticalPps);
		ppsFilterSensor.setProxiedSensor(SensorType::AcceleratorPedalUnfiltered);
		ppsFilterSensor.setConverter([](SensorResult arg) {
			if (!arg) {
				return arg;
			}
			static ExpAverage ppsExpAverage;
			ppsExpAverage.setSmoothingFactor(engineConfiguration->ppsExpAverageAlpha);
			SensorResult result = ppsExpAverage.initOrAverage(arg.Value);
			return result;
		});
		ppsFilterSensor.Register();

		// TPS-like stuff that isn't actually a TPS
		wastegate.init(
				{engineConfiguration->wastegatePositionSensor,
				 engineConfiguration->wastegatePositionClosedVoltage,
				 engineConfiguration->wastegatePositionOpenedVoltage,
				 minTpsPps,
				 maxTpsPps});
		idlePos.init(
				{engineConfiguration->idlePositionChannel,
				 (float)engineConfiguration->idlePositionMin,
				 (float)engineConfiguration->idlePositionMax,
				 minTpsPps,
				 maxTpsPps});
	}

	// Route the pedal or TPS to driverIntent as appropriate
	if (isAdcChannelValid(engineConfiguration->throttlePedalPositionAdcChannel)) {
		driverIntent.setProxiedSensor(SensorType::AcceleratorPedal);
	} else {
		driverIntent.setProxiedSensor(SensorType::Tps1);
	}

	driverIntent.Register();
}

void deinitTps() {
	bool isFordTps = activeConfiguration.useFordRedundantTps;
	bool isFordPps = activeConfiguration.useFordRedundantPps;

	analogTps1.deinit(isFordTps, &fordTps1);
	tps2.deinit(isFordTps, &fordTps2);
	pedal.deinit(isFordPps, &fordPps);

#if EFI_SENT_SUPPORT
	sentTps.unregister();
#endif

	wastegate.deinit();
	idlePos.deinit();
}

// Feeds TPS1/TPSB volts (converted by the caller from the external CAN ETB board's raw ADC
// telemetry - see can_etb.h's CAN_ETB_BOARD_ADC_FULL_SCALE_VOLTS) into the same calibration curve
// a physically-wired TPS1/TPSB would use. Declared in tps.h; called from init_etb_can.cpp's
// EtbCanRawListener whenever a fresh ETB_RAW frame arrives. No-op (harmlessly updates an
// unregistered sensor nobody reads) unless initTps() configured analogTps1 as virtual, which only
// happens when isExternalCanEtbEnabled() (can_etb.h) is true - the same condition that gates
// registration of the CAN listener that calls this in the first place.
void postExternalCanEtbRawTps(float tps1Volts, float tpsBVolts, efitick_t nowNt) {
	analogTps1.postRawValues(tps1Volts, tpsBVolts, nowNt);
}

// Same idea as postExternalCanEtbRawTps(), for the pedal (also wired to the board, see
// init_etb_can.cpp's EtbCanRawListener). No-op unless initTps() configured pedal as virtual.
void postExternalCanEtbRawPedal(float pedal1Volts, float pedal2Volts, efitick_t nowNt) {
	pedal.postRawValues(pedal1Volts, pedal2Volts, nowNt);
}
