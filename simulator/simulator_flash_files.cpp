/**
 * File-backed MCU flash for the host simulator.
 *
 * Live copies (the analog of the two internal-flash banks):
 *   simulator_flash/primary.bin
 *   simulator_flash/backup.bin
 * Each file is persistentState || page3, written atomically by intFlashWrite2().
 *
 * On every process start, a copy of whichever live file exists is stored as
 *   simulator_flash/tunes/tune-YYYYMMDD-HHMMSS.bin
 * so a restart leaves a versioned history without replacing the live banks.
 *
 * Missing files read as 0xFF, matching erased STM32 flash. Returning a short
 * or failed read as success used to CRC-fail every boot and look like the
 * tune was never saved.
 */

#include "pch.h"
#include "flash_int.h"
#include "simulator_flash_files.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

uintptr_t getFlashAddrFirstCopy() {
	return 1;
}

uintptr_t getFlashAddrSecondCopy() {
	return 2;
}

static std::string s_rootOverride;

void simulatorFlashSetRoot(const char *path) {
	if (path && *path) {
		s_rootOverride = path;
	}
}

static fs::path flashRoot() {
	// Explicit root first (in-process hosts pin it, see simulatorFlashSetRoot);
	// then the creation-time environment; the exe's relative default last.
	if (!s_rootOverride.empty()) {
		return fs::path(s_rootOverride);
	}
	if (const char *dir = std::getenv("EPICEFISIM_FLASH_DIR")) {
		if (*dir) {
			return fs::path(dir);
		}
	}
	return "simulator_flash";
}

static fs::path copyPath(flashaddr_t base) {
	const char *name = (base == getFlashAddrFirstCopy()) ? "primary.bin" : "backup.bin";
	return flashRoot() / name;
}

static void ensureFlashDirs() {
	std::error_code ec;
	fs::create_directories(flashRoot() / "tunes", ec);
}

/* storage_flash always reads/writes settings then page3 as a pair. Remember
 * the settings size so the page3 address (base + size) maps into the same file. */
static flashaddr_t s_pairBase = 0;
static size_t s_pairSettingsSize = 0;

static bool isCopyBase(flashaddr_t address) {
	return address == getFlashAddrFirstCopy() || address == getFlashAddrSecondCopy();
}

static bool resolveCopy(flashaddr_t address, size_t size, fs::path &path, size_t &offset) {
	if (isCopyBase(address)) {
		s_pairBase = address;
		s_pairSettingsSize = size;
		path = copyPath(address);
		offset = 0;
		return true;
	}
	if (s_pairBase != 0 && address == s_pairBase + s_pairSettingsSize) {
		path = copyPath(s_pairBase);
		offset = s_pairSettingsSize;
		return true;
	}
	return false;
}

static int writeFully(const fs::path &path, const char *data, size_t len) {
	ensureFlashDirs();
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) {
		printf("Simulator flash: failed to open %s for write\n", path.string().c_str());
		return FLASH_RETURN_OPERROR;
	}
	out.write(data, static_cast<std::streamsize>(len));
	out.flush();
	if (!out.good()) {
		printf("Simulator flash: short/failed write to %s\n", path.string().c_str());
		return FLASH_RETURN_OPERROR;
	}
	return FLASH_RETURN_SUCCESS;
}

static std::string bootStampName() {
	std::time_t now = std::time(nullptr);
	std::tm local{};
	std::tm *parsed = std::localtime(&now);
	if (parsed != nullptr) {
		local = *parsed;
	}
	char buf[32];
	std::strftime(buf, sizeof(buf), "tune-%Y%m%d-%H%M%S.bin", &local);
	return buf;
}

void simulatorFlashInitOnBoot() {
	ensureFlashDirs();
	auto absRoot = fs::absolute(flashRoot());
	printf("Simulator flash directory: %s\n", absRoot.string().c_str());

	fs::path src;
	std::error_code ec;
	auto primary = copyPath(getFlashAddrFirstCopy());
	auto backup = copyPath(getFlashAddrSecondCopy());
	if (fs::exists(primary, ec) && fs::file_size(primary, ec) > 0) {
		src = primary;
	} else if (fs::exists(backup, ec) && fs::file_size(backup, ec) > 0) {
		src = backup;
	}
	if (src.empty()) {
		printf("Simulator flash: blank (no primary.bin/backup.bin) - first boot or erased ECU\n");
		return;
	}

	std::string stamp = bootStampName();
	fs::path dest = flashRoot() / "tunes" / stamp;
	int suffix = 2;
	while (fs::exists(dest, ec) && suffix < 100) {
		std::string stem = stamp.substr(0, stamp.size() - 4);
		dest = flashRoot() / "tunes" / (stem + "-" + std::to_string(suffix++) + ".bin");
	}
	fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
	if (ec) {
		printf("Simulator flash: failed to snapshot %s -> %s (%s)\n",
			src.string().c_str(), dest.string().c_str(), ec.message().c_str());
		return;
	}
	printf("Simulator flash: boot snapshot %s (%llu bytes)\n",
		dest.string().c_str(),
		static_cast<unsigned long long>(fs::file_size(dest, ec)));
}

