/*
 * @file high_pressure_fuel_pump.cpp
 * @brief High Pressure Fuel Pump controller for GDI applications
 *
 * TL,DR: we have constant displacement mechanical pump driven by camshaft
 * here we control desired fuel high pressure by controlling relief/strain/spill valve electronically
 *
 * @date Nov 6, 2021
 * @author Scott Smith, (c) 2021
 */

/*
 * This file is part of rusEfi - see http://rusefi.com
 *
 * rusEfi is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * rusEfi is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "pch.h"

#include "high_pressure_fuel_pump.h"
#include "spark_logic.h"
#include "fuel_computer.h"

#if EFI_HPFP
#if !EFI_SHAFT_POSITION_INPUT
	fail("EFI_SHAFT_POSITION_INPUT required to have EFI_EMULATE_POSITION_SENSORS")
#endif

// A constant we use; doesn't seem important to hoist into engineConfiguration.
static constexpr int rpm_spinning_cutoff = 60; // Below this RPM, we don't run the logic

angle_t HpfpLobe::findNextLobe() {
    return 0;
}

// As a percent of the full pump stroke
float HpfpQuantity::calcFuelPercent(float rpm) {
    return 0.f;
}




float HpfpQuantity::calcPI(float rpm, float calc_fuel_percent) {
    return 0.f;
}

angle_t HpfpQuantity::pumpAngleFuel(float rpm, HpfpController *model) {
    return 0;
}

void HpfpController::onFastCallback() {
}

#define HPFP_CONTROLLER "hpfp"

void HpfpController::pinTurnOn(HpfpController *self) {
}

void HpfpController::pinTurnOff(HpfpController *self) {
}

void HpfpController::scheduleNextCycle() {
}

#endif // EFI_HPFP

bool isGdiEngine() {
    return false;
}