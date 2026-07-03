#include "pch.h"
#include "cht_clt_estimator.h"
#include "custom_page.h"

static void setFlatCurve(int16_t (&bins)[CLT_EST_CURVE_SIZE], float (&values)[CLT_EST_CURVE_SIZE],
		int16_t binLo, int16_t binHi, float value) {
	for (int i = 0; i < CLT_EST_CURVE_SIZE; i++) {
		bins[i] = binLo + (binHi - binLo) * i / (CLT_EST_CURVE_SIZE - 1);
		values[i] = value;
	}
}

TEST(ChtCltEstimatorTest, TrendsTowardChtAtZeroAirflow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->cltFromCht = true;
	getCustomPage()->cltEstThermostatTemp = 40;
	getCustomPage()->cltEstFan1Cfm = 0;
	getCustomPage()->cltEstFan2Cfm = 0;
	getCustomPage()->cltEstFallbackClt = 20;
	getCustomPage()->cltEstRadiatorFailTemp = 200;

	getCustomPage()->cltEstHeadTransferRate = 0.05f;
	getCustomPage()->cltEstHeadTransferGain = 0.0f;
	getCustomPage()->cltEstRadiatorBaseRejection = 0.0f;
	getCustomPage()->cltEstRadiatorAirflowGain = 0.0f;
	setFlatCurve(getCustomPage()->cltEstVssBins, getCustomPage()->cltEstVssAirflow, 0, 400, 0);
	setFlatCurve(getCustomPage()->cltEstIatBins, getCustomPage()->cltEstIatFactor, -40, 200, 1.0f);

	initChtCltEstimator();
	Sensor::resetMockValue(SensorType::Clt); // EngineTestHelper pre-mocks Clt=70; clear it so our estimator's registered value actually surfaces.

	Sensor::setMockValue(SensorType::Iat, 20);
	Sensor::setInvalidMockValue(SensorType::VehicleSpeed);

	// Seed the estimator at a low CHT first (first valid update pins the estimate at CHT),
	// then jump CHT up and confirm the estimate actually climbs toward it over time.
	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 40);
	updateChtCltEstimator();

	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 90);
	for (int i = 0; i < 200; i++) {
		eth.moveTimeForwardSec(1);
		updateChtCltEstimator();
	}

	// Zero rejection rate everywhere: CLT should have climbed close to CHT, not stayed
	// down near the (much lower) thermostat temperature.
	EXPECT_NEAR(90, Sensor::getOrZero(SensorType::Clt), 1.0);
}

TEST(ChtCltEstimatorTest, TrendsTowardThermostatAtHighAirflow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->cltFromCht = true;
	getCustomPage()->cltEstThermostatTemp = 70;
	getCustomPage()->cltEstFan1Cfm = 500;
	getCustomPage()->cltEstFan2Cfm = 500;
	getCustomPage()->cltEstFallbackClt = 20;
	getCustomPage()->cltEstRadiatorFailTemp = 200;
	// Zero spread/lag: valve snaps instantly fully open right at thermostatTemp, isolating the
	// base competing-rate ODE from the (separately tested) valve-lag hunting behavior.
	getCustomPage()->cltEstThermostatSpread = 0;
	getCustomPage()->cltEstThermostatLag = 0;

	// Mild heat rate, strong rejection rate once there's airflow.
	getCustomPage()->cltEstHeadTransferRate = 0.01f;
	getCustomPage()->cltEstHeadTransferGain = 0.0f;
	getCustomPage()->cltEstRadiatorBaseRejection = 0.2f;
	getCustomPage()->cltEstRadiatorAirflowGain = 0.0f;
	setFlatCurve(getCustomPage()->cltEstVssBins, getCustomPage()->cltEstVssAirflow, 0, 400, 0);
	setFlatCurve(getCustomPage()->cltEstIatBins, getCustomPage()->cltEstIatFactor, -40, 200, 1.0f);

	initChtCltEstimator();
	Sensor::resetMockValue(SensorType::Clt); // EngineTestHelper pre-mocks Clt=70; clear it so our estimator's registered value actually surfaces.

	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 120);
	Sensor::setMockValue(SensorType::Iat, 20);
	enginePins.fanRelay.setValue(1);
	enginePins.fanRelay2.setValue(1);

	for (int i = 0; i < 200; i++) {
		eth.moveTimeForwardSec(1);
		updateChtCltEstimator();
	}

	// Strong rejection with fans blowing should pull CLT down toward the thermostat
	// temperature rather than letting it track the much hotter CHT.
	EXPECT_NEAR(70, Sensor::getOrZero(SensorType::Clt), 5.0);

	enginePins.fanRelay.setValue(0);
	enginePins.fanRelay2.setValue(0);
}

