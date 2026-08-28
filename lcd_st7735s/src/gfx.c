#include <stdlib.h>
#include <string.h>
#include "gfx.h"
#include "font8x8.h"

void gfx_clear(uint16_t *buf, uint16_t color) {
    if (!buf) return;
    for (size_t i = 0; i < LCD_PIXELS; i++) {
        buf[i] = color;
    }
}

void gfx_draw_pixel(uint16_t *buf, int x, int y, uint16_t color) {
    if (!buf) return;
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
        buf[y * LCD_WIDTH + x] = color;
    }
}

void gfx_draw_fast_h_line(uint16_t *buf, int x, int y, int w, uint16_t color) {
    if (!buf || y < 0 || y >= LCD_HEIGHT || w <= 0) return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (x + w > LCD_WIDTH) {
        w = LCD_WIDTH - x;
    }
    if (w <= 0) return;

    uint16_t *row = &buf[y * LCD_WIDTH + x];
    for (int i = 0; i < w; i++) {
        row[i] = color;
    }
}

void gfx_draw_fast_v_line(uint16_t *buf, int x, int y, int h, uint16_t color) {
    if (!buf || x < 0 || x >= LCD_WIDTH || h <= 0) return;
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (y + h > LCD_HEIGHT) {
        h = LCD_HEIGHT - y;
    }
    if (h <= 0) return;

    uint16_t *p = &buf[y * LCD_WIDTH + x];
    for (int i = 0; i < h; i++) {
        *p = color;
        p += LCD_WIDTH;
    }
}

void gfx_draw_line(uint16_t *buf, int x0, int y0, int x1, int y1, uint16_t color) {
    if (y0 == y1) {
        if (x1 > x0) {
            gfx_draw_fast_h_line(buf, x0, y0, x1 - x0 + 1, color);
        } else {
            gfx_draw_fast_h_line(buf, x1, y0, x0 - x1 + 1, color);
        }
        return;
    }
    if (x0 == x1) {
        if (y1 > y0) {
            gfx_draw_fast_v_line(buf, x0, y0, y1 - y0 + 1, color);
        } else {
            gfx_draw_fast_v_line(buf, x0, y1, y0 - y1 + 1, color);
        }
        return;
    }

    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        gfx_draw_pixel(buf, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_draw_rect(uint16_t *buf, int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    gfx_draw_fast_h_line(buf, x, y, w, color);
    gfx_draw_fast_h_line(buf, x, y + h - 1, w, color);
    gfx_draw_fast_v_line(buf, x, y, h, color);
    gfx_draw_fast_v_line(buf, x + w - 1, y, h, color);
}

void gfx_fill_rect(uint16_t *buf, int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    for (int i = y; i < y + h; i++) {
        gfx_draw_fast_h_line(buf, x, i, w, color);
    }
}

void gfx_draw_circle(uint16_t *buf, int x0, int y0, int r, uint16_t color) {
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    gfx_draw_pixel(buf, x0, y0 + r, color);
    gfx_draw_pixel(buf, x0, y0 - r, color);
    gfx_draw_pixel(buf, x0 + r, y0, color);
    gfx_draw_pixel(buf, x0 - r, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        gfx_draw_pixel(buf, x0 + x, y0 + y, color);
        gfx_draw_pixel(buf, x0 - x, y0 + y, color);
        gfx_draw_pixel(buf, x0 + x, y0 - y, color);
        gfx_draw_pixel(buf, x0 - x, y0 - y, color);
        gfx_draw_pixel(buf, x0 + y, y0 + x, color);
        gfx_draw_pixel(buf, x0 - y, y0 + x, color);
        gfx_draw_pixel(buf, x0 + y, y0 - x, color);
        gfx_draw_pixel(buf, x0 - y, y0 - x, color);
    }
}

void gfx_fill_circle(uint16_t *buf, int x0, int y0, int r, uint16_t color) {
    gfx_draw_fast_v_line(buf, x0, y0 - r, 2 * r + 1, color);

    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        gfx_draw_fast_v_line(buf, x0 + x, y0 - y, 2 * y + 1, color);
        gfx_draw_fast_v_line(buf, x0 - x, y0 - y, 2 * y + 1, color);
        gfx_draw_fast_v_line(buf, x0 + y, y0 - x, 2 * x + 1, color);
        gfx_draw_fast_v_line(buf, x0 - y, y0 - x, 2 * x + 1, color);
    }
}

void gfx_draw_char(uint16_t *buf, int x, int y, char c, uint16_t color, uint16_t bg, uint8_t size) {
    if (c < 32 || c > 127) {
        c = '?';
    }
    if (size == 0) size = 1;

    uint8_t char_idx = (uint8_t)(c - 32);

    for (int r = 0; r < 8; r++) {
        uint8_t line = font8x8[char_idx][r];
        for (int col = 0; col < 8; col++) {
            bool pixel_on = (line & (0x80 >> col)) != 0;
            if (pixel_on) {
                if (size == 1) {
                    gfx_draw_pixel(buf, x + col, y + r, color);
                } else {
                    gfx_fill_rect(buf, x + col * size, y + r * size, size, size, color);
                }
            } else if (bg != GFX_COLOR_TRANSPARENT) {
                if (size == 1) {
                    gfx_draw_pixel(buf, x + col, y + r, bg);
                } else {
                    gfx_fill_rect(buf, x + col * size, y + r * size, size, size, bg);
                }
            }
        }
    }
}

void gfx_draw_string(uint16_t *buf, int x, int y, const char *str, uint16_t color, uint16_t bg, uint8_t size) {
    if (!str) return;
    if (size == 0) size = 1;
    int curr_x = x;
    int curr_y = y;

    while (*str) {
        if (*str == '\n') {
            curr_x = x;
            curr_y += 8 * size;
        } else if (*str == '\r') {
            // ignore
        } else {
            gfx_draw_char(buf, curr_x, curr_y, *str, color, bg, size);
            curr_x += 8 * size;
        }
        str++;
    }
}

void gfx_draw_string_centered(uint16_t *buf, int y, const char *str, uint16_t color, uint16_t bg, uint8_t size) {
    if (!str) return;
    if (size == 0) size = 1;
    int len = (int)strlen(str);
    int total_width = len * 8 * size;
    int x = (LCD_WIDTH - total_width) / 2;
    if (x < 0) x = 0;
    gfx_draw_string(buf, x, y, str, color, bg, size);
}
