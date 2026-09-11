#pragma once
#include "sensor.h"

class GearDetector : public EngineModule, public Sensor {
public:
	GearDetector();
	~GearDetector();

	void onSlowCallback() override;
	void onConfigurationChange(engine_configuration_s const * /*previousConfig*/) override;

	float getGearboxRatio() const;

	// Returns 0 for neutral, 1 for 1st, 5 for 5th, etc.
	size_t determineGearFromRatio(float ratio) const;

	float getRpmInGear(size_t gear) const;

	// Percent deviation of the configured Slip RPM Source from the RPM expected for the
	// Detected Gear. 0 = no slip, positive = source spinning faster than expected. Only valid
	// (nonzero) while Slip Detection is enabled and the driveshaft speed reference is a real
	// Output Shaft Speed sensor -- see transmissionSlipDetectionEnabled.
	float getSlipPercent() const;

	SensorResult get() const override;
	void showInfo(const char* sensorName) const override;

private:
	float computeGearboxRatio() const;
	float computeSlipPercent() const;
	float getDriveshaftRpm() const;
	void initGearDetector();
    bool isInitialized = false;

	float m_gearboxRatio = 0;
	float m_slipPercent = 0;
	size_t m_currentGear = 0;

	float m_gearThresholds[TCU_GEAR_COUNT - 1];
};
