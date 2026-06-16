/**
 * @file boards/hellen/alphax-s197-v2/board_configuration.cpp
 *
 * S197 Mustang custom board.
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

	// 4 Cam inputs for VVT support (Bank 1 & 2 Intake & Exhaust)
	engineConfiguration->camInputs[0] = Gpio::E13; // H144_IN_D_2 (STM32_CAM1)
	engineConfiguration->camInputs[1] = Gpio::E14; // H144_IN_D_3 (STM32_CAM2)
	engineConfiguration->camInputs[2] = Gpio::E15; // H144_IN_D_4 (STM32_CAM3)
	engineConfiguration->camInputs[3] = Gpio::E0;  // H144_UART8_RX (STM32_CAM4)

	// Sensor ADC Channels
	engineConfiguration->clt.adcChannel = H144_IN_CLT; // PC2 / CLT_OUT
	engineConfiguration->iat.adcChannel = H144_IN_IAT; // PC3 / IAT_OUT

	// TPS (Throttle Position Sensor) Inputs
	setTPS1Inputs(H144_IN_TPS, EFI_ADC_1); // TPSA on A17 (PA4), TPSB on A12 (PA1)

	// PPS (Pedal Position Sensor) Inputs
	setPPSInputs(H144_IN_PPS, EFI_ADC_7); // PPSA on A18 (PA3), PPSB on A20 (PA7)

	// Vehicle Speed Sensor
	engineConfiguration->vehicleSpeedSensorInputPin = Gpio::D6; // H144_UART2_RX (STM32_VSS)

	// Flex Fuel Sensor
	engineConfiguration->flexSensorPin = Gpio::D5; // H144_UART2_TX (STM32_FLEX)

	// MAP sensor
	engineConfiguration->map.sensor.hwChannel = H144_IN_MAP1; // PC0 / STM32_OMAP

	// Knock sensor is configured at compile-time in knock_config.h

	// Cylinder Head Temp & AC Pressure
	engineConfiguration->auxTempSensor1.adcChannel = EFI_ADC_14; // PC4 / CHT_OUT (A21)
	engineConfiguration->acPressure.hwChannel = EFI_ADC_15; // PC5 / AC_PRESS_OUT (A22)
	engineConfiguration->oilPressure.hwChannel = EFI_ADC_0; // PA0 / OILPRESS_OUT (A13)

	// O2 sensors
	engineConfiguration->afr.hwChannel = EFI_ADC_NONE;
	engineConfiguration->afr.hwChannel2 = EFI_ADC_NONE;

	engineConfiguration->auxLinear1.hwChannel = EFI_ADC_NONE;
	engineConfiguration->auxLinear2.hwChannel = EFI_ADC_NONE;
	engineConfiguration->auxLinear3.hwChannel = EFI_ADC_NONE;
	engineConfiguration->auxLinear4.hwChannel = EFI_ADC_NONE;


	// Switch inputs
	engineConfiguration->clutchDownPin = Gpio::A6; // PA6 / A19 (STM32_CLUTCHU)
	engineConfiguration->brakePedalPin = Gpio::B1; // PB1 / A24 (STM32_BRAKE)

}

static void alphax_s197_boardConfigOverrides() {
	setHellenMegaEnPin(); // PWR_EN (PE10)
	setHellenVbatt();     // VBAT (PA5) / VIGN (PA5)

	// No SPI/SD Card/Accelerometer
	// setHellenSdCardSpi3();
	// engineConfiguration->isSdCardEnabled = true;
	// hellenMegaAccelerometerPreInitCS2Pin();

	// CAN Configurations
	setHellenCan();

	// Force disable hidden/unconfigurable AuxLinear sensors to prevent pin conflicts (specifically PA2 for Knock control)
	engineConfiguration->auxLinear3.hwChannel = EFI_ADC_NONE;
	engineConfiguration->auxLinear4.hwChannel = EFI_ADC_NONE;
}

static void alphax_s197_boardDefaultConfiguration() {
	setInjectorPins();
	setIgnitionPins();

	// Electronic Throttle Body (ETB) TLE9201 configuration
	// PWM: OUT_PWM4 (PC8), DIR: OUT_PWM3 (PC7), DIS: SPI2_MOSI (PB15)
	setupTLE9201IncludingStepper(Gpio::C8, Gpio::C7, Gpio::B15);

	engineConfiguration->enableSoftwareKnock = true;

	// Relays and Solenoids
	engineConfiguration->mainRelayPin = Gpio::D7;    // PD7 / LED2 (STM32_MREL)
	engineConfiguration->fuelPumpPin = Gpio::D14;    // H144_OUT_PWM6 (STM32_FP)
	engineConfiguration->acRelayPin = Gpio::B14;     // H_SPI2_MISO (STM32_ACREL)
	engineConfiguration->fanPin = Gpio::A15;         // H_SPI3_CS (STM32_FAN)
	engineConfiguration->fan2Pin = Gpio::C11;        // H_SPI3_MISO (STM32_FAN2)
	engineConfiguration->alternatorControlPin = Gpio::C9; // H144_OUT_PWM5 (STM32_ALT)

	// Boost Control
	engineConfiguration->boostControlPin = Gpio::E9; // PE9 / A25 (STM32_BOOST)

	// VVT Solenoids
	engineConfiguration->vvtPins[0] = Gpio::D13; // H144_OUT_PWM1 (STM32_VVT1)
	engineConfiguration->vvtPins[1] = Gpio::C6;  // H144_OUT_PWM2 (STM32_VVT2)
	engineConfiguration->vvtPins[2] = Gpio::A10; // PA10 / USBID (STM32_VVT3)
	engineConfiguration->vvtPins[3] = Gpio::E8;  // H144_LED4_YELLOW (STM32_VVT4)

	setupDefaultSensorInputs();

	// V8 Engine default setup
	engineConfiguration->cylindersCount = 8;
	engineConfiguration->firingOrder = FO_1_5_4_2_6_3_7_8; // Mustang V8
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
	Gpio::D7,  // Main Relay
	Gpio::D14, // Fuel Pump
	Gpio::B14, // A/C Relay
	Gpio::A15, // Fan 1
	Gpio::C11, // Fan 2
	Gpio::C9,  // Alternator
	Gpio::E9,  // Boost
	Gpio::D13, // VVT 1
	Gpio::C6,  // VVT 2
	Gpio::A10, // VVT 3
	Gpio::E8,  // VVT 4
};

int getBoardMetaOutputsCount() {
	return efi::size(OUTPUTS);
}

Gpio* getBoardMetaOutputs() {
	return OUTPUTS;
}

int getBoardMetaDcOutputsCount() {
	return 2; // ETB H-Bridge
}

void setup_custom_board_overrides() {
	custom_board_DefaultConfiguration = alphax_s197_boardDefaultConfiguration;
	custom_board_ConfigOverrides = alphax_s197_boardConfigOverrides;
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