int intFlashErase(flashaddr_t address, size_t) {
	if (!isCopyBase(address)) {
		return FLASH_RETURN_SUCCESS;
	}
	auto path = copyPath(address);
	std::error_code ec;
	fs::remove(path, ec);
	printf("Simulator flash: erase %s\n", path.string().c_str());
	return FLASH_RETURN_SUCCESS;
}

int intFlashRead(flashaddr_t address, char *buffer, size_t size) {
	fs::path path;
	size_t offset = 0;
	if (!resolveCopy(address, size, path, offset)) {
		printf("Simulator flash: read cannot map address %llu size %zu\n",
			static_cast<unsigned long long>(address), size);
		memset(buffer, 0xFF, size);
		return FLASH_RETURN_OPERROR;
	}

	printf("Simulator flash: reading %s offset %zu (%zu bytes)\n",
		path.string().c_str(), offset, size);

	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		memset(buffer, 0xFF, size);
		return FLASH_RETURN_SUCCESS;
	}
	in.seekg(static_cast<std::streamoff>(offset));
	in.read(buffer, static_cast<std::streamsize>(size));
	auto got = static_cast<size_t>(in.gcount());
	if (got < size) {
		memset(buffer + got, 0xFF, size - got);
	}
	return FLASH_RETURN_SUCCESS;
}

// rusEFI's SettingStorageFlash asks this before reads and writes. Erased = every byte
// 0xFF, which a missing file or a short file reads as (see intFlashRead).
bool intFlashIsErased(flashaddr_t address, size_t size) {
	fs::path path;
	size_t offset = 0;
	if (!resolveCopy(address, size, path, offset)) {
		return true;
	}
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		return true;
	}
	in.seekg(static_cast<std::streamoff>(offset));
	char chunk[4096];
	size_t remaining = size;
	while (remaining > 0) {
		const size_t want = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
		in.read(chunk, static_cast<std::streamsize>(want));
		const size_t got = static_cast<size_t>(in.gcount());
		for (size_t i = 0; i < got; i++) {
			if (static_cast<uint8_t>(chunk[i]) != 0xFF) {
				return false;
			}
		}
		if (got < want) {
			return true;   // past the end of the file reads as erased
		}
		remaining -= got;
	}
	return true;
}

int intFlashWrite(flashaddr_t address, const char *buffer, size_t size) {
	fs::path path;
	size_t offset = 0;
	if (!resolveCopy(address, size, path, offset)) {
		printf("Simulator flash: write cannot map address %llu size %zu\n",
			static_cast<unsigned long long>(address), size);
		return FLASH_RETURN_OPERROR;
	}

	std::vector<char> existing;
	{
		std::ifstream in(path, std::ios::binary);
		if (in.is_open()) {
			in.seekg(0, std::ios::end);
			auto n = static_cast<size_t>(in.tellg());
			in.seekg(0);
			existing.resize(n);
			if (n > 0) {
				in.read(existing.data(), static_cast<std::streamsize>(n));
			}
		}
	}
	size_t need = offset + size;
	if (existing.size() < need) {
		existing.resize(need, static_cast<char>(0xFF));
	}
	memcpy(existing.data() + offset, buffer, size);
	printf("Simulator flash: writing %s offset %zu (%zu bytes, file %zu)\n",
		path.string().c_str(), offset, size, existing.size());
	return writeFully(path, existing.data(), existing.size());
}

int intFlashWrite2(flashaddr_t address, const char *buf1, const char *buf2,
		size_t size1, size_t size2) {
	if (!isCopyBase(address)) {
		printf("Simulator flash: write2 expected copy base, got %llu\n",
			static_cast<unsigned long long>(address));
		return FLASH_RETURN_OPERROR;
	}

	s_pairBase = address;
	s_pairSettingsSize = size1;

	std::vector<char> blob(size1 + size2);
	memcpy(blob.data(), buf1, size1);
	memcpy(blob.data() + size1, buf2, size2);

	auto path = copyPath(address);
	printf("Simulator flash: writing %s (%zu + %zu bytes)\n",
		path.string().c_str(), size1, size2);
	return writeFully(path, blob.data(), blob.size());
}

bool intFlashCompare(flashaddr_t address, const char *buffer, size_t size) {
	fs::path path;
	size_t offset = 0;
	if (!resolveCopy(address, size, path, offset)) {
		return false;
	}
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		return false;
	}
	in.seekg(static_cast<std::streamoff>(offset));
	char chunk[512];
	while (size > 0) {
		size_t want = size < sizeof(chunk) ? size : sizeof(chunk);
		in.read(chunk, static_cast<std::streamsize>(want));
		if (static_cast<size_t>(in.gcount()) != want || memcmp(chunk, buffer, want) != 0) {
			return false;
		}
		buffer += want;
		size -= want;
	}
	return true;
}
