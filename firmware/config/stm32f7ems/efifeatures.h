/**
 * @file efifeatures.h
 *
 * @brief In this header we can configure which firmware modules are used.
 *
 * STM32F7 config is inherited from STM32F4. This file contains only differences between F4 and F7.
 * This is more consistent way to maintain these config 'branches' and add new features.
 *
 * @date Aug 29, 2013
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

 #pragma once

#ifndef EFI_ADVANCED_FUEL_PUMP
#define EFI_ADVANCED_FUEL_PUMP TRUE
#endif

#ifndef EFI_VVT_COMPENSATION
#define EFI_VVT_COMPENSATION TRUE
#endif

#ifndef EFI_EXHAUST_CUTOUT
#define EFI_EXHAUST_CUTOUT TRUE
#endif

#ifndef EFI_DOWNSHIFT_BLIPPER
#define EFI_DOWNSHIFT_BLIPPER TRUE
#endif

#ifndef EFI_ENGINE_STATE_MACHINE
#define EFI_ENGINE_STATE_MACHINE TRUE
#endif

// Misfire Detection (Engine State Machine sub-feature, reads SM idle state)
#ifndef EFI_MISFIRE_DETECTION
#define EFI_MISFIRE_DETECTION TRUE
#endif

#ifndef EFI_CLUTCH_DELAY_VALVE
#define EFI_CLUTCH_DELAY_VALVE TRUE
#endif

#ifndef EFI_LAUNCH_POWER_RAMP
#define EFI_LAUNCH_POWER_RAMP TRUE
#endif

// Burst Knock (transient ignition timing pull on a TPS-rate stab)
#ifndef EFI_BURST_KNOCK
#define EFI_BURST_KNOCK TRUE
#endif

// WOT Time Enrichment (richen target AFR after prolonged WOT; needs Engine State Machine at runtime)
#ifndef EFI_WOT_ENRICHMENT
#define EFI_WOT_ENRICHMENT TRUE
#endif

// Sport Pedal (ETB pedal-to-throttle ratio shaping; needs EFI_ELECTRONIC_THROTTLE_BODY)
#ifndef EFI_SPORT_PEDAL
#define EFI_SPORT_PEDAL TRUE
#endif

#ifndef EFI_OFF_IDLE_RPM_ADDER
#define EFI_OFF_IDLE_RPM_ADDER TRUE
#endif

#ifndef EFI_LUA_LIMITER
#define EFI_LUA_LIMITER TRUE
#endif

// Disable ini ramdisk as a mitigation of https://github.com/rusefi/rusefi/issues/3775
// See STM32F7.ld for more info
#ifndef EFI_EMBED_INI_MSD
#define EFI_EMBED_INI_MSD FALSE
#endif

#ifndef KNOCK_SPECTROGRAM
#define KNOCK_SPECTROGRAM TRUE
#endif

#ifndef ENABLE_PERF_TRACE
#define ENABLE_PERF_TRACE TRUE
#endif

#ifndef EFI_CONSOLE_TX_BRAIN_PIN
// todo: kill default & move into board configuration?
#define EFI_CONSOLE_TX_BRAIN_PIN Gpio::D8
#endif

#ifndef EFI_CONSOLE_RX_BRAIN_PIN
#define EFI_CONSOLE_RX_BRAIN_PIN Gpio::D9
#endif

// see also EFI_EMBED_INI_MSD which is disabled above
#ifndef EFI_USE_COMPRESSED_INI_MSD
#define EFI_USE_COMPRESSED_INI_MSD TRUE
#endif

// UART driver not implemented on F7
#ifndef AUX_SERIAL_DEVICE
#define AUX_SERIAL_DEVICE (&SD6)
#endif

// todo: our "DMA-half" ChibiOS patch not implemented for USARTv2/STM32F7/STM32H7
#ifndef EFI_USE_UART_DMA
#define EFI_USE_UART_DMA FALSE
#endif

#ifndef FULL_SD_LOGS
#define FULL_SD_LOGS TRUE
#endif

// note order of include - first we set F7 defaults (above) and only later we apply F4 defaults
#include "../stm32f4ems/efifeatures.h"
