#pragma once

#include <stdarg.h>
#include <stdio.h>

#include "hal.h"

static inline int chprintf(BaseSequentialStream* stream, const char* fmt, ...) {
	(void)stream;
	va_list ap;
	va_start(ap, fmt);
	int ret = vprintf(fmt, ap);
	va_end(ap);
	return ret;
}

static inline int chsnprintf(char* buf, size_t size, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return ret;
}

static inline int chvsnprintf(char* buf, size_t size, const char* fmt, va_list ap) {
	return vsnprintf(buf, size, fmt, ap);
}
