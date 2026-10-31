/**
 * Platform socket implementation for the C++ host simulator.
 *
 * Do not include pch.h here. On Windows, winsock2.h includes windows.h and collides with FatFS
 * types pulled in by the firmware precompiled header.
 */
#include "sim_net.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

bool simNetValid(sim_socket_t socket) {
	return socket != SIM_INVALID_SOCKET;
}

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>

static thread_local std::string lastErrorText;

static SOCKET toNative(sim_socket_t socket) {
	return static_cast<SOCKET>(socket);
}

// The Windows _execv fallback creates a new process. Sockets are inheritable by
// default, and the listen socket uses SO_EXCLUSIVEADDRUSE, so a leaked listener
// would make the re-exec'd simulator fail to bind its own port. Mirror the
// FD_CLOEXEC the POSIX branch sets.
static void setNoInherit(sim_socket_t socket) {
	SetHandleInformation(reinterpret_cast<HANDLE>(toNative(socket)), HANDLE_FLAG_INHERIT, 0);
}

void simNetInit() {
	static std::once_flag once;
	std::call_once(once, []() {
		WSADATA data{};
		int result = WSAStartup(MAKEWORD(2, 2), &data);
		if (result != 0) {
			printf("[sim] WSAStartup failed: %d\n", result);
		}
	});
}

const char* simNetLastError() {
	int error = WSAGetLastError();
	char* message = nullptr;
	DWORD length = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, static_cast<DWORD>(error), 0, reinterpret_cast<char*>(&message), 0, nullptr);
	if (length != 0 && message != nullptr) {
		lastErrorText.assign(message, length);
		while (!lastErrorText.empty()
				&& (lastErrorText.back() == '\r' || lastErrorText.back() == '\n')) {
			lastErrorText.pop_back();
		}
	} else {
		lastErrorText = "Winsock error " + std::to_string(error);
	}
	if (message != nullptr) {
		LocalFree(message);
	}
	return lastErrorText.c_str();
}

sim_socket_t simNetListen(int port) {
	simNetInit();
	SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket == INVALID_SOCKET) {
		return SIM_INVALID_SOCKET;
	}
	setNoInherit(static_cast<sim_socket_t>(socket));

	// SO_REUSEADDR permits duplicate listeners on Windows. This simulator must fail loudly when
	// another process owns its protocol port, so request the Windows-exclusive behavior instead.
	BOOL exclusive = TRUE;
	if (setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
			reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR) {
		int error = WSAGetLastError();
		closesocket(socket);
		WSASetLastError(error);
		return SIM_INVALID_SOCKET;
	}

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(static_cast<u_short>(port));
	if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR
			|| listen(socket, 1) == SOCKET_ERROR) {
		int error = WSAGetLastError();
		closesocket(socket);
		WSASetLastError(error);
		return SIM_INVALID_SOCKET;
	}
	return static_cast<sim_socket_t>(socket);
}

sim_socket_t simNetAccept(sim_socket_t listener) {
	SOCKET socket = accept(toNative(listener), nullptr, nullptr);
	if (socket == INVALID_SOCKET) {
		return SIM_INVALID_SOCKET;
	}
	setNoInherit(static_cast<sim_socket_t>(socket));
	return static_cast<sim_socket_t>(socket);
}

int simNetWaitReadable(sim_socket_t socket, int timeoutMs) {
	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(toNative(socket), &readSet);
	timeval timeout{};
	if (timeoutMs >= 0) {
		timeout.tv_sec = timeoutMs / 1000;
		timeout.tv_usec = (timeoutMs % 1000) * 1000;
	}
	return select(0, &readSet, nullptr, nullptr, timeoutMs >= 0 ? &timeout : nullptr);
}

int simNetRecv(sim_socket_t socket, void* buffer, int length) {
	int result = recv(toNative(socket), static_cast<char*>(buffer), length, 0);
	return result == SOCKET_ERROR ? -1 : result;
}

int simNetSend(sim_socket_t socket, const void* buffer, int length) {
	int result = send(toNative(socket), static_cast<const char*>(buffer), length, 0);
	return result == SOCKET_ERROR ? -1 : result;
}

