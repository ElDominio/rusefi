// file custom_page.h
//
// TS page 5 — alphax custom features (currently the Downshift Blipper).
// Lives outside the main engine_configuration_s image to keep page 1 under the
// 64 KB TS page limit. Mirrors second_tables.{h,cpp} (TS page 4).

#pragma once

// Sport Pedal multiplier curve length. Defined here (rather than in a module header like the
// other page-5 curves) because Sport Pedal has no dedicated module — its logic lives in
// electronic_throttle.cpp. Must match SPORT_PEDAL_SIZE in integration/config_page_5.txt and be
// visible before the generated struct below uses it.
#define SPORT_PEDAL_SIZE 8

// Injector Small Pulse % Correction Curve length. Must match INJ_CURVE_SIZE in config_page_5.txt.
#define INJ_CURVE_SIZE 12

#include "page_5_generated.h"

page5_s* getCustomPage();

void loadCustomPage();

// Set default values.
void customPageSetDefaults();

// Returns true if the in-RAM container has a valid version + CRC.
bool customPageIsValid();

// TunerStudio interface — returns the raw page5_s data (no container wrapper).
void* customPageGetTsPage();
size_t customPageGetTsPageSize();

// Storage interface — returns the CRC-wrapped container for persistence.
// Call customPagePrepareForStorage() first to compute the CRC before writes.
void customPagePrepareForStorage();
uint8_t* customPageGetStoragePtr();
size_t customPageGetStorageSize();
