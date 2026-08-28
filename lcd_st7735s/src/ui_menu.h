#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdint.h>
#include <stdbool.h>

#define MENU_ITEM_COUNT     10
#define MENU_VISIBLE_COUNT  6

typedef enum {
    UI_STATE_MAIN_MENU = 0,
    UI_STATE_LCD_TEST,
    UI_STATE_DISPLAY_ID,
    UI_STATE_DISPLAY_TIME,
    UI_STATE_DISPLAY_COLOR,
    UI_STATE_DISPLAY_PATTERN,
    UI_STATE_DISPLAY_PROGRESS,
    UI_STATE_DISPLAY_TEXT,
    UI_STATE_DISPLAY_MENU,
    UI_STATE_DISPLAY_BUTTON,
    UI_STATE_EXIT
} ui_state_t;

/**
 * @brief UI 시스템 초기화
 */
void ui_init(void);

/**
 * @brief 버튼 입력 처리 및 UI 상태 업데이트
 */
void ui_update(void);

/**
 * @brief 프레임 버퍼에 현재 UI 화면 렌더링
 * @param buf 렌더링 대상 128x128 프레임 버퍼
 */
void ui_render(uint16_t *buf);

#endif // UI_MENU_H
