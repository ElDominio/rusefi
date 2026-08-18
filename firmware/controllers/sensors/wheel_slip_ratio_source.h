#pragma once

expected<float> readWheelSpeedSource(wheel_speed_source_e source);

// True when the page-6 configurable Source1/Source2 selector is enabled and both sources
// are set, so this becomes the sole producer of SensorType::WheelSlipRatio. Callable
// unconditionally (returns false when EFI_WHEEL_SPEED_SENSORS is off) so the CAN-vendor and
// Aux-Speed producers can check it before registering, to avoid a duplicate registration.
bool isConfigurableWheelSlipRatioActive();
