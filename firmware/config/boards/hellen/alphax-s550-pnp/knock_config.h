#pragma once

// S550 knock is on PA0/PA1 (ADC2_IN0/IN1).
// ADC3 is reserved for slow sampling of the PF3-PF10 mux channels.
#define KNOCK_ADC ADCD2

// Knock 1 - PA1
#define KNOCK_ADC_CH1 ADC_CHANNEL_IN1
#define KNOCK_PIN_CH1 Gpio::A1

#define KNOCK_SAMPLE_TIME ADC_SAMPLE_84
#define KNOCK_SAMPLE_RATE (STM32_PCLK2 / (4 * (84 + 12)))

// Knock 2 - PA0
#define KNOCK_HAS_CH2
#define KNOCK_ADC_CH2 ADC_CHANNEL_IN0
#define KNOCK_PIN_CH2 Gpio::A0
