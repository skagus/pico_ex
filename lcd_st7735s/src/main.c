#include <stdio.h>
#include "pico/stdlib.h"
#include "st7735s.h"
#include "button.h"
#include "ui_menu.h"
#include "gfx.h"

int main(void) {
    // 1. 기본 입출력 및 주변장치 초기화
    stdio_init_all();

    // 2. 버튼 GPIO 초기화 (GP26: UP, GP27: DOWN, GP28: CANCEL, GP29: OK)
    button_init();

    // 3. UI 상태 관리자 초기화
    ui_init();

    // -----------------------------------------------------------------------
    // [project.txt 부팅 순서]
    // 1. 전원 ON 시 화면 초기화
    // 2. 화면 클리어
    // 3. R, G, B 색깔을 1 sec 간격으로 화면을 채움.
    // 4. 메뉴 표시.
    // -----------------------------------------------------------------------

    // 1. 화면 초기화
    lcd_init();

    printf("\n=======================================================\n");
    printf(" ST7735S 1.44\" LCD UI System Initialized\n");
    printf(" Boot Sequence Starting...\n");
    printf("=======================================================\n");

    // 2. 화면 클리어 (Black)
    lcd_fill_color(COLOR_BLACK);
    sleep_ms(200);

    // 3. R, G, B 색깔을 1초 간격으로 화면에 표시
    printf("Boot Step 3-1: Filling RED (1 sec)\n");
    lcd_fill_color(COLOR_RED);
    sleep_ms(1000);

    printf("Boot Step 3-2: Filling GREEN (1 sec)\n");
    lcd_fill_color(COLOR_GREEN);
    sleep_ms(1000);

    printf("Boot Step 3-3: Filling BLUE (1 sec)\n");
    lcd_fill_color(COLOR_BLUE);
    sleep_ms(1000);

    // 4. 메뉴 화면 진입 전 화면 클리어
    lcd_fill_color(COLOR_BLACK);

    printf("Boot Step 4: Entering Main Menu\n");

    // -----------------------------------------------------------------------
    // 메인 루프 (더블 버퍼링 기반 실시간 UI 렌더링 & DMA 전송)
    // -----------------------------------------------------------------------
    uint8_t draw_idx = 0;

    while (true) {
        // 1. 버튼 상태 갱신 및 디바운싱
        button_update();

        // 2. UI 상태 및 네비게이션 로직 처리
        ui_update();

        // 3. 백버퍼에 현재 UI 화면 렌더링
        uint16_t *draw_buf = g_frame_buffers[draw_idx];
        ui_render(draw_buf);

        // 4. 이전 프레임의 SPI DMA 전송 완료 대기
        lcd_wait_idle();

        // 5. 렌더링된 버퍼를 16비트 SPI DMA로 전송 시작 (Fire & Forget)
        lcd_draw_frame_buffer(draw_buf);

        // 6. 더블 버퍼 인덱스 스왑
        draw_idx = 1 - draw_idx;

        // 7. 부드러운 애니메이션 및 반응성을 위한 프레임 딜레이 (~30 FPS)
        sleep_ms(30);
    }

    return 0;
}
