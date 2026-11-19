#ifndef DOGEIO_H
#define DOGEIO_H

#include <stdint.h>
#include <bool.h>
#include <stddef.h>

#define TERMINAL_COLS 160
#define TERMINAL_ROWS 67

#define COLOR_BACKGROUND            0x0C0C0C
#define COLOR_DARK_GRAY             0x404040
#define COLOR_GRAY                  0xCCCCCC
#define COLOR_BRIGHT_GRAY           0xE0E0E0
#define COLOR_CURSOR                0xFFFFFF

#define COLOR_DARK_BLACK            0x000000
#define COLOR_DARK_BLUE             0x001F82
#define COLOR_DARK_GREEN            0x0B5E08
#define COLOR_DARK_CYAN             0x21547A
#define COLOR_DARK_RED              0x6F0912
#define COLOR_DARK_PURPLE           0x4D0D56
#define COLOR_DARK_MAGENTA          0x4D0D56
#define COLOR_DARK_ORANGE           0x803700
#define COLOR_DARK_YELLOW           0x6E5900
#define COLOR_DARK_WHITE            0x999999

#define COLOR_BLACK                 0x0C0C0C
#define COLOR_BLUE                  0x0037DA
#define COLOR_GREEN                 0x13A10E
#define COLOR_CYAN                  0x3A96DD
#define COLOR_RED                   0xC50F1F
#define COLOR_PURPLE                0x881798
#define COLOR_MAGENTA               0x881798
#define COLOR_ORANGE                0xD85D00
#define COLOR_YELLOW                0xC19C00
#define COLOR_WHITE                 0xF0F0F0

#define COLOR_BRIGHT_BLACK          0x767676
#define COLOR_BRIGHT_BLUE           0x3B78FF
#define COLOR_BRIGHT_GREEN          0x16C60C
#define COLOR_BRIGHT_CYAN           0x61D6D6
#define COLOR_BRIGHT_RED            0xE74856
#define COLOR_BRIGHT_PURPLE         0xB4009E
#define COLOR_BRIGHT_MAGENTA        0xB4009E
#define COLOR_BRIGHT_ORANGE         0xFF8C3A
#define COLOR_BRIGHT_YELLOW         0xF9F1A5
#define COLOR_BRIGHT_WHITE          0xFFFFFF

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

void clear(void);
void set_font(const uint8_t *font);
void putchar_at(char c, uint32_t x, uint32_t y);
void putchar(char c);
void vprintf(const char *fmt, va_list ap);
void printf(const char *fmt, ...);

// gdt stuff, or whatever. I do not want the 50 different header mess again
// or some garbage like that.
void init_gdt_tss(void);
void init_idt(void);
void init_fpu(void);
void init_apic(void);

#endif
