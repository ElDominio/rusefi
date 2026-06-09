#pragma once

// knock 1 - pin PA0
#define KNOCK_PIN_CH1 Gpio::A0

// Knock is on ADC3
#define KNOCK_ADC ADCD3
#define KNOCK_ADC_CH1 ADC_CHANNEL_IN0

#define KNOCK_SAMPLE_TIME ADC_SAMPLE_84
#define KNOCK_SAMPLE_RATE (STM32_PCLK2 / (4 * (84 + 12)))