TEST(ChtCltEstimatorTest, InvalidVssSoftDegradesToFanOnlyAirflow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->cltFromCht = true;
	getCustomPage()->cltEstThermostatTemp = 70;
	getCustomPage()->cltEstFan1Cfm = 100;
	getCustomPage()->cltEstFan2Cfm = 150;
	getCustomPage()->cltEstFallbackClt = 20;
	getCustomPage()->cltEstRadiatorFailTemp = 200;

	getCustomPage()->cltEstHeadTransferRate = 0.02f;
	getCustomPage()->cltEstHeadTransferGain = 0.0f;
	getCustomPage()->cltEstRadiatorBaseRejection = 0.02f;
	getCustomPage()->cltEstRadiatorAirflowGain = 0.0f;
	// VSS->airflow curve reads exactly 0 CFM at 0 kph (interpolate2d(0, ...) clamps to the
	// first bin's value when VSS is invalid and getOrZero() degrades to 0).
	getCustomPage()->cltEstVssBins[0] = 0;
	getCustomPage()->cltEstVssAirflow[0] = 0;
	for (int i = 1; i < CLT_EST_CURVE_SIZE; i++) {
		getCustomPage()->cltEstVssBins[i] = 50 * i;
		getCustomPage()->cltEstVssAirflow[i] = 200 * i;
	}
	setFlatCurve(getCustomPage()->cltEstIatBins, getCustomPage()->cltEstIatFactor, -40, 200, 1.0f);

	initChtCltEstimator();
	Sensor::resetMockValue(SensorType::Clt); // EngineTestHelper pre-mocks Clt=70; clear it so our estimator's registered value actually surfaces.

	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 90);
	Sensor::setMockValue(SensorType::Iat, 20);
	Sensor::setInvalidMockValue(SensorType::VehicleSpeed);
	enginePins.fanRelay.setValue(1);
	enginePins.fanRelay2.setValue(1);

	updateChtCltEstimator();

	EXPECT_NEAR(250, getChtCltEstimator().airflow, 0.01);

	enginePins.fanRelay.setValue(0);
	enginePins.fanRelay2.setValue(0);
}

TEST(ChtCltEstimatorTest, FallbackWhenChtInvalid) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->cltFromCht = true;
	getCustomPage()->cltEstFallbackClt = 77;

	initChtCltEstimator();
	Sensor::resetMockValue(SensorType::Clt); // EngineTestHelper pre-mocks Clt=70; clear it so our estimator's registered value actually surfaces.

	Sensor::setInvalidMockValue(SensorType::CylinderHeadTemperature);

	updateChtCltEstimator();

	EXPECT_NEAR(77, Sensor::getOrZero(SensorType::Clt), 0.01);
}

TEST(ChtCltEstimatorTest, EstimateNeverExceedsCht) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->cltFromCht = true;
	// Adversarial: thermostat temp above CHT would otherwise pull the ODE above CHT.
	getCustomPage()->cltEstThermostatTemp = 150;
	getCustomPage()->cltEstFan1Cfm = 0;
	getCustomPage()->cltEstFan2Cfm = 0;
	getCustomPage()->cltEstFallbackClt = 20;
	getCustomPage()->cltEstRadiatorFailTemp = 200;

	getCustomPage()->cltEstHeadTransferRate = 0.05f;
	getCustomPage()->cltEstHeadTransferGain = 0.0f;
	getCustomPage()->cltEstRadiatorBaseRejection = 0.05f;
	getCustomPage()->cltEstRadiatorAirflowGain = 0.0f;
	setFlatCurve(getCustomPage()->cltEstVssBins, getCustomPage()->cltEstVssAirflow, 0, 400, 0);
	setFlatCurve(getCustomPage()->cltEstIatBins, getCustomPage()->cltEstIatFactor, -40, 200, 1.0f);

	initChtCltEstimator();
	Sensor::resetMockValue(SensorType::Clt); // EngineTestHelper pre-mocks Clt=70; clear it so our estimator's registered value actually surfaces.

	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 90);
	Sensor::setMockValue(SensorType::Iat, 20);
	Sensor::setInvalidMockValue(SensorType::VehicleSpeed);

	for (int i = 0; i < 200; i++) {
		eth.moveTimeForwardSec(1);
		updateChtCltEstimator();
		EXPECT_LE(Sensor::getOrZero(SensorType::Clt), 90.001f);
	}
}

