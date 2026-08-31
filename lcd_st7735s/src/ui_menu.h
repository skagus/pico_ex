#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdint.h>
#include <stdbool.h>

#define MENU_ITEM_COUNT     10
#define MENU_VISIBLE_COUNT  6

#define MAX_LIST_ITEM   (16)

typedef enum _widget_action {
    WG_NONE, // 아무 것도 하지 않음. 현재 화면 유지.
    WG_INTO, // 하위 화면으로 진입.
    WG_EXIT, // 이 화면 밖으로 (상위 복귀).
} widget_act;

typedef widget_act (*list_item_func)(uint16_t* fb, uint8_t* ctx, bool first_call);

// 메뉴 위젯의 내부 context (ctx_buf 위에 스택됨)
typedef struct _menu_ctx {
    bool b_active;      // true: 메뉴 리스트 표시 중, false: 자식 위젯 활성
    int selected;       // 현재 선택된 항목 인덱스
    int scroll_top;     // 스크롤 시작 인덱스
} menu_ctx;

typedef struct _list_widget {
    int num_items;
    char* title;
    char* item[MAX_LIST_ITEM];
    list_item_func item_func[MAX_LIST_ITEM];
} list_widget;

/**
 * @brief UI 시스템 초기화
 */
void ui_init(void);

/**
 * @brief 메인 UI 루프 (매 프레임 호출)
 * @param buf 렌더링 대상 128x128 프레임 버퍼
 */
void ui_main(uint16_t *buf);

#endif // UI_MENU_H
