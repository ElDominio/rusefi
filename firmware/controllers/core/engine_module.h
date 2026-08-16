/**
 * @file engine_module.h
 *
 * Engine modules are the building blocks of engine-asynchronous control logic. A module
 * derives from EngineModule, overrides the lifecycle hooks it needs, and is registered in
 * either Engine::coreModules or the top-level AlphaXModuleList, both in engine.h; the
 * framework then invokes each hook on every registered module across both lists via
 * Engine::forEachModule()/aggregateModules() (see [tag:disable_engine_module] below).
 *
 * [tag:disable_engine_module] Making a module compile-time optional:
 *
 * 0. Decide CORE vs AlphaX: if the module is an "AlphaX custom subsystem" per
 *    FEATURE_FLAGS.md, its entry goes in AlphaXModuleList (non-CCM storage); otherwise it
 *    goes in Engine::coreModules (stays in the fixed-size CCM region on STM32F4 targets -
 *    keep this list lean). Either way, the module is reached the same way everywhere else:
 *    engine->module<T>() - never reach into either list by name directly.
 * 1. Pick an EFI_<NAME> feature flag and give it an #ifndef-guarded default in
 *    firmware/config/stm32f4ems/efifeatures.h (the master feature list - other ports and
 *    the simulator/unit_tests keep their own copies, see the caveat there).
 * 2. Wrap the module's entry in its list (engine.h) and the body of its .cpp in
 *    #if EFI_<NAME>. Keep the .h compilable either way so callers can be guarded locally.
 * 3. Guard call sites (status_loop, live_data, bench_test, storage, ...) with the same flag,
 *    consistently, everywhere the module is referenced - retrofitting a guard after the
 *    fact means auditing every consumer, which can be expensive (some existing AlphaX
 *    modules were left unconditionally-present specifically because of this cost - see the
 *    scope note on AlphaXModuleList in engine.h).
 *
 * EXTRA RULES when the module owns a TunerStudio page or any other .ini surface
 * (tooltips, table editors, gauges) - see [tag:ts_page_table]:
 *
 * - The feature flag becomes a "TS page guard flag". It must be declared in the board's
 *   prepend.txt (NOT in board.mk, NOT by editing efifeatures.h): prepend.txt is the only
 *   input the .ini generator can see, and firmware/Makefile lifts the flag from there into
 *   DDEFS so firmware and .ini always agree. Add the flag to TS_PAGE_GUARD_FLAGS in
 *   firmware/Makefile and a guardFlag row in TS_PAGES (TSProjectConsumer.java).
 * - Add a static_assert(EFI_<NAME> == <NAME>_PAGE_ENABLED, ...) next to the page table in
 *   tunerstudio.cpp, like LTFT does. If firmware and .ini ever disagree, the firmware
 *   answers reads of a page the .ini still lists with out_of_range (0x84) and TunerStudio
 *   retries that page until it drops the connection - the assert turns that into a build error.
 *
 * LongTermFuelTrim (long_term_fuel_trim.cpp, EFI_LTFT_CONTROL, TS page identifier 0x0200 aka
 * TS_PAGE_LTFT_TRIMS) is the worked example of an optional module that owns a TS page. Always
 * refer to pages by their wire pageIdentifier (TS_PAGE_* in tunerstudio.h), never by their
 * 1-based ordinal - ordinals are renumbered when a guarded page is dropped from the .ini.
 */

#pragma once

#include "engine_configuration.h"

class EngineModule {
public:
	// Called exactly once during boot, before configuration is loaded
	virtual void initNoConfiguration() { }

  virtual void setDefaultConfiguration() { }

	// Called when 'Burn' is invoked
	virtual void onConfigurationChange(engine_configuration_s const * /*previousConfig*/) { }

	// Called approx 20Hz
	virtual void onSlowCallback() { }

	// Called approx 200Hz
	virtual void onFastCallback() { }

	// Called when the engine stops. Reset your state, etc to prepare for the next start.
	virtual void onEngineStop() { }

	// Called whenever the ignition switch state changes
	virtual void onIgnitionStateChanged(bool /*ignitionOn*/) { }

	// Queried to determine whether this module needs a delayed shutoff, defaults to false
	virtual bool needsDelayedShutoff() { return false; }

	// Called on every successfully decoded tooth of the primary trigger
	virtual void onEnginePhase(float /*rpm*/,
								efitick_t /*edgeTimestamp*/,
								angle_t /*currentPhase*/,
								angle_t /*nextPhase*/)
								{ }
};
