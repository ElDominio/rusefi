#include "pch.h"

#if defined(EFI_SIMULATOR_POSIX)

#include "tunerstudio.h"
#include "tunerstudio_io.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>
#include "sim_net.h"

#ifndef SIM_SD1_PORT
#define SIM_SD1_PORT 29001
#endif
#ifndef SIM_SD2_PORT
#define SIM_SD2_PORT 29002
#endif

extern int isSerialOverTcpReady;

// Bind-failure policy. The standalone exe exits so its launcher can report the
// failure (BUG-legacy-744: abort() is a crash dialog on Windows, and a port-in-use
// is not a failed assert). An in-process DLL boot must not kill its host, so
// ecu_init() flips the policy before booting and reports the failure via its
// return code instead.
static bool s_tsBindIsFatal = true;
static bool s_tsBindFailed = false;

void setTsBindFailureIsFatal(bool fatal) {
	s_tsBindIsFatal = fatal;
}

bool tsBindFailureReported() {
	return s_tsBindFailed;
}

static void reportTsBindFailure() {
	s_tsBindFailed = true;
	if (s_tsBindIsFatal) {
		std::exit(1);
	}
}

class TcpTsChannel : public SerialTsChannelBase {
public:
	TcpTsChannel(const char* channel_name, int port, bool primary)
		: SerialTsChannelBase(channel_name)
		, m_port(port)
		, m_primary(primary) {}

	void start(uint32_t /*baud*/) override {
		// Allow a test instance with isolated flash to run beside the live ECU.
		const char* variable = m_primary ? "EPICEFISIM_TS1_PORT" : "EPICEFISIM_TS2_PORT";
		if (const char* text = std::getenv(variable)) {
			char* end = nullptr;
			long value = std::strtol(text, &end, 10);
			if (!*text || *end || value < 1 || value > 65535) {
				printf("[sim] invalid %s port\n", variable);
				reportTsBindFailure();
				return;
			}
			m_port = static_cast<int>(value);
		}
		m_listenFd = simNetListen(m_port);
		if (!simNetValid(m_listenFd)) {
			printf("[sim] TCP TS: listen(%d) failed: %s\n", m_port, simNetLastError());
			fflush(stdout);
			reportTsBindFailure();
			return;
		}

		printf("[sim] TCP TS: listening on port %d\n", m_port);

		m_acceptThread = std::thread([this]() { acceptLoop(); });
		m_acceptThread.detach();
	}

	void stop() override {
		m_stop = true;
		closeClient();
		sim_socket_t listener = m_listenFd.exchange(SIM_INVALID_SOCKET);
		if (simNetValid(listener)) {
			simNetClose(listener);
		}
	}

	void write(const uint8_t* buffer, size_t size, bool /*isEndOfPacket*/) override {
		sim_socket_t fd = m_clientFd.load();
		if (!simNetValid(fd)) {
			return;
		}
		simNetSend(fd, buffer, static_cast<int>(size));
	}

	size_t readTimeout(uint8_t* buffer, size_t size, int timeout) override {
		sim_socket_t fd = m_clientFd.load();
		if (!simNetValid(fd)) {
			// no client
			return 0;
		}

		int timeoutMs = 0;
		if (timeout == static_cast<int>(TIME_INFINITE)) {
			timeoutMs = -1;
		} else {
			timeoutMs = static_cast<int>((static_cast<uint64_t>(timeout) * 1000ULL) / CH_CFG_ST_FREQUENCY);
		}

		if (simNetWaitReadable(fd, timeoutMs) <= 0) {
			return 0;
		}

		int n = simNetRecv(fd, buffer, static_cast<int>(size));
		if (n <= 0) {
			closeClient();
			return 0;
		}
		return static_cast<size_t>(n);
	}

	bool isReady() const override {
		return simNetValid(m_clientFd.load());
	}

private:
	void acceptLoop() {
		while (!m_stop) {
			sim_socket_t fd = simNetAccept(m_listenFd.load());
			if (!simNetValid(fd)) {
				if (m_stop) {
					return;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				continue;
			}
			closeClient();
			m_clientFd = fd;
			if (m_primary) {
				isSerialOverTcpReady = TRUE;
			}
		}
	}

	void closeClient() {
		sim_socket_t fd = m_clientFd.exchange(SIM_INVALID_SOCKET);
		if (simNetValid(fd)) {
			simNetClose(fd);
			if (m_primary) {
				isSerialOverTcpReady = FALSE;
			}
		}
	}

	int m_port;
	bool m_primary;
	std::atomic<sim_socket_t> m_clientFd{SIM_INVALID_SOCKET};
	std::atomic<sim_socket_t> m_listenFd{SIM_INVALID_SOCKET};
	std::atomic<bool> m_stop{false};
	std::thread m_acceptThread;
};

class PrimaryTcpThread : public TunerstudioThread {
public:
	PrimaryTcpThread() : TunerstudioThread("Primary TS TCP"), m_channel("TCP1", SIM_SD1_PORT, true) { }
	TsChannelBase* setupChannel() override {
		m_channel.start(0);
		return &m_channel;
	}
	void stopChannel() { m_channel.stop(); }
private:
	TcpTsChannel m_channel;
};

class SecondaryTcpThread : public TunerstudioThread {
public:
	SecondaryTcpThread() : TunerstudioThread("Secondary TS TCP"), m_channel("TCP2", SIM_SD2_PORT, false) { }
	TsChannelBase* setupChannel() override {
		m_channel.start(0);
		return &m_channel;
	}
	void stopChannel() { m_channel.stop(); }
private:
	TcpTsChannel m_channel;
};

static PrimaryTcpThread primaryTcpThread;
static SecondaryTcpThread secondaryTcpThread;

void startSerialChannels() {
	primaryTcpThread.start();
	secondaryTcpThread.start();
}

void stopSerialChannels() {
	primaryTcpThread.stopChannel();
	secondaryTcpThread.stopChannel();
}

#endif // EFI_SIMULATOR_POSIX
