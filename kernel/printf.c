#include <dogeio.h>
#include <stdint.h>
#include <stddef.h>
#include <fonts.h>
#include <limine.h>

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

    uint32_t magic = *(const uint32_t *)current_font;

    if ((magic & 0xFFFF) == 0x0436) {
        uint8_t bytes_per_glyph = current_font[3];
        return &current_font[4 + ((uint8_t)c * bytes_per_glyph)];
    } else if (magic == 0x864ab572) {
        uint32_t header_size = *(const uint32_t *)&current_font[8];
        uint32_t bytes_per_glyph = *(const uint32_t *)&current_font[20];
        return &current_font[header_size + ((uint8_t)c * bytes_per_glyph)];
    }

    return &current_font[(uint8_t)c * 16];
}

static void render_glyph(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    struct limine_framebuffer *fb = get_fb();
    if (!fb || !current_font) return;

    uint32_t *fb_ptr = (uint32_t *)fb->address;
    uint32_t pixel_x = x * 8;
    uint32_t pixel_y = y * 16;
    uint64_t pitch32 = fb->pitch / 4;

    if (pixel_x + 8 > fb->width || pixel_y + 16 > fb->height) {
        return;
    }

    const uint8_t *glyph = get_glyph_pointer(c);

    for (uint32_t g_row = 0; g_row < 16; g_row++) {
        uint8_t bits = glyph[g_row];
        size_t row_offset = (pixel_y + g_row) * pitch32 + pixel_x;

        for (int bit = 0; bit < 8; bit++) {
            fb_ptr[row_offset + (size_t)bit] = ((bits >> (7 - bit)) & 1) ? fg : bg;
        }
    }
}

void clear(void) {
    struct limine_framebuffer *fb = get_fb();
    if (fb) {
        term_cols = (uint32_t)(fb->width / 8);
        term_rows = (uint32_t)(fb->height / 16);
        if (term_cols > TERMINAL_COLS) term_cols = TERMINAL_COLS;
        if (term_rows > TERMINAL_ROWS) term_rows = TERMINAL_ROWS;

        uint32_t *fb_ptr = (uint32_t *)fb->address;
        uint64_t pitch32 = fb->pitch / 4;
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

void putchar(char c) {
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

static void print_uint(uint64_t val, int base, int upper) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[65];
    int i = 0;

    if (val == 0) {
        putchar('0');
        return;
    }

    while (val > 0) {
        buf[i++] = digits[val % (uint64_t)base];
        val /= (uint64_t)base;
    }

    while (i > 0) {
        putchar(buf[--i]);
    }
}

static void print_int(int64_t val) {
    if (val < 0) {
        putchar('-');
        print_uint((uint64_t)(-(val + 1)) + 1, 10, 0);
    } else {
        print_uint((uint64_t)val, 10, 0);
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
                print_int(is_long ? va_arg(ap, int64_t) : (int64_t)va_arg(ap, int));
                break;
            case 'u':
                print_uint(is_long ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int), 10, 0);
                break;
            case 'x':
                print_uint(is_long ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int), 16, 0);
                break;
            case 'X':
                print_uint(is_long ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int), 16, 1);
                break;
            case 'p':
                print_str("0x");
                print_uint((uintptr_t)va_arg(ap, void *), 16, 0);
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
