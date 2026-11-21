#ifndef STDIO_H
#define STDIO_H

#include "stdint.h"
#include "bool.h"
#include "root.h"

static inline uint64_t printf(const char *fmt, ...) {
    char* buf;
    va_list args;
    va_start(args, fmt);
    int len = _root_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len < 0) return ERR_INVAL;
    return syscall_2(TEXT, TEXT_PRINTF, (uint64_t)buf);
}

#endif