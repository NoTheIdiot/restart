#include <dogeio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <fonts.h>
#include <limine.h>
#include <io.h>

extern volatile struct limine_framebuffer_request framebuffer_request;

static char     text_grid[TERMINAL_ROWS * TERMINAL_COLS];
static uint32_t text_color_grid[TERMINAL_ROWS * TERMINAL_COLS];
static uint32_t bg_color_grid[TERMINAL_ROWS * TERMINAL_COLS];

static uint32_t term_cols = 80;
static uint32_t term_rows = 25;
static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;
static uint32_t background_color = COLOR_BLACK;
static uint32_t text_color       = COLOR_GRAY;
static const uint8_t *current_font = vga_text;

static const uint32_t ansi_colors[16] = {
    0x00000000, 0x00AA0000, 0x0000AA00, 0x00AA5500, // 0..3: Black, Red, Green, Yellow/Brown
    0x000000AA, 0x00AA00AA, 0x0000AAAA, 0x00AAAAAA, // 4..7: Blue, Magenta, Cyan, Light Gray
    0x00555555, 0x00FF5555, 0x0055FF55, 0x00FFFF55, // 8..11: Dark Gray, Bright Red/Green/Yellow
    0x005555FF, 0x00FF55FF, 0x0055FFFF, 0x00FFFFFF  // 12..15: Bright Blue/Magenta/Cyan/White
};

static int      ansi_state = 0;
static uint32_t ansi_params[8];
static int      ansi_param_count = 0;
static uint32_t ansi_cur_val = 0;
static int      ansi_has_val = 0;

static inline uint32_t read_u32_unaligned(const uint8_t *p) {
    uint32_t v = 0;
    memcpy(&v, p, sizeof(v));
    return v;
}

#if defined(__x86_64__) || defined(__i386__)
static inline void io_outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t io_inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
#else
static inline void io_outb(uint16_t port, uint8_t val) {
    (void)port;
    (void)val;
}

static inline uint8_t io_inb(uint16_t port) {
    (void)port;
    return 0;
}
#endif

static inline struct limine_framebuffer *get_fb(void) {
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        return NULL;
    }
    return framebuffer_request.response->framebuffers[0];
}

static const uint8_t *get_glyph_pointer(char c) {
    if ((uint8_t)c >= 128) {
        c = ' ';
    }

    uint32_t magic = read_u32_unaligned(current_font);

    if ((magic & 0xFFFFU) == 0x0436U) {
        uint8_t bytes_per_glyph = current_font[3];
        return &current_font[4 + ((uint8_t)c * bytes_per_glyph)];
    } else if (magic == 0x864ab572U) {
        uint32_t header_size = read_u32_unaligned(&current_font[8]);
        uint32_t bytes_per_glyph = read_u32_unaligned(&current_font[20]);
        return &current_font[header_size + ((uint8_t)c * bytes_per_glyph)];
    }

    return &current_font[(uint8_t)c * 16];
}

static void render_glyph(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    struct limine_framebuffer *fb = get_fb();
    if (!fb || !fb->address || !current_font) return;

    uint32_t pixel_x = x * 8;
    uint32_t pixel_y = y * 16;
    uint64_t pitch32 = fb->pitch / sizeof(uint32_t);

    if (pixel_x + 8 > fb->width || pixel_y + 16 > fb->height) {
        return;
    }

    uint32_t *fb_ptr = (uint32_t *)fb->address;
    const uint8_t *glyph = get_glyph_pointer(c);

    for (uint32_t g_row = 0; g_row < 16; g_row++) {
        uint8_t bits = glyph[g_row];
        size_t row_offset = (size_t)(pixel_y + g_row) * pitch32 + pixel_x;

        for (int bit = 0; bit < 8; bit++) {
            fb_ptr[row_offset + (size_t)bit] = ((bits >> (7 - bit)) & 1U) ? fg : bg;
        }
    }
}

