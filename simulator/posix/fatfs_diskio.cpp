/** Host-file-backed FatFS disk for the C++ simulator. */
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <mutex>

#include "ff.h"
#include "diskio.h"

static constexpr const char* IMAGE_PATH = "rusefi_simulator_sd.img";
static constexpr uint32_t SECTOR_SIZE = 512;
static constexpr uint32_t SECTOR_COUNT = 65536; // 32 MiB, large enough for FAT16

static FILE* image;
static std::mutex imageMutex;
static std::mutex filesystemMutexes[FF_VOLUMES + 1];

static bool seekTo(uint64_t offset) {
#if defined(_WIN32)
	return _fseeki64(image, static_cast<long long>(offset), SEEK_SET) == 0;
#else
	return fseeko(image, static_cast<off_t>(offset), SEEK_SET) == 0;
#endif
}

extern "C" {

DSTATUS disk_status(BYTE drive) {
	if (drive != 0) {
		return STA_NODISK;
	}
	return image == nullptr ? STA_NOINIT : 0;
}

DSTATUS disk_initialize(BYTE drive) {
	if (drive != 0) {
		return STA_NODISK;
	}
	std::lock_guard<std::mutex> lock(imageMutex);
	if (image != nullptr) {
		return 0;
	}

	image = fopen(IMAGE_PATH, "r+b");
	if (image == nullptr) {
		image = fopen(IMAGE_PATH, "w+b");
	}
	if (image == nullptr) {
		perror("sim FatFS: open image");
		return STA_NOINIT;
	}

	if (fseek(image, 0, SEEK_END) != 0) {
		perror("sim FatFS: seek image end");
		fclose(image);
		image = nullptr;
		return STA_NOINIT;
	}
#if defined(_WIN32)
	auto rawSize = _ftelli64(image);
#else
	auto rawSize = ftello(image);
#endif
	if (rawSize < 0) {
		perror("sim FatFS: get image size");
		fclose(image);
		image = nullptr;
		return STA_NOINIT;
	}
	uint64_t size = static_cast<uint64_t>(rawSize);
	uint64_t required = static_cast<uint64_t>(SECTOR_COUNT) * SECTOR_SIZE;
	if (size < required) {
		const char zero = 0;
		if (!seekTo(required - 1) || fwrite(&zero, 1, 1, image) != 1 || fflush(image) != 0) {
			perror("sim FatFS: extend image");
			fclose(image);
			image = nullptr;
			return STA_NOINIT;
		}
	}
	return 0;
}

DRESULT disk_read(BYTE drive, BYTE* buffer, LBA_t sector, UINT count) {
	if (drive != 0) {
		return RES_NOTRDY;
	}
	if (buffer == nullptr || count == 0 || sector >= SECTOR_COUNT
			|| count > SECTOR_COUNT - sector) {
		return RES_PARERR;
	}
	std::lock_guard<std::mutex> lock(imageMutex);
	if (image == nullptr) {
		return RES_NOTRDY;
	}
	uint64_t offset = static_cast<uint64_t>(sector) * SECTOR_SIZE;
	size_t bytes = static_cast<size_t>(count) * SECTOR_SIZE;
	if (!seekTo(offset)) {
		return RES_ERROR;
	}
	return fread(buffer, 1, bytes, image) == bytes ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE drive, const BYTE* buffer, LBA_t sector, UINT count) {
	if (drive != 0) {
		return RES_NOTRDY;
	}
	if (buffer == nullptr || count == 0 || sector >= SECTOR_COUNT
			|| count > SECTOR_COUNT - sector) {
		return RES_PARERR;
	}
	std::lock_guard<std::mutex> lock(imageMutex);
	if (image == nullptr) {
		return RES_NOTRDY;
	}
	uint64_t offset = static_cast<uint64_t>(sector) * SECTOR_SIZE;
	size_t bytes = static_cast<size_t>(count) * SECTOR_SIZE;
	if (!seekTo(offset)) {
		return RES_ERROR;
	}
	return fwrite(buffer, 1, bytes, image) == bytes ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE drive, BYTE command, void* buffer) {
	if (drive != 0) {
		return RES_NOTRDY;
	}
	std::lock_guard<std::mutex> lock(imageMutex);
	if (image == nullptr) {
		return RES_NOTRDY;
	}
	switch (command) {
	case CTRL_SYNC:
		return fflush(image) == 0 ? RES_OK : RES_ERROR;
	case GET_SECTOR_COUNT:
		if (buffer == nullptr) return RES_PARERR;
		*static_cast<LBA_t*>(buffer) = SECTOR_COUNT;
		return RES_OK;
	case GET_SECTOR_SIZE:
		if (buffer == nullptr) return RES_PARERR;
		*static_cast<WORD*>(buffer) = SECTOR_SIZE;
		return RES_OK;
	case GET_BLOCK_SIZE:
		if (buffer == nullptr) return RES_PARERR;
		*static_cast<DWORD*>(buffer) = 1;
		return RES_OK;
	case CTRL_TRIM:
		return RES_OK;
	default:
		return RES_PARERR;
	}
}

void* ff_memalloc(UINT size) {
	return malloc(static_cast<size_t>(size));
}

void ff_memfree(void* block) {
	free(block);
}

int ff_mutex_create(int volume) {
	return volume >= 0 && volume <= FF_VOLUMES;
}

void ff_mutex_delete(int) {
}

int ff_mutex_take(int volume) {
	if (volume < 0 || volume > FF_VOLUMES) {
		return 0;
	}
	filesystemMutexes[volume].lock();
	return 1;
}

void ff_mutex_give(int volume) {
	if (volume >= 0 && volume <= FF_VOLUMES) {
		filesystemMutexes[volume].unlock();
	}
}

DWORD get_fattime() {
	time_t now = time(nullptr);
	struct tm local{};
#if defined(_WIN32)
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	return (static_cast<DWORD>(local.tm_year - 80) << 25)
		| (static_cast<DWORD>(local.tm_mon + 1) << 21)
		| (static_cast<DWORD>(local.tm_mday) << 16)
		| (static_cast<DWORD>(local.tm_hour) << 11)
		| (static_cast<DWORD>(local.tm_min) << 5)
		| (static_cast<DWORD>(local.tm_sec) >> 1);
}

} // extern "C"

static FATFS filesystem;

void simFatfsMount() {
	static bool mounted;
	if (mounted) {
		return;
	}

	FRESULT result = f_mount(&filesystem, "", 1);
	if (result == FR_NO_FILESYSTEM) {
		printf("[sim] FatFS: formatting %s\n", IMAGE_PATH);
		static BYTE work[4 * SECTOR_SIZE];
		MKFS_PARM options{};
		options.fmt = FM_FAT;
		result = f_mkfs("", &options, work, sizeof(work));
		if (result == FR_OK) {
			result = f_mount(&filesystem, "", 1);
		}
	}
	if (result != FR_OK) {
		printf("[sim] FatFS: mount failed (%d)\n", static_cast<int>(result));
		return;
	}
	mounted = true;
}
