#pragma once

#include "ch.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace chibios_rt {

class Mutex {
public:
	void lock() { m.lock(); }
	void unlock() { m.unlock(); }

private:
	std::mutex m;
};

class MutexLocker {
public:
	explicit MutexLocker(Mutex& m) : m_mutex(m) { m_mutex.lock(); }
	~MutexLocker() { m_mutex.unlock(); }

private:
	Mutex& m_mutex;
};

class CriticalSectionLocker {
public:
	CriticalSectionLocker() { chSysLock(); }
	~CriticalSectionLocker() { chSysUnlock(); }
};

template<typename T, size_t N>
class Mailbox {
public:
	msg_t post(T msg, sysinterval_t timeout) {
		std::unique_lock<std::mutex> lk(m_mutex);
		if (timeout == TIME_IMMEDIATE) {
			if (m_queue.size() >= N) {
				return MSG_TIMEOUT;
			}
		} else if (timeout != TIME_INFINITE) {
			auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(
				(static_cast<uint64_t>(timeout) * 1000ULL) / CH_CFG_ST_FREQUENCY
			);
			if (!m_cv.wait_until(lk, deadline, [&] { return m_queue.size() < N; })) {
				return MSG_TIMEOUT;
			}
		} else {
			m_cv.wait(lk, [&] { return m_queue.size() < N; });
		}

		m_queue.push(msg);
		lk.unlock();
		m_cv.notify_all();
		return MSG_OK;
	}

	msg_t postI(T msg) {
		std::lock_guard<std::mutex> lk(m_mutex);
		if (m_queue.size() >= N) {
			return MSG_TIMEOUT;
		}
		m_queue.push(msg);
		m_cv.notify_all();
		return MSG_OK;
	}

	msg_t fetch(T* out, sysinterval_t timeout) {
		std::unique_lock<std::mutex> lk(m_mutex);
		if (timeout == TIME_IMMEDIATE) {
			if (m_queue.empty()) {
				return MSG_TIMEOUT;
			}
		} else if (timeout != TIME_INFINITE) {
			auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(
				(static_cast<uint64_t>(timeout) * 1000ULL) / CH_CFG_ST_FREQUENCY
			);
			if (!m_cv.wait_until(lk, deadline, [&] { return !m_queue.empty(); })) {
				return MSG_TIMEOUT;
			}
		} else {
			m_cv.wait(lk, [&] { return !m_queue.empty(); });
		}

		*out = m_queue.front();
		m_queue.pop();
		lk.unlock();
		m_cv.notify_all();
		return MSG_OK;
	}

	msg_t fetchI(T* out) {
		std::lock_guard<std::mutex> lk(m_mutex);
		if (m_queue.empty()) {
			return MSG_TIMEOUT;
		}
		*out = m_queue.front();
		m_queue.pop();
		m_cv.notify_all();
		return MSG_OK;
	}

	// ChibiOS mailbox reset: drop queued messages (waiters see MSG_RESET there; the
	// tooth logger only uses it to drain/re-arm the pool).
	void resetI() {
		std::lock_guard<std::mutex> lk(m_mutex);
		std::queue<T>().swap(m_queue);
		m_cv.notify_all();
	}
	void reset() { resetI(); }
	void resumeX() { }

	size_t getUsedCountI() const {
		std::lock_guard<std::mutex> lk(m_mutex);
		return m_queue.size();
	}

private:
	mutable std::mutex m_mutex;
	std::condition_variable m_cv;
	std::queue<T> m_queue;
};

class BinarySemaphore {
public:
	explicit BinarySemaphore(bool taken = false) : m_taken(taken) { }

	void signal() {
		std::lock_guard<std::mutex> lk(m_mutex);
		m_taken = false;
		m_cv.notify_all();
	}

	void signalI() {
		signal();
	}

	void wait() {
		std::unique_lock<std::mutex> lk(m_mutex);
		m_cv.wait(lk, [&] { return !m_taken; });
		m_taken = true;
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_taken;
};

class CounterSemaphore {
public:
	explicit CounterSemaphore(int count = 0) : m_count(count) { }

	void signal() {
		std::lock_guard<std::mutex> lk(m_mutex);
		m_count++;
		m_cv.notify_all();
	}

	void signalI() {
		signal();
	}

	void wait() {
		std::unique_lock<std::mutex> lk(m_mutex);
		m_cv.wait(lk, [&] { return m_count > 0; });
		m_count--;
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	int m_count;
};

class ThreadReference {
public:
	ThreadReference() : m_thread(nullptr) { }
	explicit ThreadReference(thread_t* t) : m_thread(t) { }

	void requestTerminate() { chThdRequestTerminate(m_thread); }
	void wait() { chThdWait(m_thread); }

private:
	thread_t* m_thread;
};

template<int TStackSize>
class BaseStaticThread {
public:
	ThreadReference start(tprio_t /*prio*/) {
		struct Ctx {
			BaseStaticThread* self;
		};
		auto* ctx = new Ctx{this};
		auto thread = chThdCreateFromHeap(nullptr, TStackSize, "posix", 0, [](void* arg) {
			auto* ctx_ptr = static_cast<Ctx*>(arg);
			ctx_ptr->self->main();
			delete ctx_ptr;
		}, ctx);
		return ThreadReference(thread);
	}
	void setName(const char*) { }

protected:
	virtual void main() = 0;
};

} // namespace chibios_rt
