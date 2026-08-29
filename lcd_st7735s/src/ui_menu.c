#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/clocks.h"
#include "ui_menu.h"
#include "button.h"
#include "gfx.h"
#include "st7735s.h"

// ---------------------------------------------------------------------------
// 10개 메뉴 항목 명칭 (project.txt 기준)
// ---------------------------------------------------------------------------
static const char *menu_names[MENU_ITEM_COUNT] = {
    "LCD Test",
    "Display ID",
    "Display Time",
    "Display Color",
    "Display Pattern",
    "Display Progress",
    "Display Text",
    "Display Menu",
    "Display Button",
    "Exit"
};

// ---------------------------------------------------------------------------
// UI State & Variables
// ---------------------------------------------------------------------------
static ui_state_t current_state = UI_STATE_MAIN_MENU;
static int selected_menu = 0;      // 0 ~ 9
static int scroll_top = 0;          // 0 ~ 4 (6개 항목 표시)

// 서브메뉴용 추가 상태값들
static int sub_menu_selected = 0;
static float progress_val = 0.0f;
static float anim_angle = 0.0f;

// ---------------------------------------------------------------------------
// 시간 포맷 헬퍼 (부팅 후 경과 시간 기반 시:분:초)
// ---------------------------------------------------------------------------
static void get_formatted_time(char *out_str, size_t max_len) {
    uint32_t total_sec = to_ms_since_boot(get_absolute_time()) / 1000;
    uint32_t hours = (total_sec / 3600) % 24;
    uint32_t minutes = (total_sec / 60) % 60;
    uint32_t seconds = total_sec % 60;
    snprintf(out_str, max_len, "%02lu:%02lu:%02lu", (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
}

// ---------------------------------------------------------------------------
// 공통 UI 요소 렌더러
// ---------------------------------------------------------------------------
static void render_header(uint16_t *buf, const char *title) {
    // Title Bar 배경 (어두운 파랑)
    gfx_fill_rect(buf, 0, 0, LCD_WIDTH, 14, COLOR_NAVY);
    gfx_draw_fast_h_line(buf, 0, 14, LCD_WIDTH, COLOR_CYAN);

    // Title 텍스트
    gfx_draw_string(buf, 3, 3, title, COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);

    // 시각 텍스트 (우측 정렬)
    char time_str[16];
    get_formatted_time(time_str, sizeof(time_str));
    gfx_draw_string(buf, LCD_WIDTH - (8 * 8) - 2, 3, time_str, COLOR_YELLOW, GFX_COLOR_TRANSPARENT, 1);
}

static void render_footer(uint16_t *buf, const char *left_hint, const char *right_hint) {
    // Status Bar 배경 (짙은 회색)
    gfx_fill_rect(buf, 0, 114, LCD_WIDTH, 14, COLOR_DARKGRAY);
    gfx_draw_fast_h_line(buf, 0, 113, LCD_WIDTH, COLOR_LIGHTGRAY);

    if (left_hint) {
        gfx_draw_string(buf, 3, 117, left_hint, COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);
    }
    if (right_hint) {
        int len = (int)strlen(right_hint);
        int x = LCD_WIDTH - (len * 8) - 2;
        gfx_draw_string(buf, x, 117, right_hint, COLOR_CYAN, GFX_COLOR_TRANSPARENT, 1);
    }
}

// ---------------------------------------------------------------------------
// 1. 메인 메뉴 화면 렌더링
// ---------------------------------------------------------------------------
static void render_main_menu(uint16_t *buf) {
    // 1. Title Bar
    render_header(buf, "ST7735S");

    // 2. Body: 6개 메뉴 항목 표시 (Y: 16 ~ 111, 각 행 높이 16px)
    const int item_height = 16;
    const int start_y = 16;

    for (int i = 0; i < MENU_VISIBLE_COUNT; i++) {
        int item_idx = scroll_top + i;
        if (item_idx >= MENU_ITEM_COUNT) break;

        int y = start_y + (i * item_height);
        bool is_selected = (item_idx == selected_menu);

        if (is_selected) {
            // 현재 선택된 메뉴는 역상(Inverted) 하이라이트 표시
            gfx_fill_rect(buf, 2, y + 1, LCD_WIDTH - 10, item_height - 2, COLOR_WHITE);
            
            // 번호와 메뉴명 출력 (검은색 폰트)
            char text[32];
            snprintf(text, sizeof(text), ">%d.%-11s", item_idx + 1, menu_names[item_idx]);
            gfx_draw_string(buf, 4, y + 4, text, COLOR_BLACK, GFX_COLOR_TRANSPARENT, 1);
        } else {
            // 미선택 메뉴는 일반 텍스트
            gfx_fill_rect(buf, 2, y + 1, LCD_WIDTH - 10, item_height - 2, COLOR_BLACK);
            char text[32];
            snprintf(text, sizeof(text), " %d.%-11s", item_idx + 1, menu_names[item_idx]);
            gfx_draw_string(buf, 4, y + 4, text, COLOR_LIGHTGRAY, GFX_COLOR_TRANSPARENT, 1);
        }
    }

    // 3. 우측 스크롤 바 인디케이터 (Y: 16 ~ 111)
    int track_x = LCD_WIDTH - 6;
    int track_y = 16;
    int track_h = 96;
    gfx_fill_rect(buf, track_x, track_y, 4, track_h, COLOR_DARKGRAY);

    // 스크롤 썸(Thumb) 계산
    int thumb_h = (track_h * MENU_VISIBLE_COUNT) / MENU_ITEM_COUNT; // 57px
    int max_scroll = MENU_ITEM_COUNT - MENU_VISIBLE_COUNT;           // 4
    int thumb_y = track_y;
    if (max_scroll > 0) {
        thumb_y += (scroll_top * (track_h - thumb_h)) / max_scroll;
    }
    gfx_fill_rect(buf, track_x, thumb_y, 4, thumb_h, COLOR_CYAN);

    // 4. Status Bar (하단 인디케이터 및 버튼 가이드)
    char status_str[32];
    snprintf(status_str, sizeof(status_str), "[%02d/%02d]", selected_menu + 1, MENU_ITEM_COUNT);
    render_footer(buf, status_str, "OK:Enter");
}

// ---------------------------------------------------------------------------
// 2. 각 서브 화면 렌더링 함수들
// ---------------------------------------------------------------------------

// 1) LCD Test: 테두리, 대각선, 십자선, 체커보드, 컬러박스
static void render_lcd_test(uint16_t *buf) {
    render_header(buf, "LCD TEST");

    // 화면 영역 테두리
    gfx_draw_rect(buf, 2, 16, LCD_WIDTH - 4, 95, COLOR_WHITE);
    gfx_draw_rect(buf, 4, 18, LCD_WIDTH - 8, 91, COLOR_RED);

    // 십자선
    gfx_draw_line(buf, LCD_WIDTH / 2, 18, LCD_WIDTH / 2, 108, COLOR_GREEN);
    gfx_draw_line(buf, 4, (16 + 111) / 2, LCD_WIDTH - 5, (16 + 111) / 2, COLOR_GREEN);

    // 대각선
    gfx_draw_line(buf, 4, 18, LCD_WIDTH - 5, 108, COLOR_DARKGRAY);
    gfx_draw_line(buf, 4, 108, LCD_WIDTH - 5, 18, COLOR_DARKGRAY);

    // 컬러 블록
    gfx_fill_rect(buf, 10, 25, 20, 15, COLOR_RED);
    gfx_fill_rect(buf, 35, 25, 20, 15, COLOR_GREEN);
    gfx_fill_rect(buf, 60, 25, 20, 15, COLOR_BLUE);
    gfx_fill_rect(buf, 85, 25, 20, 15, COLOR_YELLOW);

    // 미니 체커보드
    for (int y = 75; y < 100; y += 5) {
        for (int x = 20; x < 105; x += 5) {
            if (((x / 5) + (y / 5)) % 2 == 0) {
                gfx_fill_rect(buf, x, y, 5, 5, COLOR_CYAN);
            }
        }
    }

    render_footer(buf, "Pattern OK", "[CANCEL]");
}

// 2) Display ID: 고유 보드 ID, 클럭, 패널 정보
static void render_display_id(uint16_t *buf) {
    render_header(buf, "DEVICE ID");

    char board_id_str[33] = "Unknown";
    pico_get_unique_board_id_string(board_id_str, sizeof(board_id_str));

    uint32_t sys_khz = clock_get_hz(clk_sys) / 1000;

    gfx_draw_string(buf, 4, 20, "MCU: RP2040", COLOR_CYAN, GFX_COLOR_TRANSPARENT, 1);
    
    char clk_buf[24];
    snprintf(clk_buf, sizeof(clk_buf), "Clock:%luMHz", (unsigned long)(sys_khz / 1000));
    gfx_draw_string(buf, 4, 34, clk_buf, COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);

    gfx_draw_string(buf, 4, 48, "Board ID:", COLOR_YELLOW, GFX_COLOR_TRANSPARENT, 1);
    // 16글자 ID를 8자씩 2줄로 표시
    char id_part1[9] = {0};
    char id_part2[9] = {0};
    strncpy(id_part1, board_id_str, 8);
    if (strlen(board_id_str) >= 8) {
        strncpy(id_part2, board_id_str + 8, 8);
    }
    gfx_draw_string(buf, 12, 60, id_part1, COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);
    gfx_draw_string(buf, 12, 72, id_part2, COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);

    gfx_draw_string(buf, 4, 88, "LCD: ST7735S", COLOR_GREEN, GFX_COLOR_TRANSPARENT, 1);

    uint8_t unique_id[8];
    flash_get_unique_id(unique_id);    

    char flash_buf[24];
    snprintf(flash_buf, sizeof(flash_buf), "F ID:%02X%02X_%02X%02X", 
            unique_id[0], unique_id[1], unique_id[2], unique_id[3]);

    gfx_draw_string(buf, 4, 100, flash_buf, COLOR_MAGENTA, GFX_COLOR_TRANSPARENT, 1);

    render_footer(buf, "Info Ready", "[CANCEL]");
}

// 3) Display Time: 대형 디지털 시계 및 아날로그 시계 애니메이션
static void render_display_time(uint16_t *buf) {
    render_header(buf, "TIME DISP");

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    uint32_t total_sec = now_ms / 1000;
    uint32_t hours = (total_sec / 3600) % 24;
    uint32_t minutes = (total_sec / 60) % 60;
    uint32_t seconds = total_sec % 60;
    uint32_t sub_sec = (now_ms % 1000) / 100;

    // 디지털 시계 텍스트 (Size 2)
    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%02lu:%02lu", (unsigned long)hours, (unsigned long)minutes);
    gfx_draw_string(buf, 18, 22, time_buf, COLOR_YELLOW, GFX_COLOR_TRANSPARENT, 2);

    char sec_buf[16];
    snprintf(sec_buf, sizeof(sec_buf), ":%02lu.%lu", (unsigned long)seconds, (unsigned long)sub_sec);
    gfx_draw_string(buf, 98, 26, sec_buf, COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);

    // 아날로그 원형 시계
    int center_x = 64;
    int center_y = 75;
    int radius = 28;
    gfx_draw_circle(buf, center_x, center_y, radius, COLOR_CYAN);
    gfx_draw_circle(buf, center_x, center_y, radius - 1, COLOR_DARKBLUE);
    gfx_fill_circle(buf, center_x, center_y, 2, COLOR_WHITE);

    // 12, 3, 6, 9 눈금
    gfx_draw_pixel(buf, center_x, center_y - radius + 2, COLOR_WHITE);
    gfx_draw_pixel(buf, center_x, center_y + radius - 2, COLOR_WHITE);
    gfx_draw_pixel(buf, center_x - radius + 2, center_y, COLOR_WHITE);
    gfx_draw_pixel(buf, center_x + radius - 2, center_y, COLOR_WHITE);

    // 초침 (Sec hand)
    float sec_angle = (seconds * 6.0f + (now_ms % 1000) * 0.006f) * 3.14159f / 180.0f - 1.5708f;
    int sec_x = center_x + (int)(cosf(sec_angle) * (radius - 4));
    int sec_y = center_y + (int)(sinf(sec_angle) * (radius - 4));
    gfx_draw_line(buf, center_x, center_y, sec_x, sec_y, COLOR_RED);

    // 분침 (Min hand)
    float min_angle = (minutes * 6.0f + seconds * 0.1f) * 3.14159f / 180.0f - 1.5708f;
    int min_x = center_x + (int)(cosf(min_angle) * (radius - 8));
    int min_y = center_y + (int)(sinf(min_angle) * (radius - 8));
    gfx_draw_line(buf, center_x, center_y, min_x, min_y, COLOR_GREEN);

    // 시침 (Hour hand)
    float hour_angle = ((hours % 12) * 30.0f + minutes * 0.5f) * 3.14159f / 180.0f - 1.5708f;
    int hour_x = center_x + (int)(cosf(hour_angle) * (radius - 14));
    int hour_y = center_y + (int)(sinf(hour_angle) * (radius - 14));
    gfx_draw_line(buf, center_x, center_y, hour_x, hour_y, COLOR_WHITE);

    render_footer(buf, "Clock Running", "[CANCEL]");
}

// 4) Display Color: 팔레트 및 그라디언트 바
static void render_display_color(uint16_t *buf) {
    render_header(buf, "COLOR PAL");

    static const struct {
        uint16_t color;
        const char *name;
    } palette[] = {
        {COLOR_RED,     "RED"},
        {COLOR_GREEN,   "GREEN"},
        {COLOR_BLUE,    "BLUE"},
        {COLOR_YELLOW,  "YELLOW"},
        {COLOR_CYAN,    "CYAN"},
        {COLOR_MAGENTA, "MAGEN"},
        {COLOR_ORANGE,  "ORANG"},
        {COLOR_WHITE,   "WHITE"}
    };

    // 8개 색상 블록
    for (int i = 0; i < 8; i++) {
        int x = 4 + (i % 4) * 30;
        int y = 20 + (i / 4) * 26;
        gfx_fill_rect(buf, x, y, 28, 14, palette[i].color);
        gfx_draw_rect(buf, x, y, 28, 14, COLOR_LIGHTGRAY);
        gfx_draw_string(buf, x + 2, y + 16, palette[i].name, COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);
    }

    // 그라디언트 바 3종 (R, G, B)
    gfx_draw_string(buf, 4, 76, "Gradients:", COLOR_LIGHTGRAY, GFX_COLOR_TRANSPARENT, 1);
    for (int x = 0; x < 120; x++) {
        uint8_t val5 = (x * 31) / 119;
        uint8_t val6 = (x * 63) / 119;

        uint16_t r_col = (val5 << 11);
        uint16_t g_col = (val6 << 5);
        uint16_t b_col = val5;

        gfx_draw_fast_v_line(buf, 4 + x, 88, 6, r_col);
        gfx_draw_fast_v_line(buf, 4 + x, 96, 6, g_col);
        gfx_draw_fast_v_line(buf, 4 + x, 104, 6, b_col);
    }

    render_footer(buf, "RGB565 16-Bit", "[CANCEL]");
}

// 5) Display Pattern: 기하학적 도형 및 동심원 패턴
static void render_display_pattern(uint16_t *buf) {
    render_header(buf, "PATTERNS");

    int center_x = 64;
    int center_y = 64;

    // 회전하는 방사형 선 패턴
    anim_angle += 0.05f;
    if (anim_angle > 6.28f) anim_angle = 0.0f;

    for (int i = 0; i < 12; i++) {
        float angle = anim_angle + (i * (6.28318f / 12.0f));
        int px = center_x + (int)(cosf(angle) * 44);
        int py = center_y + (int)(sinf(angle) * 44);
        uint16_t line_col = (i % 2 == 0) ? COLOR_CYAN : COLOR_MAGENTA;
        gfx_draw_line(buf, center_x, center_y, px, py, line_col);
    }

    // 동심원들
    for (int r = 10; r <= 42; r += 10) {
        gfx_draw_circle(buf, center_x, center_y, r, COLOR_YELLOW);
    }

    // 사각 프레임
    gfx_draw_rect(buf, 20, 20, 88, 88, COLOR_WHITE);
    gfx_draw_rect(buf, 35, 35, 58, 58, COLOR_GREEN);

    render_footer(buf, "Geometric", "[CANCEL]");
}

// 6) Display Progress: 게이지 바 및 회전 스피너
static void render_display_progress(uint16_t *buf) {
    render_header(buf, "PROGRESS");

    // 프로그레스 증가
    progress_val += 1.2f;
    if (progress_val > 100.0f) progress_val = 0.0f;

    int p = (int)progress_val;

    // 상단 텍스트
    char p_str[32];
    snprintf(p_str, sizeof(p_str), "Loading... %3d%%", p);
    gfx_draw_string(buf, 6, 22, p_str, COLOR_YELLOW, GFX_COLOR_TRANSPARENT, 1);

    // 1. 연속 프로그레스 바 (Linear bar)
    gfx_draw_rect(buf, 6, 36, 116, 14, COLOR_WHITE);
    int bar_fill_w = (p * 112) / 100;
    if (bar_fill_w > 0) {
        gfx_fill_rect(buf, 8, 38, bar_fill_w, 10, COLOR_GREEN);
    }

    // 2. 세그먼트 프로그레스 바
    gfx_draw_string(buf, 6, 56, "Block Segments:", COLOR_LIGHTGRAY, GFX_COLOR_TRANSPARENT, 1);
    for (int s = 0; s < 10; s++) {
        int x = 6 + (s * 12);
        gfx_draw_rect(buf, x, 70, 10, 10, COLOR_DARKGRAY);
        if (p >= (s + 1) * 10) {
            gfx_fill_rect(buf, x + 2, 72, 6, 6, COLOR_CYAN);
        }
    }

    // 3. 회전 스피너 (Spinner)
    int sp_cx = 64;
    int sp_cy = 98;
    int sp_r = 10;
    float sp_angle = (progress_val * 3.6f) * 3.14159f / 180.0f;
    int sp_x = sp_cx + (int)(cosf(sp_angle) * sp_r);
    int sp_y = sp_cy + (int)(sinf(sp_angle) * sp_r);

    gfx_draw_circle(buf, sp_cx, sp_cy, sp_r, COLOR_DARKBLUE);
    gfx_fill_circle(buf, sp_x, sp_y, 3, COLOR_ORANGE);

    render_footer(buf, "Auto Running", "[CANCEL]");
}

// 7) Display Text: 다양한 크기 및 색상 텍스트 데모
static void render_display_text(uint16_t *buf) {
    render_header(buf, "TEXT DEMO");

    gfx_draw_string(buf, 4, 18, "ST7735S 1.44\"", COLOR_CYAN, GFX_COLOR_TRANSPARENT, 1);
    gfx_draw_string(buf, 4, 30, "Font: 8x8 ASCII", COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);

    // 2x 스케일 폰트
    gfx_draw_string(buf, 4, 44, "SIZE 2X", COLOR_YELLOW, GFX_COLOR_TRANSPARENT, 2);

    gfx_draw_string(buf, 4, 66, "Red Green Blue", COLOR_MAGENTA, GFX_COLOR_TRANSPARENT, 1);
    
    // 반전 텍스트 박스
    gfx_fill_rect(buf, 4, 80, 120, 14, COLOR_WHITE);
    gfx_draw_string(buf, 8, 83, "INVERTED TEXT", COLOR_BLACK, GFX_COLOR_TRANSPARENT, 1);

    gfx_draw_string(buf, 4, 98, "Line 1234567890", COLOR_GREEN, GFX_COLOR_TRANSPARENT, 1);

    render_footer(buf, "Scale & Color", "[CANCEL]");
}

// 8) Display Menu: 계층형 서브메뉴 예시
static void render_display_menu(uint16_t *buf) {
    render_header(buf, "SUB MENU");

    const char *sub_items[] = {
        "Sub Setting 1",
        "Sub Setting 2",
        "Sub Setting 3",
        "Sub Setting 4"
    };

    gfx_draw_string(buf, 6, 20, "Select Option:", COLOR_YELLOW, GFX_COLOR_TRANSPARENT, 1);

    for (int i = 0; i < 4; i++) {
        int y = 36 + (i * 18);
        bool is_sel = (i == sub_menu_selected);

        if (is_sel) {
            gfx_fill_rect(buf, 6, y, 116, 14, COLOR_CYAN);
            char line[32];
            snprintf(line, sizeof(line), "* %s", sub_items[i]);
            gfx_draw_string(buf, 10, y + 3, line, COLOR_BLACK, GFX_COLOR_TRANSPARENT, 1);
        } else {
            gfx_draw_rect(buf, 6, y, 116, 14, COLOR_DARKGRAY);
            char line[32];
            snprintf(line, sizeof(line), "  %s", sub_items[i]);
            gfx_draw_string(buf, 10, y + 3, line, COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);
        }
    }

    render_footer(buf, "UP/DN:Select", "[CANCEL]");
}

// 9) Display Button: 4개 버튼의 실시간 눌림 상태를 그래픽 박스로 표시
static void render_display_button(uint16_t *buf) {
    render_header(buf, "BTN TEST");

    static const struct {
        button_id_t id;
        const char *name;
        int pin;
        int x;
        int y;
    } btns[] = {
        {BTN_UP,     "UP",     26, 44, 22},
        {BTN_DOWN,   "DOWN",   27, 44, 52},
        {BTN_CANCEL, "CANCEL", 28,  6, 82},
        {BTN_OK,     "OK",     29, 68, 82}
    };

    for (int i = 0; i < 4; i++) {
        bool down = button_is_down(btns[i].id);
        int x = btns[i].x;
        int y = btns[i].y;
        int w = (i >= 2) ? 54 : 40;
        int h = 24;

        if (down) {
            // 버튼 눌림 상태 (활성 녹색 배경)
            gfx_fill_rect(buf, x, y, w, h, COLOR_GREEN);
            gfx_draw_rect(buf, x, y, w, h, COLOR_WHITE);
            gfx_draw_string(buf, x + 4, y + 4, btns[i].name, COLOR_BLACK, GFX_COLOR_TRANSPARENT, 1);
            gfx_draw_string(buf, x + 4, y + 14, "PRESSED", COLOR_BLACK, GFX_COLOR_TRANSPARENT, 1);
        } else {
            // 버튼 뗌 상태 (회색 배경)
            gfx_fill_rect(buf, x, y, w, h, COLOR_DARKGRAY);
            gfx_draw_rect(buf, x, y, w, h, COLOR_LIGHTGRAY);
            gfx_draw_string(buf, x + 4, y + 4, btns[i].name, COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);
            char pin_str[16];
            snprintf(pin_str, sizeof(pin_str), "GP%d", btns[i].pin);
            gfx_draw_string(buf, x + 4, y + 14, pin_str, COLOR_YELLOW, GFX_COLOR_TRANSPARENT, 1);
        }
    }

    render_footer(buf, "Press Any Key", "[CANCEL]");
}

// 10) Exit: 대기 / 절전 화면
static void render_exit(uint16_t *buf) {
    render_header(buf, "STANDBY");

    // 중앙 안내 박스
    gfx_fill_rect(buf, 10, 35, 108, 60, COLOR_NAVY);
    gfx_draw_rect(buf, 10, 35, 108, 60, COLOR_CYAN);

    gfx_draw_string_centered(buf, 45, "SYSTEM IDLE", COLOR_YELLOW, GFX_COLOR_TRANSPARENT, 1);
    gfx_draw_string_centered(buf, 60, "Press [CANCEL]", COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);
    gfx_draw_string_centered(buf, 75, "to Resume", COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);

    render_footer(buf, "Idle Mode", "[CANCEL]");
}

// ---------------------------------------------------------------------------
// Public UI Functions
// ---------------------------------------------------------------------------

void ui_init(void) {
    current_state = UI_STATE_MAIN_MENU;
    selected_menu = 0;
    scroll_top = 0;
    sub_menu_selected = 0;
    progress_val = 0.0f;
    anim_angle = 0.0f;
}

void ui_update(void) {
    // 1. CANCEL 버튼은 어디에서든 메인 메뉴로 복귀 (project.txt 요구사항)
    if (button_was_pressed(BTN_CANCEL)) {
        if (current_state != UI_STATE_MAIN_MENU) {
            current_state = UI_STATE_MAIN_MENU;
            return;
        }
    }

    // 2. 현재 화면 상태에 따른 키 입력 처리
    if (current_state == UI_STATE_MAIN_MENU) {
        // UP 버튼: 메뉴 위로 이동
        if (button_was_pressed(BTN_UP)) {
            if (selected_menu > 0) {
                selected_menu--;
                if (selected_menu < scroll_top) {
                    scroll_top = selected_menu;
                }
            } else {
                // 맨 위에서 UP 누르면 맨 아래로 래핑
                selected_menu = MENU_ITEM_COUNT - 1;
                scroll_top = MENU_ITEM_COUNT - MENU_VISIBLE_COUNT;
            }
        }

        // DOWN 버튼: 메뉴 아래로 이동
        if (button_was_pressed(BTN_DOWN)) {
            if (selected_menu < MENU_ITEM_COUNT - 1) {
                selected_menu++;
                if (selected_menu >= scroll_top + MENU_VISIBLE_COUNT) {
                    scroll_top = selected_menu - MENU_VISIBLE_COUNT + 1;
                }
            } else {
                // 맨 아래에서 DOWN 누르면 맨 위로 래핑
                selected_menu = 0;
                scroll_top = 0;
            }
        }

        // OK 버튼: 선택된 메뉴 진입
        if (button_was_pressed(BTN_OK)) {
            current_state = (ui_state_t)(selected_menu + 1);
            progress_val = 0.0f;
            sub_menu_selected = 0;
        }
    } else if (current_state == UI_STATE_DISPLAY_MENU) {
        // 서브메뉴 내에서의 UP / DOWN
        if (button_was_pressed(BTN_UP)) {
            if (sub_menu_selected > 0) sub_menu_selected--;
            else sub_menu_selected = 3;
        }
        if (button_was_pressed(BTN_DOWN)) {
            if (sub_menu_selected < 3) sub_menu_selected++;
            else sub_menu_selected = 0;
        }
    }
}

void ui_render(uint16_t *buf) {
    if (!buf) return;

    // 기본 배경색을 검은색으로 클리어
    gfx_clear(buf, COLOR_BLACK);

    switch (current_state) {
        case UI_STATE_MAIN_MENU:
            render_main_menu(buf);
            break;
        case UI_STATE_LCD_TEST:
            render_lcd_test(buf);
            break;
        case UI_STATE_DISPLAY_ID:
            render_display_id(buf);
            break;
        case UI_STATE_DISPLAY_TIME:
            render_display_time(buf);
            break;
        case UI_STATE_DISPLAY_COLOR:
            render_display_color(buf);
            break;
        case UI_STATE_DISPLAY_PATTERN:
            render_display_pattern(buf);
            break;
        case UI_STATE_DISPLAY_PROGRESS:
            render_display_progress(buf);
            break;
        case UI_STATE_DISPLAY_TEXT:
            render_display_text(buf);
            break;
        case UI_STATE_DISPLAY_MENU:
            render_display_menu(buf);
            break;
        case UI_STATE_DISPLAY_BUTTON:
            render_display_button(buf);
            break;
        case UI_STATE_EXIT:
            render_exit(buf);
            break;
        default:
            current_state = UI_STATE_MAIN_MENU;
            render_main_menu(buf);
            break;
    }
}
