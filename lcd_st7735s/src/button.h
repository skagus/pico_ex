#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>

// 버튼 핀 정의 (project.txt 기준)
#define BTN_PIN_UP      26
#define BTN_PIN_DOWN    27
#define BTN_PIN_CANCEL  28
#define BTN_PIN_OK      29

typedef enum {
    BTN_UP = 0,
    BTN_DOWN,
    BTN_CANCEL,
    BTN_OK,
    BTN_COUNT
} button_id_t;

/**
 * @brief 버튼 GPIO 및 풀업 저항 초기화
 */
void button_init(void);

/**
 * @brief 주기적으로 호출하여 버튼 상태 갱신 및 디바운스 처리 (메인 루프에서 호출)
 */
void button_update(void);

/**
 * @brief 특정 버튼이 한 번 클릭(Press 후 Release 또는 엣지 감지)되었는지 확인하고 클리어
 * @param btn 버튼 ID
 * @return true 클릭됨, false 클릭 안 됨
 */
bool button_was_pressed(button_id_t btn);

/**
 * @brief 현재 버튼이 물리적으로 눌려있는지 실시간 확인 (Display Button 테스트용)
 * @param btn 버튼 ID
 * @return true 현재 눌림 상태, false 뗌 상태
 */
bool button_is_down(button_id_t btn);

#endif // BUTTON_H
