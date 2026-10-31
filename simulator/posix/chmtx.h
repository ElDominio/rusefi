#pragma once

// Real mutexes (std::recursive_mutex behind impl); rusEFI's CAN TX service and others rely on them.
typedef struct mutex_t {
	void* impl;
} mutex_t;

#ifdef __cplusplus
extern "C" {
#endif
void chMtxObjectInit(mutex_t* mp);
void chMtxLock(mutex_t* mp);
void chMtxUnlock(mutex_t* mp);
bool chMtxTryLock(mutex_t* mp);
#ifdef __cplusplus
}
#endif
static inline void chMtxInit(mutex_t* mp) { chMtxObjectInit(mp); }
