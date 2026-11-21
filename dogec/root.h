#ifndef ROOT_H
#define ROOT_H

#include "stdint.h"
#include "bool.h"
#include <stdarg.h>

enum {
    TEXT = 0x01,
    FS   = 0x02,
    DOGE = 0x03,
    MEM  = 0x04,
    IPC  = 0x05,
    PROC  = 0x06,
};

enum {
    TEXT_PRINTF = 0x01
};

enum {
    DOGE_EXIT = 0x01
};

#define ERR_UNKNOWN_SYSCALL ((uint64_t)-1)
#define ERR_UNKNOWN_OPCODE  ((uint64_t)-2)
#define ERR_BAD_PTR         ((uint64_t)-3)
#define ERR_INVAL           ((uint64_t)-4)

static inline uint64_t syscall_0(uint64_t num) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        :
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_1(uint64_t num, uint64_t arg1) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    __asm__ volatile (
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_4(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    __asm__ volatile (
        "mov %[a4], %%r10\n\t"
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3), [a4] "r"(arg4)
        : "rcx", "r10", "r11", "memory"
    );
    return num;
}

static inline uint64_t syscall_5(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    __asm__ volatile (
        "mov %[a4], %%r10\n\t"
        "mov %[a5], %%r8\n\t"
        "syscall"
        : "+a"(num)
        : "D"(arg1), "S"(arg2), "d"(arg3), [a4] "r"(arg4), [a5] "r"(arg5)
        : "rcx", "r8", "r10", "r11", "memory"
    );
    return num;
}

static inline int _root_vsnprintf(char *buf, uint64_t size, const char *fmt, va_list args) {
    if (!buf || size == 0 || !fmt) return -1;

    uint64_t pos = 0;
    #define EMIT(c) do { if (pos + 1 < size) buf[pos] = (c); pos++; } while (0)

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            EMIT(*p);
            continue;
        }
        p++;
        if (!*p) break;

        bool is_long = false;
        if (*p == 'l') {
            is_long = true;
            p++;
            if (*p == 'l') p++;
        } else if (*p == 'z') {
            is_long = true;
            p++;
        }

        switch (*p) {
            case '%':
                EMIT('%');
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                EMIT(c);
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s) EMIT(*s++);
                break;
            }
            case 'd':
            case 'i': {
                int64_t val = is_long ? va_arg(args, int64_t) : (int64_t)va_arg(args, int);
                uint64_t uval;
                if (val < 0) {
                    EMIT('-');
                    uval = (uint64_t)(-(val + 1)) + 1;
                } else {
                    uval = (uint64_t)val;
                }
                char tmp[24];
                int i = 0;
                do { tmp[i++] = (char)('0' + (uval % 10)); uval /= 10; } while (uval);
                while (i > 0) EMIT(tmp[--i]);
                break;
            }
            case 'u': {
                uint64_t uval = is_long ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
                char tmp[24];
                int i = 0;
                do { tmp[i++] = (char)('0' + (uval % 10)); uval /= 10; } while (uval);
                while (i > 0) EMIT(tmp[--i]);
                break;
            }
            case 'x':
            case 'X':
            case 'p': {
                const char *digits = (*p == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                uint64_t uval = (*p == 'p' || is_long)
                                ? va_arg(args, uint64_t)
                                : (uint64_t)va_arg(args, unsigned int);
                if (*p == 'p') {
                    EMIT('0');
                    EMIT('x');
                }
                char tmp[24];
                int i = 0;
                do { tmp[i++] = digits[uval & 0xF]; uval >>= 4; } while (uval);
                while (i > 0) EMIT(tmp[--i]);
                break;
            }
            default:
                EMIT('%');
                EMIT(*p);
                break;
        }
    }

    #undef EMIT
    buf[(pos < size) ? pos : (size - 1)] = '\0';
    return (int)pos;
}

static inline uint64_t root_console(uint32_t opcode, ...) {
    if (opcode == TEXT_PRINTF) {
        va_list args;
        va_start(args, opcode);

        const char *fmt = va_arg(args, const char *);
        char buf[512];

        int len = _root_vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        if (len < 0) return ERR_INVAL;

        return syscall_2(TEXT, TEXT_PRINTF, (uint64_t)buf);
    }

    return ERR_UNKNOWN_OPCODE;
}

static inline uint64_t root_doge(uint32_t opcode, ...) {
    if (opcode == DOGE_EXIT) {
        syscall_1(DOGE, DOGE_EXIT);
    }
}

#endif