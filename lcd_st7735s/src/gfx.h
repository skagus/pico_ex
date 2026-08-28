#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "st7735s.h"

// 투명 배경을 위한 특수 색상 정의 (bg로 이 값을 주면 배경을 칠하지 않음)
#define GFX_COLOR_TRANSPARENT 0x0001

void gfx_clear(uint16_t *buf, uint16_t color);
void gfx_draw_pixel(uint16_t *buf, int x, int y, uint16_t color);
void gfx_draw_line(uint16_t *buf, int x0, int y0, int x1, int y1, uint16_t color);
void gfx_draw_fast_h_line(uint16_t *buf, int x, int y, int w, uint16_t color);
void gfx_draw_fast_v_line(uint16_t *buf, int x, int y, int h, uint16_t color);
void gfx_draw_rect(uint16_t *buf, int x, int y, int w, int h, uint16_t color);
void gfx_fill_rect(uint16_t *buf, int x, int y, int w, int h, uint16_t color);
void gfx_draw_circle(uint16_t *buf, int x0, int y0, int r, uint16_t color);
void gfx_fill_circle(uint16_t *buf, int x0, int y0, int r, uint16_t color);
void gfx_draw_char(uint16_t *buf, int x, int y, char c, uint16_t color, uint16_t bg, uint8_t size);
void gfx_draw_string(uint16_t *buf, int x, int y, const char *str, uint16_t color, uint16_t bg, uint8_t size);
void gfx_draw_string_centered(uint16_t *buf, int y, const char *str, uint16_t color, uint16_t bg, uint8_t size);

#endif // GFX_H