TEST(ChtCltEstimatorTest, RadiatorFailLocksCltToCht) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->cltFromCht = true;
	// Thermostat temp is far below CHT, which would otherwise pull CLT down over time -
	// the radiator-fail override must bypass that entirely and lock CLT = CHT immediately.
	getCustomPage()->cltEstThermostatTemp = 40;
	getCustomPage()->cltEstFan1Cfm = 0;
	getCustomPage()->cltEstFan2Cfm = 0;
	getCustomPage()->cltEstFallbackClt = 20;
	getCustomPage()->cltEstRadiatorFailTemp = 126;

	getCustomPage()->cltEstHeadTransferRate = 0.02f;
	getCustomPage()->cltEstHeadTransferGain = 0.0f;
	getCustomPage()->cltEstRadiatorBaseRejection = 0.05f;
	getCustomPage()->cltEstRadiatorAirflowGain = 0.0f;
	setFlatCurve(getCustomPage()->cltEstVssBins, getCustomPage()->cltEstVssAirflow, 0, 400, 0);
	setFlatCurve(getCustomPage()->cltEstIatBins, getCustomPage()->cltEstIatFactor, -40, 200, 1.0f);

	initChtCltEstimator();
	Sensor::resetMockValue(SensorType::Clt); // EngineTestHelper pre-mocks Clt=70; clear it so our estimator's registered value actually surfaces.

	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 130);
	Sensor::setMockValue(SensorType::Iat, 20);
	Sensor::setInvalidMockValue(SensorType::VehicleSpeed);

	updateChtCltEstimator();

	EXPECT_NEAR(130, Sensor::getOrZero(SensorType::Clt), 0.01);
}

TEST(ChtCltEstimatorTest, ThermostatLagProducesOvershootAndRecover) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->cltFromCht = true;
	getCustomPage()->cltEstThermostatTemp = 70;
	getCustomPage()->cltEstFan1Cfm = 0;
	getCustomPage()->cltEstFan2Cfm = 0;
	getCustomPage()->cltEstFallbackClt = 20;
	getCustomPage()->cltEstRadiatorFailTemp = 200;
	getCustomPage()->cltEstThermostatSpread = 5;  // narrow: valve target snaps open a few degrees past setpoint
	getCustomPage()->cltEstThermostatLag = 30;    // slow physical valve response

	// Strong heat rate (CHT pulls CLT up quickly) and strong rejection once the valve opens,
	// so the lagged valve genuinely lets CLT overshoot before rejection catches up.
	getCustomPage()->cltEstHeadTransferRate = 0.05f;
	getCustomPage()->cltEstHeadTransferGain = 0.0f;
	getCustomPage()->cltEstRadiatorBaseRejection = 0.05f;
	getCustomPage()->cltEstRadiatorAirflowGain = 0.0f;
	setFlatCurve(getCustomPage()->cltEstVssBins, getCustomPage()->cltEstVssAirflow, 0, 400, 0);
	setFlatCurve(getCustomPage()->cltEstIatBins, getCustomPage()->cltEstIatFactor, -40, 200, 1.0f);

	initChtCltEstimator();
	Sensor::resetMockValue(SensorType::Clt); // EngineTestHelper pre-mocks Clt=70; clear it so our estimator's registered value actually surfaces.

	Sensor::setMockValue(SensorType::Iat, 20);
	Sensor::setInvalidMockValue(SensorType::VehicleSpeed);

	// Seed cold (well below thermostat temp), then jump CHT to a sustained hot value so CLT
	// has to climb up through the thermostat's regulation point.
	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 40);
	updateChtCltEstimator();

	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 100);

	float peakClt = 0;
	for (int i = 0; i < 400; i++) {
		eth.moveTimeForwardSec(1);
		updateChtCltEstimator();
		peakClt = maxF(peakClt, Sensor::getOrZero(SensorType::Clt));
	}

	float finalClt = Sensor::getOrZero(SensorType::Clt);

	// The lagged valve should let CLT overshoot meaningfully above the thermostat setpoint...
	EXPECT_GT(peakClt, 75.0f);
	// ...then recover back down as rejection catches up, rather than settling at the peak --
	// impossible in a plain single-state monotonic model (this is the "hunt" the valve lag adds).
	EXPECT_LT(finalClt, peakClt - 1.0f);
}