void clear(void) {
    struct limine_framebuffer *fb = get_fb();
    if (fb && fb->address) {
        term_cols = (uint32_t)(fb->width / 8);
        term_rows = (uint32_t)(fb->height / 16);
        if (term_cols > TERMINAL_COLS) term_cols = TERMINAL_COLS;
        if (term_rows > TERMINAL_ROWS) term_rows = TERMINAL_ROWS;

        uint32_t *fb_ptr = (uint32_t *)fb->address;
        uint64_t pitch32 = fb->pitch / sizeof(uint32_t);
        for (uint64_t y = 0; y < fb->height; y++) {
            for (uint64_t x = 0; x < fb->width; x++) {
                fb_ptr[y * pitch32 + x] = background_color;
            }
        }
    }

    for (uint32_t i = 0; i < (TERMINAL_ROWS * TERMINAL_COLS); i++) {
        text_grid[i]       = ' ';
        text_color_grid[i] = text_color;
        bg_color_grid[i]   = background_color;
    }

    cursor_x = 0;
    cursor_y = 0;
}

void set_font(const uint8_t *font) {
    if (font != NULL) {
        current_font = font;
    }
}

void putchar_at(char c, uint32_t x, uint32_t y) {
    if ((uint8_t)c >= 128) {
        c = ' ';
    }
    if (x >= term_cols || y >= term_rows) {
        return;
    }

    uint32_t idx = y * TERMINAL_COLS + x;
    text_grid[idx]       = c;
    text_color_grid[idx] = text_color;
    bg_color_grid[idx]   = background_color;

    render_glyph(c, x, y, text_color, background_color);
}

static void scroll(void) {
    for (uint32_t row = 1; row < term_rows; row++) {
        for (uint32_t col = 0; col < term_cols; col++) {
            uint32_t dst = ((row - 1) * TERMINAL_COLS) + col;
            uint32_t src = (row * TERMINAL_COLS) + col;
            text_grid[dst]       = text_grid[src];
            text_color_grid[dst] = text_color_grid[src];
            bg_color_grid[dst]   = bg_color_grid[src];
        }
    }

    uint32_t blank_row = term_rows - 1;
    for (uint32_t col = 0; col < term_cols; col++) {
        uint32_t idx = blank_row * TERMINAL_COLS + col;
        text_grid[idx]       = ' ';
        text_color_grid[idx] = text_color;
        bg_color_grid[idx]   = background_color;
    }

    for (uint32_t row = 0; row < term_rows; row++) {
        for (uint32_t col = 0; col < term_cols; col++) {
            uint32_t idx = row * TERMINAL_COLS + col;
            render_glyph(text_grid[idx], col, row, text_color_grid[idx], bg_color_grid[idx]);
        }
    }
}

static void newline(void) {
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= term_rows) {
        scroll();
        cursor_y = term_rows - 1;
    }
}

static void apply_ansi_sgr(void) {
    if (ansi_param_count == 0) {
        text_color = COLOR_GRAY;
        background_color = COLOR_BLACK;
        return;
    }
    for (int i = 0; i < ansi_param_count; i++) {
        uint32_t code = ansi_params[i];
        if (code == 0) {
            text_color = COLOR_GRAY;
            background_color = COLOR_BLACK;
        } else if (code >= 30 && code <= 37) {
            text_color = ansi_colors[code - 30];
        } else if (code == 39) {
            text_color = COLOR_GRAY;
        } else if (code >= 40 && code <= 47) {
            background_color = ansi_colors[code - 40];
        } else if (code == 49) {
            background_color = COLOR_BLACK;
        } else if (code >= 90 && code <= 97) {
            text_color = ansi_colors[8 + (code - 90)];
        } else if (code >= 100 && code <= 107) {
            background_color = ansi_colors[8 + (code - 100)];
        }
    }
}

void putchar(char c) {
    if (ansi_state == 1) {
        if (c == '[') {
            ansi_state = 2;
            ansi_param_count = 0;
            ansi_cur_val = 0;
            ansi_has_val = 0;
        } else {
            ansi_state = 0;
        }
        return;
    }

    if (ansi_state == 2) {
        if (c >= '0' && c <= '9') {
            ansi_cur_val = ansi_cur_val * 10 + (uint32_t)(c - '0');
            ansi_has_val = 1;
            return;
        }
        if (c == ';') {
            if (ansi_param_count < 8) {
                ansi_params[ansi_param_count++] = ansi_has_val ? ansi_cur_val : 0;
            }
            ansi_cur_val = 0;
            ansi_has_val = 0;
            return;
        }
        if (c >= 0x40 && c <= 0x7E) {
            if (ansi_has_val && ansi_param_count < 8) {
                ansi_params[ansi_param_count++] = ansi_cur_val;
            }
            if (c == 'm') {
                apply_ansi_sgr();
            } else if (c == 'J' && ansi_param_count > 0 && ansi_params[0] == 2) {
                clear();
            }
            ansi_state = 0;
            return;
        }
        ansi_state = 0;
        return;
    }

    if (c == '\033') {
        ansi_state = 1;
        return;
    }

    if (c == '\r') {
        cursor_x = 0;
        return;
    }
    if (c == '\n') {
        newline();
        return;
    }
    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            putchar_at(' ', cursor_x, cursor_y);
        }
        return;
    }
    if (c == '\t') {
        uint32_t next = (cursor_x + 4) & ~3U;
        if (next >= term_cols) newline();
        else cursor_x = next;
        return;
    }

    if (cursor_x >= term_cols) {
        newline();
    }

    putchar_at(c, cursor_x, cursor_y);
    cursor_x++;
    if (cursor_x >= term_cols) {
        newline();
    }
}

