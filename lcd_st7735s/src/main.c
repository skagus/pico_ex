#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "st7735s.h"

// ---------------------------------------------------------------------------
// 그래픽 함수 (16비트 네이티브 컬러 직접 렌더링)
// ---------------------------------------------------------------------------

static inline void draw_pixel(uint16_t *buf, int x, int y, uint16_t color) {
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
        buf[y * LCD_WIDTH + x] = color;
    }
}

static void draw_line(uint16_t *buf, int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        draw_pixel(buf, x0, y0, color);
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

static inline uint16_t random_color(void) {
    uint8_t r = rand() % 32;
    uint8_t g = rand() % 64;
    uint8_t b = rand() % 32;
    if (r == 0 && g == 0 && b == 0) r = 31;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

// ---------------------------------------------------------------------------
// Main (16비트 SPI + 더블 버퍼링 파이프라인)
// ---------------------------------------------------------------------------
int main(void) {
    stdio_init_all();

    // LCD 초기화
    lcd_init();

    printf("\n=======================================================\n");
    printf(" ST7735S High-Performance Pipeline Benchmark\n");
    printf(" Mode: Double Buffering + 16-bit SPI DMA\n");
    printf(" Resolution: %dx%d, Requested SPI: %d MHz\n", LCD_WIDTH, LCD_HEIGHT, LCD_SPI_FREQ / 1000000);
    printf("=======================================================\n\n");

    uint32_t frame_count = 0;
    uint64_t total_render_time_us = 0;
    uint32_t fps_start_time_us = time_us_32();
    uint32_t last_frame_time_us = time_us_32();

    const int NUM_LINES = 15;  // 프레임당 무작위 선 개수
    uint8_t draw_idx = 0;      // 렌더링 대상 버퍼 인덱스 (0 or 1)

    while (true) {
        uint16_t *draw_buf = g_frame_buffers[0];//draw_idx];

        // 1. [CPU 작업] 현재 백버퍼에 화면 렌더링 (이전 프레임의 DMA 전송과 완전 병렬 실행!)
        uint32_t render_start_us = time_us_32();

        //memset(draw_buf, 0, sizeof(uint16_t) * LCD_PIXELS);
        for (int i = 0; i < NUM_LINES; i++) {
            int x0 = rand() % LCD_WIDTH;
            int y0 = rand() % LCD_HEIGHT;
            int x1 = rand() % LCD_WIDTH;
            int y1 = rand() % LCD_HEIGHT;
            draw_line(draw_buf, x0, y0, x1, y1, random_color());
        }

        uint32_t render_end_us = time_us_32();
        total_render_time_us += (render_end_us - render_start_us);

        // 2. [동기화] 이전 프레임의 DMA 전송 완료 대기
        lcd_wait_idle();

        // 3. [DMA 작업] 방금 렌더링 완료된 버퍼를 16비트 SPI DMA로 백그라운드 전송 시작 (Fire & Forget)
        lcd_draw_frame_buffer(draw_buf);

        // 4. 버퍼 교체 (더블 버퍼링 스왑)
        draw_idx = 1 - draw_idx;

        // 프레임 주기(Frame Period) 계산
        uint32_t now_us = time_us_32();
        uint32_t frame_period_us = now_us - last_frame_time_us;
        last_frame_time_us = now_us;
        frame_count++;

        // 100프레임마다 통계 출력
        if (frame_count % 100 == 0) {
            uint32_t elapsed_us = now_us - fps_start_time_us;
            float fps = (float)frame_count * 1000000.0f / (float)elapsed_us;
            float avg_render_ms = (float)total_render_time_us / (float)frame_count / 1000.0f;
            float avg_period_ms = (float)elapsed_us / (float)frame_count / 1000.0f;

            printf("[Frame %5u] FPS: %6.2f | Period: %5.2f ms | CPU Render: %5.2f ms (Pipelined)\n",
                   frame_count, fps, avg_period_ms, avg_render_ms);

            frame_count = 0;
            total_render_time_us = 0;
            fps_start_time_us = time_us_32();
        }
    }

    return 0;
}
