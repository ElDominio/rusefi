// file custom_page.h
//
// TS page 5 — alphax custom features (currently the Downshift Blipper).
// Lives outside the main engine_configuration_s image to keep page 1 under the
// 64 KB TS page limit. Mirrors second_tables.{h,cpp} (TS page 4).

#pragma once

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
