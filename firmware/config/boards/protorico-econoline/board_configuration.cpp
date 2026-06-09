/**
 * @file boards/hellen/protorico-econoline/board_configuration.cpp
 *
 * Protorico-Econoline custom board based on H144 (144-pin MCU module).
 *
 * @author Andrey Belomutskiy, (c) 2012-2024
 */

#include "pch.h"
#include "defaults.h"
#include "hellen_meta.h"
#include "board_overrides.h"

static void setInjectorPins() {
	// Mapped to injector low-side drivers
	engineConfiguration->injectionPins[0] = Gpio::D3;   // H144_OUT_IO1 / STM32_INJ1
	engineConfiguration->injectionPins[1] = Gpio::A9;   // H144_OUT_IO2 / STM32_INJ2
	engineConfiguration->injectionPins[2] = Gpio::D11;  // H144_LS_3     / STM32_INJ3
	engineConfiguration->injectionPins[3] = Gpio::D10;  // H144_LS_4     / STM32_INJ4
	engineConfiguration->injectionPins[4] = Gpio::D2;   // H144_OUT_IO5 / STM32_INJ5
	engineConfiguration->injectionPins[5] = Gpio::A8;   // H144_OUT_IO12 / STM32_INJ6
	engineConfiguration->injectionPins[6] = Gpio::D15;  // H144_OUT_PWM7 / STM32_INJ7
	engineConfiguration->injectionPins[7] = Gpio::D12;  // H144_OUT_PWM8 / STM32_INJ8
}

static void setIgnitionPins() {
	// Mapped to 5V ignition coil outputs
	engineConfiguration->ignitionPins[0] = Gpio::H144_IGN_1;  // PC13 / STM32_IGN1
	engineConfiguration->ignitionPins[1] = Gpio::H144_IGN_2;  // PE5  / STM32_IGN2
	engineConfiguration->ignitionPins[2] = Gpio::H144_IGN_3;  // PE4  / STM32_IGN3
	engineConfiguration->ignitionPins[3] = Gpio::H144_IGN_4;  // PE3  / STM32_IGN4
	engineConfiguration->ignitionPins[4] = Gpio::H144_IGN_5;  // PE2  / STM32_IGN5
	engineConfiguration->ignitionPins[5] = Gpio::H144_IGN_6;  // PB8  / STM32_IGN6
	engineConfiguration->ignitionPins[6] = Gpio::H144_IGN_7;  // PB9  / STM32_IGN7
	engineConfiguration->ignitionPins[7] = Gpio::H144_IGN_8;  // PE6  / STM32_IGN8
}

static void setupDefaultSensorInputs() {
	// Trigger inputs
	engineConfiguration->triggerInputPins[0] = Gpio::E12; // H144_IN_D_1 (STM32_CRANK)
	engineConfiguration->triggerInputPins[1] = Gpio::Unassigned;

	// Cam input
	engineConfiguration->camInputs[0] = Gpio::E13; // H144_IN_D_2 (STM32_CAM)
	engineConfiguration->camInputs[1] = Gpio::Unassigned;
	engineConfiguration->camInputs[2] = Gpio::Unassigned;
	engineConfiguration->camInputs[3] = Gpio::Unassigned;

	// Sensor ADC Channels
	engineConfiguration->clt.adcChannel = H144_IN_CLT; // PC2 / CLT_OUT
	engineConfiguration->iat.adcChannel = H144_IN_IAT; // PC3 / IAT_OUT

	// TPS (Throttle Position Sensor) Inputs
	setTPS1Inputs(H144_IN_TPS, EFI_ADC_NONE); // PA4 / IN_TPS

	// PPS (Pedal Position Sensor) Inputs
	setPPSInputs(EFI_ADC_NONE, EFI_ADC_NONE); // No pedal position sensors

	// MAP sensor
	engineConfiguration->map.sensor.hwChannel = H144_IN_MAP1; // PC0 / IN_MAP

	// Baro sensor
	engineConfiguration->baroSensor.hwChannel = H144_IN_MAP2; // PC1 / IN_MAP2

	// O2 sensors
	engineConfiguration->afr.hwChannel = EFI_ADC_NONE;
	engineConfiguration->afr.hwChannel2 = EFI_ADC_NONE;

	// Aux linear inputs (mapped to other analog inputs)
	engineConfiguration->auxLinear1.hwChannel = EFI_ADC_NONE; // PA7 / IN_AUX3 - Unset
	engineConfiguration->auxLinear2.hwChannel = EFI_ADC_14;   // PC4 / IN_AUX2 (A21)
	engineConfiguration->auxLinear3.hwChannel = EFI_ADC_NONE; // PA2 / IN_AUX4 - Unset
	engineConfiguration->auxLinear4.hwChannel = EFI_ADC_8;    // PB0 / IN_AUX1 (A23)

	// Switch inputs
	engineConfiguration->clutchDownPin = Gpio::A6; // PA6 / A19 (STM32_CLUTCHU)
	engineConfiguration->brakePedalPin = Gpio::B1; // PB1 / A24 (STM32_BRAKE)
}

