/*
 * @file	mazda_miata_vvt.cpp
 *
 * Miata NB2, also known as MX-5 Mk2.5
 *
 * Frankenso MAZDA_MIATA_2003
 * set engine_type 47
 *
 * coil1/4          (p1 +5 VP)    Gpio::E14
 * coil2/2          (p1 +5 VP)    Gpio::C9
 * tachometer +5 VP (p3 +12 VP)   Gpio::E8
 * alternator +5 VP (p3 +12 VP)   Gpio::E10
 * ETB PWM                        Gpio::E6 inverted low-side with pull-up
 * ETB dir1                       Gpio::E12
 * ETB dir2                       Gpio::C7
 *
 * COP ion #1                     Gpio::D8
 * COP ion #3                     Gpio::D9
 *
 * @date Oct 4, 2016
 * @author Andrey Belomutskiy, (c) 2012-2020
 * http://rusefi.com/forum/viewtopic.php?f=3&t=1095
 *
 *
 * See also TT_MAZDA_MIATA_VVT_TEST for trigger simulation
 *
 * Based on http://rusefi.com/wiki/index.php?title=Manual:Hardware_Frankenso_board#Default_Pinout
 *
 * board #70 - red car, hunchback compatible
 * set engine_type 55
 *
 * Crank   primary trigger        PA5 (3E in Miata board)       white
 * Cam     vvt input              PC6 (3G in Miata board)       blue
 * Wideband input                 PA3 (3J in Miata board)
 *
 * coil1/4          (p1 +5 VP)    Gpio::E14
 * coil2/2          (p1 +5 VP)    Gpio::C7
 *
 * tachometer +5 VP (p3 +12 VP)   Gpio::E8
 * alternator +5 VP (p3 +12 VP)   Gpio::E10
 *
 * VVT solenoid on aux PID#1      Gpio::E3
 * warning light                  Gpio::E6
 *
 *
 * idle solenoid                  PC13 on middle harness plug. diodes seem to be in the harness
 */

#include "pch.h"

#include "mazda_miata_vvt.h"
#include "custom_engine.h"
#include "mazda_miata_base_maps.h"


#include "proteus_meta.h"
#include "mre_meta.h"

void setMiataNB2_Proteus_TCU() {

}
void setMazdaMiataNB1() {
}

void setMazdaMiataNB2() {

}

void setMazdaMiataNB2_36() {
}
