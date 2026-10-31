#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct memory_stream_t {
	int dummy;
} memory_stream_t;

typedef struct MemoryStream {
	uint8_t* buffer;
	size_t size;
	size_t position;
	size_t eos;
} MemoryStream;

static inline void msObjectInit(MemoryStream* ms, uint8_t* buf, size_t size, uint32_t) {
	if (!ms) {
		return;
	}
	ms->buffer = buf;
	ms->size = size;
	ms->position = 0;
	ms->eos = 0;
}