static void protorico_econoline_boardConfigOverrides() {
	setHellenMegaEnPin(); // PWR_EN (PE10)
	setHellenVbatt();     // VBAT (PA5) / VIGN (PA5)

	// No SPI/SD Card/Accelerometer
	// setHellenSdCardSpi3();
	// engineConfiguration->isSdCardEnabled = true;
	// hellenMegaAccelerometerPreInitCS2Pin();

	// CAN Configurations
	setHellenCan();
	setHellenCan2();

	setDefaultHellenAtPullUps();
}

static void protorico_econoline_boardDefaultConfiguration() {
	setInjectorPins();
	setIgnitionPins();

	engineConfiguration->displayLogicLevelsInEngineSniffer = true;
	engineConfiguration->enableSoftwareKnock = true;

	// Relays and Solenoids
	engineConfiguration->mainRelayPin = Gpio::Unassigned;
	engineConfiguration->fuelPumpPin = Gpio::B14;         // H_SPI2_MISO (STM32_FP)
	engineConfiguration->acRelayPin = Gpio::C6;           // H144_OUT_PWM2 (STM32_AC_CL)
	engineConfiguration->fanPin = Gpio::Unassigned;
	engineConfiguration->fan2Pin = Gpio::Unassigned;
	engineConfiguration->alternatorControlPin = Gpio::Unassigned;
	engineConfiguration->boostControlPin = Gpio::Unassigned;
	engineConfiguration->tachOutputPin = Gpio::C7;             // H144_OUT_PWM3 (STM32_TACHO)
	engineConfiguration->malfunctionIndicatorPin = Gpio::C8; // H144_OUT_PWM4 (STM32_CEL)

	// Idle Valve
	engineConfiguration->idle.solenoidPin = Gpio::D13;    // H144_OUT_PWM1 (STM32_IDLE)

	setupDefaultSensorInputs();

	// V8 Engine default setup
	engineConfiguration->cylindersCount = 8;
	engineConfiguration->firingOrder = FO_1_3_7_2_6_5_4_8; // Ford Modular V8 firing order
	engineConfiguration->ignitionMode = IM_INDIVIDUAL_COILS;
	engineConfiguration->injectionMode = IM_SEQUENTIAL;

	setCrankOperationMode();
	setAlgorithm(engine_load_mode_e::LM_SPEED_DENSITY);

	// Disable SPI
	engineConfiguration->sdCardCsPin = Gpio::Unassigned;
	engineConfiguration->isSdCardEnabled = false;
	engineConfiguration->is_enabled_spi_1 = false;
	engineConfiguration->is_enabled_spi_2 = false;
	engineConfiguration->is_enabled_spi_3 = false;
	engineConfiguration->spi1mosiPin = Gpio::Unassigned;
	engineConfiguration->spi1misoPin = Gpio::Unassigned;
	engineConfiguration->spi1sckPin = Gpio::Unassigned;
	engineConfiguration->spi2mosiPin = Gpio::Unassigned;
	engineConfiguration->spi2misoPin = Gpio::Unassigned;
	engineConfiguration->spi2sckPin = Gpio::Unassigned;
	engineConfiguration->spi3mosiPin = Gpio::Unassigned;
	engineConfiguration->spi3misoPin = Gpio::Unassigned;
	engineConfiguration->spi3sckPin = Gpio::Unassigned;

	hellenWbo();
}

static Gpio OUTPUTS[] = {
	Gpio::D3,  // Injector 1
	Gpio::A9,  // Injector 2
	Gpio::D11, // Injector 3
	Gpio::D10, // Injector 4
	Gpio::D2,  // Injector 5
	Gpio::A8,  // Injector 6
	Gpio::D15, // Injector 7
	Gpio::D12, // Injector 8
	Gpio::C13, // Coil 1
	Gpio::E5,  // Coil 2
	Gpio::E4,  // Coil 3
	Gpio::E3,  // Coil 4
	Gpio::E2,  // Coil 5
	Gpio::B8,  // Coil 6
	Gpio::B9,  // Coil 7
	Gpio::E6,  // Coil 8
	Gpio::B14, // Fuel Pump
	Gpio::C6,  // A/C Clutch
	Gpio::C8,  // CEL
	Gpio::D13, // Idle
	Gpio::C7,  // Tacho
	Gpio::D7,  // EGR Solenoid
	Gpio::E8,  // TCI Control
	Gpio::A10, // IMRC Control
	Gpio::B15, // Line Pressure
	Gpio::A15, // O2 Heater
	Gpio::C11, // Torque Lockup
	Gpio::C12, // TLS115 PG
	Gpio::C10, // Purge Solenoid
	Gpio::C9,  // Solenoid A
	Gpio::D14, // Solenoid B
};

int getBoardMetaOutputsCount() {
	return efi::size(OUTPUTS);
}

Gpio* getBoardMetaOutputs() {
	return OUTPUTS;
}

int getBoardMetaDcOutputsCount() {
	return 0;
}

void setup_custom_board_overrides() {
	custom_board_DefaultConfiguration = protorico_econoline_boardDefaultConfiguration;
	custom_board_ConfigOverrides = protorico_econoline_boardConfigOverrides;
}

Gpio getCommsLedPin() {
	return Gpio::E7;
}

Gpio getRunningLedPin() {
	return Gpio::D7;
}

Gpio getWarningLedPin() {
	return Gpio::Unassigned;
}
