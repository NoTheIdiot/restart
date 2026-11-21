#ifndef STDIO_H
#define STDIO_H

#include "stdint.h"
#include "bool.h"
#include "root.h"
#include "stddef.h"

int printf(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int len = _root_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len < 0) return -1;
    size_t ulen = (size_t)len;
    if (ulen >= sizeof(buf)) ulen = sizeof(buf) - 1;

    return (int)syscall_3(TEXT, TEXT_PRINTF, (uint64_t)buf, (uint64_t)ulen);
}

#endif