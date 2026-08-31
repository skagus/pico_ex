#include <stdio.h>
#include "pico/stdlib.h"
#include <pico/time.h>
#include <hardware/gpio.h>
#include "st7735s.h"
#include "button.h"
#include "ui_menu.h"
#include "gfx.h"

#define ABS_USC(x)      (x)
#define ABS_MSEC(x)     ((uint64_t)(x) * 1000)
#define ABS_SEC(x)      ABS_MSEC((x)*1000)

#define LED_PIN         (25)

int main(void) {

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // 1. 기본 입출력 및 주변장치 초기화
    stdio_init_all();

    // 2. 버튼 GPIO 초기화 (GP26: UP, GP27: DOWN, GP28: CANCEL, GP29: OK)
    button_init();

    // 3. UI 상태 관리자 초기화
    ui_init();

    // 1. 화면 초기화
    lcd_init();

    printf("\n=======================================================\n");
    printf(" ST7735S 1.44\" LCD UI System Initialized\n");
    printf(" Boot Sequence Starting...\n");
    printf("=======================================================\n");

    // 2. 화면 클리어 (Black)
    lcd_fill_color(g_frame_buffers[0], COLOR_BLACK);
    lcd_draw_frame_buffer(g_frame_buffers[0]);
    sleep_ms(200);

    printf("Boot Step 4: Entering Main Menu\n");

    // -----------------------------------------------------------------------
    // 메인 루프 (더블 버퍼링 기반 실시간 UI 렌더링 & DMA 전송)
    // -----------------------------------------------------------------------
    uint8_t draw_idx = 0;

    absolute_time_t ui_prev = get_absolute_time();
    absolute_time_t my_prev = get_absolute_time();
    absolute_time_t curr;

    while (true) {
        curr = get_absolute_time();
        uint64_t ui_diff = absolute_time_diff_us(ui_prev, curr);
        if(ui_diff >= ABS_MSEC(30) && !lcd_is_busy())
        {
            ui_prev = curr;

            // 1. 버튼 상태 갱신 및 디바운싱
            // button_update();

            // 2. UI 상태 및 네비게이션 로직 처리
            //ui_update();

            // 3. 백버퍼에 현재 UI 화면 렌더링
            uint16_t *draw_buf = g_frame_buffers[0]; // draw_idx];
            ui_main(draw_buf);

            // 4. 렌더링된 버퍼를 16비트 SPI DMA로 전송 시작 (Fire & Forget)
            lcd_draw_frame_buffer(draw_buf);

            // 5. 프레임 버퍼 인덱스 스왑
#if LCD_NUM_BUFFERS > 1
            draw_idx = (draw_idx + 1) % LCD_NUM_BUFFERS;
#endif
        }
        
        curr = get_absolute_time();
        uint64_t my_diff = absolute_time_diff_us(my_prev, curr);
        if( my_diff > ABS_SEC(1))  // 1 sec.
        {
            printf("Tick\n");
            gpio_put(LED_PIN, !gpio_get(LED_PIN));
            my_prev = curr;
        }
    }

    return 0;
}