TEST(ChtCltEstimatorTest, CoolantLagDelaysAirflowEffect) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->cltFromCht = true;
	getCustomPage()->cltEstThermostatTemp = 40; // low setpoint: valve is fully open throughout, isolating airflow lag
	getCustomPage()->cltEstFallbackClt = 20;
	getCustomPage()->cltEstRadiatorFailTemp = 200;
	getCustomPage()->cltEstThermostatSpread = 0;
	getCustomPage()->cltEstThermostatLag = 0;     // instant valve: only the coolant lag is under test
	getCustomPage()->cltEstCoolantLag = 20;        // slow coolant response to airflow changes

	// No heat rate at all, so any CLT movement below is purely a function of the rejection
	// term -- if rejectionRate itself doesn't ramp in over cltEstCoolantLag, this test proves
	// nothing about the lag and would still pass with an instant model.
	getCustomPage()->cltEstHeadTransferRate = 0.0f;
	getCustomPage()->cltEstHeadTransferGain = 0.0f;
	// Rejection rate must actually vary with airflow (not flat) for this test to be able to
	// tell a lagged airflow value apart from the raw instantaneous one. Base=0, gain chosen so
	// the fully-lagged-in 500 CFM fan airflow below lands exactly on a 0.05 1/s steady state.
	getCustomPage()->cltEstRadiatorBaseRejection = 0.0f;
	getCustomPage()->cltEstRadiatorAirflowGain = 0.0001f;
	setFlatCurve(getCustomPage()->cltEstVssBins, getCustomPage()->cltEstVssAirflow, 0, 400, 0);
	setFlatCurve(getCustomPage()->cltEstIatBins, getCustomPage()->cltEstIatFactor, -40, 200, 1.0f);
	getCustomPage()->cltEstFan1Cfm = 500; // 500 CFM * 0.0001 gain = 0.05 1/s once fully lagged in
	getCustomPage()->cltEstFan2Cfm = 0;

	initChtCltEstimator();
	Sensor::resetMockValue(SensorType::Clt); // EngineTestHelper pre-mocks Clt=70; clear it so our estimator's registered value actually surfaces.

	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 90);
	Sensor::setMockValue(SensorType::Iat, 20);
	Sensor::setInvalidMockValue(SensorType::VehicleSpeed);

	// Seed with the fan already off so the estimate settles well above thermostatTemp first.
	updateChtCltEstimator();

	// Step the fan on and immediately sample the reported rejection rate: with a 20s coolant
	// lag and a single ~1s step, it should be far below its eventual steady-state value, not
	// snapped instantly to it.
	enginePins.fanRelay.setValue(1);
	eth.moveTimeForwardSec(1);
	updateChtCltEstimator();
	float rejectionRateRightAfterStep = getChtCltEstimator().rejectionRate;

	// Let plenty of time pass at constant airflow so the lag filter (and CLT) fully settles.
	for (int i = 0; i < 300; i++) {
		eth.moveTimeForwardSec(1);
		updateChtCltEstimator();
	}
	float steadyStateRejectionRate = getChtCltEstimator().rejectionRate;

	EXPECT_NEAR(0.05f, steadyStateRejectionRate, 0.001f); // sanity: fan-on airflow does eventually reach its full rejection rate
	EXPECT_LT(rejectionRateRightAfterStep, steadyStateRejectionRate * 0.5f); // but not within one second of the step

	enginePins.fanRelay.setValue(0);
}