void simNetClose(sim_socket_t socket) {
	if (simNetValid(socket)) {
		closesocket(toNative(socket));
	}
}

int simReexec(char** argv) {
	if (argv == nullptr || argv[0] == nullptr) {
		errno = EINVAL;
		return -1;
	}
	char path[MAX_PATH];
	DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) {
		errno = ENOENT;
		return -1;
	}
	_execv(path, argv);
	return -1;
}

#else

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

static void setCloexec(int fd) {
	int flags = fcntl(fd, F_GETFD);
	if (flags >= 0) {
		fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
	}
}

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

void simNetInit() {
}

const char* simNetLastError() {
	return strerror(errno);
}

sim_socket_t simNetListen(int port) {
	int socket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (socket < 0) {
		return SIM_INVALID_SOCKET;
	}
	setCloexec(socket);
	int reuse = 1;
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(static_cast<uint16_t>(port));
	if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0
			|| listen(socket, 1) < 0) {
		int error = errno;
		close(socket);
		errno = error;
		return SIM_INVALID_SOCKET;
	}
	return socket;
}

sim_socket_t simNetAccept(sim_socket_t listener) {
#if defined(__linux__)
	// Android's app sandbox seccomp filter allows accept4 but not the bare
	// accept syscall (arm64 nr 202) -- bionic never emits it, static musl
	// does, and the filter SIGSYS-kills on sight (proven on-device). And
	// musl's own accept4() short-circuits to bare accept() when flags are
	// zero ("cbz w3" straight into accept, verified in the 1.2.5
	// disassembly), so the flag must be nonzero: SOCK_CLOEXEC is what
	// setCloexec() below used to add after the fact, just racily. macOS has
	// no accept4, so keep plain accept there.
	int socket = accept4(static_cast<int>(listener), nullptr, nullptr, SOCK_CLOEXEC);
#else
	int socket = accept(static_cast<int>(listener), nullptr, nullptr);
#endif
	if (socket < 0) {
		return SIM_INVALID_SOCKET;
	}
	setCloexec(socket);
	return socket;
}

int simReexec(char** argv) {
	if (argv == nullptr || argv[0] == nullptr) {
		errno = EINVAL;
		return -1;
	}
	char path[4096];
	const char* exe = nullptr;
	ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (n > 0) {
		path[n] = '\0';
		exe = path;
	}
#if defined(__APPLE__)
	if (exe == nullptr) {
		uint32_t size = sizeof(path);
		if (_NSGetExecutablePath(path, &size) == 0) {
			exe = path;
		}
	}
#endif
	if (exe == nullptr && argv[0][0] == '/') {
		exe = argv[0];
	}
	if (exe == nullptr) {
		errno = ENOENT;
		return -1;
	}
	execv(exe, argv);
	return -1;
}

int simNetWaitReadable(sim_socket_t socket, int timeoutMs) {
	int fd = static_cast<int>(socket);
	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(fd, &readSet);
	timeval timeout{};
	if (timeoutMs >= 0) {
		timeout.tv_sec = timeoutMs / 1000;
		timeout.tv_usec = (timeoutMs % 1000) * 1000;
	}
	return select(fd + 1, &readSet, nullptr, nullptr, timeoutMs >= 0 ? &timeout : nullptr);
}

int simNetRecv(sim_socket_t socket, void* buffer, int length) {
	// recvfrom/sendto, not recv/send: the bare syscalls (arm64 nr 212/211)
	// are outside Android's app allowlist for the same reason as accept.
	return static_cast<int>(recvfrom(static_cast<int>(socket), buffer,
			static_cast<size_t>(length), 0, nullptr, nullptr));
}

int simNetSend(sim_socket_t socket, const void* buffer, int length) {
	return static_cast<int>(sendto(static_cast<int>(socket), buffer,
		static_cast<size_t>(length), MSG_NOSIGNAL, nullptr, 0));
}

void simNetClose(sim_socket_t socket) {
	if (simNetValid(socket)) {
		close(static_cast<int>(socket));
	}
}

#endif
