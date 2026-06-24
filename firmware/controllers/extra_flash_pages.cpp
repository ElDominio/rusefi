/*
* @file extra_flash_pages.cpp
 *
 * @date: apr 10, 2026
 * @author FDSoftware
 */

#include "pch.h"
#include "extra_flash_pages.h"
#include "second_tables.h"
#include "lua_config_page.h"
#include "custom_page.h"
#include "flash_main.h"
#include "persistent_configuration.h"

// Extra pages live at fixed offsets within the primary settings sector (defined in
// extra_flash_pages.h). A fixed offset is critical: if it were derived from
// sizeof(persistent_config_container_s) it would shift whenever the config struct grows,
// causing the read address to change across firmware updates and corrupting stored data.
// PAGE4_SECTOR_OFFSET (72 KB) satisfies STM32H7's 32-byte flash-word alignment and leaves
// headroom above the current largest config; the Lua page sits above it (see header).
static_assert(sizeof(persistent_config_container_s) <= PAGE4_SECTOR_OFFSET,
	"persistent_config_container_s exceeds PAGE4_SECTOR_OFFSET — increase the offset");

// Page 6 (AlphaX custom) lives above page 4 in the same shared settings sector. 80 KB (81920) is
// 32-byte aligned, clears page 4 (72 KB + page-4 container), and fits within a
// 128 KB sector alongside its small container.
// Page 5 (Lua script) is mapped by lua_config_page.cpp (LUA_PAGE_SECTOR_OFFSET in its header).
static constexpr size_t PAGE6_SECTOR_OFFSET = 80u * 1024u;
static_assert(PAGE4_SECTOR_OFFSET + sizeof(ExtraPageContainer<page4_s, 1>) <= PAGE6_SECTOR_OFFSET,
	"page 4 region overlaps PAGE6_SECTOR_OFFSET — increase the offset");

void resetExtraPages() {
	secondTablesSetDefaults();
	luaConfigPageSetDefaults();
	customPageSetDefaults();

	// When extracting a new config page from the main config, add a
	// resetXxx() call here
}

void loadExtraPages() {
	loadSecondTables();
	loadLuaConfigPage();
	loadCustomPage();

	// When extracting a new config page from the main config, add a
	// loadXxx() call here
}

void loadExtraPage(StorageItemId id) {
	if (id == EFI_SECOND_TABLES_RECORD_ID) {
		loadSecondTables();
	} else if (id == EFI_LUA_PAGE_RECORD_ID) {
		loadLuaConfigPage();
	} else if (id == EFI_CUSTOM_PAGE_RECORD_ID) {
		loadCustomPage();
	}

	// When extracting a new config page from the main config, add an
	// if/else-if branch here dispatching to loadXxx()
}

void burnExtraFlashPages() {
#if EFI_CONFIGURATION_STORAGE
	secondTablesPrepareForStorage();
	storageWrite(EFI_SECOND_TABLES_RECORD_ID,
		secondTablesGetStoragePtr(),
		secondTablesGetStorageSize());

	luaConfigPagePrepareForStorage();
	storageWrite(EFI_LUA_PAGE_RECORD_ID,
		luaConfigPageGetStoragePtr(),
		luaConfigPageGetStorageSize());

	customPagePrepareForStorage();
	storageWrite(EFI_CUSTOM_PAGE_RECORD_ID,
		customPageGetStoragePtr(),
		customPageGetStorageSize());

	// When extracting a new config page from the main config, add a
	// storageWrite() call here
#endif // EFI_CONFIGURATION_STORAGE
}

void* getExtraPageAddr(StorageItemId id) {
	if (id == EFI_SECOND_TABLES_RECORD_ID) {
		return secondTablesGetTsPage();
	} else if (id == EFI_LUA_PAGE_RECORD_ID) {
		return luaConfigPageGetTsPage();
	} else if (id == EFI_CUSTOM_PAGE_RECORD_ID) {
		return customPageGetTsPage();
	}

	// When extracting a new config page from the main config, add an
	// else-if branch here
	return nullptr;
}

size_t getExtraPageSize(StorageItemId id) {
	if (id == EFI_SECOND_TABLES_RECORD_ID) {
		return secondTablesGetTsPageSize();
	} else if (id == EFI_LUA_PAGE_RECORD_ID) {
		return luaConfigPageGetTsPageSize();
	} else if (id == EFI_CUSTOM_PAGE_RECORD_ID) {
		return customPageGetTsPageSize();
	}

	// When extracting a new config page from the main config, add an
	// else-if branch here
	return 0;
}

void burnExtraFlashPage(StorageItemId id) {
#if EFI_CONFIGURATION_STORAGE
#if (EFI_STORAGE_INT_FLASH == TRUE) && (EFI_STORAGE_MFS != TRUE) && !EFI_SIMULATOR
	// INT_FLASH boards: extra pages share a flash sector with the main config.
	// A full config burn erases the sector, then burnExtraFlashPages()
	// writes all extra pages into the now-blank region in one pass.
	// Use the deferred (non-forced) request, NOT writeToFlashNow(): on F4/F7 the
	// CPU freezes for ~1-2s during an internal-flash write, so the storage manager
	// postpones the write until the engine is stopped (storageAllowWriteID()).
	// This also lights TunerStudio's "unsaved changes" indicator until the burn
	// actually lands, matching how the main settings page (TS_PAGE_SETTINGS) burns.
	(void)id;
	setNeedToWriteConfiguration();
#else
	// MFS/SD boards or simulator: write the specific page directly to all backends.
	if (id == EFI_SECOND_TABLES_RECORD_ID) {
		secondTablesPrepareForStorage();
		storageWrite(EFI_SECOND_TABLES_RECORD_ID,
			secondTablesGetStoragePtr(),
			secondTablesGetStorageSize());
	} else if (id == EFI_LUA_PAGE_RECORD_ID) {
		luaConfigPagePrepareForStorage();
		storageWrite(EFI_LUA_PAGE_RECORD_ID,
			luaConfigPageGetStoragePtr(),
			luaConfigPageGetStorageSize());
	} else if (id == EFI_CUSTOM_PAGE_RECORD_ID) {
		customPagePrepareForStorage();
		storageWrite(EFI_CUSTOM_PAGE_RECORD_ID,
			customPageGetStoragePtr(),
			customPageGetStorageSize());
	}

	// When extracting a new config page from the main config, add an
	// if/else-if branch here: prepare the page and call storageWrite()
#endif
#else
	(void)id;
#endif // EFI_CONFIGURATION_STORAGE
}

size_t getExtraPageFlashOffset(StorageItemId id) {
	if (id == EFI_SECOND_TABLES_RECORD_ID) {
		return PAGE4_SECTOR_OFFSET;
	} else if (id == EFI_LUA_PAGE_RECORD_ID) {
		return LUA_PAGE_SECTOR_OFFSET;
	} else if (id == EFI_CUSTOM_PAGE_RECORD_ID) {
		return PAGE6_SECTOR_OFFSET;
	}

	// When adding a new extra page, add an else-if branch here
	// returning its sector offset.
	return 0;
}