static void print_str(const char *s) {
    if (!s) s = "(null)";
    while (*s) {
        putchar(*s++);
    }
}

static void print_uint_padded(uint64_t val, int base, int upper, int width, char pad_char) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[65];
    int i = 0;

    if (val == 0) {
        buf[i++] = '0';
    } else {
        while (val > 0) {
            buf[i++] = digits[val % (uint64_t)base];
            val /= (uint64_t)base;
        }
    }

    while (i < width && i < 64) {
        buf[i++] = pad_char;
    }

    while (i > 0) {
        putchar(buf[--i]);
    }
}

static void print_int_padded(int64_t val, int width, char pad_char) {
    if (val < 0) {
        putchar('-');
        if (width > 0) width--;
        print_uint_padded((uint64_t)(-(val + 1)) + 1, 10, 0, width, pad_char);
    } else {
        print_uint_padded((uint64_t)val, 10, 0, width, pad_char);
    }
}

void vprintf(const char *fmt, va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            putchar(*fmt);
            continue;
        }

        fmt++;
        if (!*fmt) break;

        char pad_char = ' ';
        if (*fmt == '0') {
            pad_char = '0';
            fmt++;
        }

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
            if (*fmt == 'l') fmt++;
        }

        switch (*fmt) {
            case 'c':
                putchar((char)va_arg(ap, int));
                break;
            case 's':
                print_str(va_arg(ap, const char *));
                break;
            case 'd':
            case 'i':
                print_int_padded(is_long ? va_arg(ap, int64_t) : (int64_t)va_arg(ap, int),
                                 width, pad_char);
                break;
            case 'u':
                print_uint_padded(is_long ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int),
                                  10, 0, width, pad_char);
                break;
            case 'x':
                print_uint_padded(is_long ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int),
                                  16, 0, width, pad_char);
                break;
            case 'X':
                print_uint_padded(is_long ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int),
                                  16, 1, width, pad_char);
                break;
            case 'p':
                print_str("0x");
                print_uint_padded((uintptr_t)va_arg(ap, void *), 16, 0, width, pad_char);
                break;
            case '%':
                putchar('%');
                break;
            default:
                putchar('%');
                putchar(*fmt);
                break;
        }
    }
}

void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

#define COM1_PORT 0x3F8

void init_serial(void) {
    io_outb(COM1_PORT + 1, 0x00);
    io_outb(COM1_PORT + 3, 0x80);
    io_outb(COM1_PORT + 0, 0x03);
    io_outb(COM1_PORT + 1, 0x00);
    io_outb(COM1_PORT + 3, 0x03);
    io_outb(COM1_PORT + 2, 0xC7);
    io_outb(COM1_PORT + 4, 0x0B);
}

int sis_transmit_empty(void) {
    return io_inb(COM1_PORT + 5) & 0x20;
}

static void serial_putc(char c) {
    while (!sis_transmit_empty()) {}
    io_outb(COM1_PORT, (uint8_t)c);
}

static void serial_out(char c) {
    if (c == '\n') serial_putc('\r');
    serial_putc(c);
}

void sprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            serial_out(*fmt);
            continue;
        }
        fmt++;
        if (!*fmt) break;
        switch (*fmt) {
            case 'c':
                serial_out((char)va_arg(ap, int));
                break;
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s) serial_out(*s++);
                break;
            }
            case '%':
                serial_out('%');
                break;
            default:
                serial_out('%');
                serial_out(*fmt);
                break;
        }
    }
    va_end(ap);
}