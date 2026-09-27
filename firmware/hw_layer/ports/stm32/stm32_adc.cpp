/**
 * @file	stm32_common_adc.cpp
 * @brief	Low level common STM32 code
 *
 * @date Mar 28, 2019
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#include "pch.h"
#include "efilib.h"
#include "adc_offchip.h"

#if HAL_USE_ADC

#include "stm32_adc_channel_map.h"

brain_pin_e getAdcChannelBrainPin(const char *msg, adc_channel_e hwChannel) {
    static_assert(EFI_ADC_NONE == ADC_CHANNEL_NONE);

    if (isAdcChannelOffChip(hwChannel)) {
        return adcOffchipGetPin(hwChannel);
    }

    /* Muxed adc inputs */
    hwChannel = adcMuxedGetParent(hwChannel);

    for (size_t idx = 0; idx < efi::size(adcChannels); idx++) {
        if (adcChannels[idx].ch == hwChannel) {
            return adcChannels[idx].pin;
        }
    }

    /* todo: what is upper range ADC is used while lower range ADC is not used? how do we still mark pin used?
     * external muxes for internal ADC #3350
     * firmwareError(ObdCode::CUSTOM_ERR_ADC_UNKNOWN_CHANNEL, "Unknown hw channel %d [%s]", hwChannel, msg);
     */
    (void)msg;

    return Gpio::Invalid;
}

bool adcIsMuxedInput(adc_channel_e hwChannel) {
#ifdef ADC_MUX_PIN
    if ((hwChannel >= EFI_ADC_16) && (hwChannel <= EFI_ADC_31)) {
        return true;
    }
#ifdef ADC3_SLOW_CHANNEL_COUNT
    if ((hwChannel >= EFI_ADC_40) && (hwChannel < static_cast<adc_channel_e>(EFI_ADC_40 + ADC3_SLOW_CHANNEL_COUNT))) {
        return true;
    }
#elif defined(EFI_SLOW_ADC) && (EFI_SLOW_ADC == ADCD3)
    // Boards with EFI_SLOW_ADC redirected onto ADCD3 have no dedicated ADC3_SLOW_CHANNEL_COUNT
    // group, but still populate EFI_ADC_40-47 as the muxed counterparts of EFI_ADC_32-39 - see
    // the "EFI_SLOW_ADC == ADCD3" aliasing block in stm32_adc_v2.cpp::readSlowAnalogInputs().
    if ((hwChannel >= EFI_ADC_40) && (hwChannel <= EFI_ADC_47)) {
        return true;
    }
#endif
    return false;
#else
    UNUSED(hwChannel);
    return false;
#endif
}

// If mux is not enabled or channel is already a root channel - return itself
adc_channel_e adcMuxedGetParent(adc_channel_e hwChannel)
{
#ifdef ADC_MUX_PIN
    // ADC1 muxed channels: EFI_ADC_16-31 -> EFI_ADC_0-15
    if ((hwChannel >= EFI_ADC_16) && (hwChannel <= EFI_ADC_31)) {
        return (adc_channel_e)(EFI_ADC_0 + (hwChannel - EFI_ADC_16));
    }
#ifdef ADC3_SLOW_CHANNEL_COUNT
    // ADC3 muxed channels: EFI_ADC_40-47 -> EFI_ADC_32-39
    if ((hwChannel >= EFI_ADC_40) && (hwChannel < static_cast<adc_channel_e>(EFI_ADC_40 + ADC3_SLOW_CHANNEL_COUNT))) {
        return (adc_channel_e)(EFI_ADC_32 + (hwChannel - EFI_ADC_40));
    }
#elif defined(EFI_SLOW_ADC) && (EFI_SLOW_ADC == ADCD3)
    // See matching comment in adcIsMuxedInput() above.
    if ((hwChannel >= EFI_ADC_40) && (hwChannel <= EFI_ADC_47)) {
        return (adc_channel_e)(EFI_ADC_32 + (hwChannel - EFI_ADC_40));
    }
#endif
#endif
    return hwChannel;
}

adc_channel_e getAdcChannel(brain_pin_e pin) {
    if (pin == Gpio::Unassigned)
        return EFI_ADC_NONE;

    for (size_t idx = 0; idx < efi::size(adcChannels); idx++) {
        if (adcChannels[idx].pin == pin) {
            return adcChannels[idx].ch;
        }
    }

    criticalError("getAdcChannel %d", pin);
    return EFI_ADC_ERROR;
}

// Get ADC internal input index for given hwChannel
int getAdcInternalChannel(ADC_TypeDef *adc, adc_channel_e hwChannel)
{
    uint8_t mask = 0;

#if STM32_ADC_USE_ADC1
    if (adc == ADC1) {
        mask = BIT(0);
    }
#endif
#if STM32_ADC_USE_ADC2
    if (adc == ADC2) {
        mask = BIT(1);
    }
#endif
#if STM32_ADC_USE_ADC3
    if (adc == ADC3) {
        mask = BIT(2);
    }
#endif

    if (mask == 0) {
        // Unknown ADC instance
        return -1;
    }

    return getStm32AdcInternalChannel(mask, hwChannel);
}

adc_channel_e getHwChannelForAdcInput(ADC_TypeDef *adc, size_t hwIndex)
{
    uint8_t mask = 0;

#if STM32_ADC_USE_ADC1
    if (adc == ADC1) {
        mask = BIT(0);
    }
#endif
#if STM32_ADC_USE_ADC2
    if (adc == ADC2) {
        mask = BIT(1);
    }
#endif
#if STM32_ADC_USE_ADC3
    if (adc == ADC3) {
        mask = BIT(2);
    }
#endif

    if (mask == 0) {
        // Unknown ADC instance
        return EFI_ADC_ERROR;
    }

    size_t tmpIndex = 0;
    for (size_t idx = 0; idx < efi::size(adcChannels); idx++) {
        if (adcChannels[idx].adc & mask) {
            if (hwIndex == tmpIndex) {
                return adcChannels[idx].ch;
            }
            tmpIndex++;
        }
    }

    // Channel is not supported by this ADC
    return EFI_ADC_ERROR;

}

// deprecated - inline?
ioportid_t getAdcChannelPort(const char *msg, adc_channel_e hwChannel) {
    brain_pin_e brainPin = getAdcChannelBrainPin(msg, hwChannel);
    return getHwPort(msg, brainPin);
}

// deprecated - inline?
int getAdcChannelPin(adc_channel_e hwChannel) {
    brain_pin_e brainPin = getAdcChannelBrainPin("get_pin", hwChannel);
    return getHwPin("get_pin", brainPin);
}

#endif /* HAL_USE_ADC */
