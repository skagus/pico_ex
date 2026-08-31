#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "ui_menu.h"
#include "button.h"
#include "gfx.h"
#include "st7735s.h"

// ---------------------------------------------------------------------------
// Forward declarations (list_item_func 시그니처)
// ---------------------------------------------------------------------------
static widget_act wnd_lcd_test(uint16_t *buf, uint8_t* ctx, bool first_call);
static widget_act wnd_display_id(uint16_t *buf, uint8_t* ctx, bool first_call);
static widget_act wnd_display_time(uint16_t *buf, uint8_t* ctx, bool first_call);
static widget_act wnd_display_color(uint16_t *buf, uint8_t* ctx, bool first_call);
static widget_act wnd_display_pattern(uint16_t *buf, uint8_t* ctx, bool first_call);
static widget_act wnd_display_progress(uint16_t *buf, uint8_t* ctx, bool first_call);
static widget_act wnd_display_text(uint16_t *buf, uint8_t* ctx, bool first_call);
static widget_act wnd_display_menu(uint16_t *buf, uint8_t* ctx, bool first_call);
static widget_act wnd_display_button(uint16_t *buf, uint8_t* ctx, bool first_call);

// ---------------------------------------------------------------------------
// 메뉴 항목 정의 (list_widget)
// ---------------------------------------------------------------------------
static list_widget menus =
{
    .num_items = 9,
    .title = "ST7735",
    .item = {
        "LCD Test",
        "Show ID",
        "Show Time",
        "Show Color",
        "Show Pattern",
        "Show Progress",
        "Show Text",
        "Show Menu",
        "Show Button",
    },
    .item_func = {
        wnd_lcd_test,
        wnd_display_id,
        wnd_display_time,
        wnd_display_color,
        wnd_display_pattern,
        wnd_display_progress,
        wnd_display_text,
        wnd_display_menu,
        wnd_display_button,
    }
};


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
    int footer_y = LCD_HEIGHT - 14;
    // Status Bar 배경 (짙은 회색)
    gfx_fill_rect(buf, 0, footer_y, LCD_WIDTH, 14, COLOR_DARKGRAY);
    gfx_draw_fast_h_line(buf, 0, footer_y - 1, LCD_WIDTH, COLOR_LIGHTGRAY);

    if (left_hint) {
        gfx_draw_string(buf, 3, footer_y + 3, left_hint, COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);
    }
    if (right_hint) {
        int len = (int)strlen(right_hint);
        int x = LCD_WIDTH - (len * 8) - 2;
        gfx_draw_string(buf, x, footer_y + 3, right_hint, COLOR_CYAN, GFX_COLOR_TRANSPARENT, 1);
    }
}

// ---------------------------------------------------------------------------
// 메뉴 리스트 렌더링 (menu_ctx 기반)
// ---------------------------------------------------------------------------
static void render_menu_list(uint16_t *buf, int selected, int scroll_top) {

    // 1. Title Bar
    render_header(buf, menus.title);

    // 2. Body: 메뉴 항목 표시 (반응형 행 높이)
    int start_y = 16;
    int footer_y = LCD_HEIGHT - 14;
    int available_h = footer_y - start_y;
    int item_height = available_h / MENU_VISIBLE_COUNT;

    for (int i = 0; i < MENU_VISIBLE_COUNT; i++) {
        int item_idx = scroll_top + i;
        if (item_idx >= menus.num_items) break;

        int y = start_y + (i * item_height);
        bool is_selected = (item_idx == selected);

        if (is_selected) {
            // 현재 선택된 메뉴는 역상(Inverted) 하이라이트 표시
            gfx_fill_rect(buf, 2, y + 1, LCD_WIDTH - 10, item_height - 2, COLOR_WHITE);
            
            // 번호와 메뉴명 출력 (검은색 폰트)
            char text[32];
            snprintf(text, sizeof(text), ">%d.%-11s", item_idx + 1, menus.item[item_idx]);
            gfx_draw_string(buf, 4, y + (item_height - 8) / 2, text, COLOR_BLACK, GFX_COLOR_TRANSPARENT, 1);
        } else {
            // 미선택 메뉴는 일반 텍스트
            gfx_fill_rect(buf, 2, y + 1, LCD_WIDTH - 10, item_height - 2, COLOR_BLACK);
            char text[32];
            snprintf(text, sizeof(text), " %d.%-11s", item_idx + 1, menus.item[item_idx]);
            gfx_draw_string(buf, 4, y + (item_height - 8) / 2, text, COLOR_LIGHTGRAY, GFX_COLOR_TRANSPARENT, 1);
        }
    }

    // 3. 우측 스크롤 바 인디케이터
    int track_x = LCD_WIDTH - 6;
    int track_y = start_y;
    int track_h = item_height * MENU_VISIBLE_COUNT;
    gfx_fill_rect(buf, track_x, track_y, 4, track_h, COLOR_DARKGRAY);

    
    // 스크롤 썸(Thumb) 계산
    int thumb_h = (track_h * MENU_VISIBLE_COUNT) / menus.num_items;
    int max_scroll = menus.num_items - MENU_VISIBLE_COUNT;
    int thumb_y = track_y;
    if (max_scroll > 0) {
        thumb_y += (scroll_top * (track_h - thumb_h)) / max_scroll;
    }
    gfx_fill_rect(buf, track_x, thumb_y, 4, thumb_h, COLOR_CYAN);

    // 4. Status Bar (하단 인디케이터 및 버튼 가이드)
    char status_str[32];
    snprintf(status_str, sizeof(status_str), "[%02d/%02d]", selected + 1, menus.num_items);
    render_footer(buf, status_str, "OK:Enter");
}

// ---------------------------------------------------------------------------
// 각 서브 화면 렌더링 함수들 (list_item_func 시그니처)
// ---------------------------------------------------------------------------

// 1) LCD Test: 테두리, 대각선, 십자선, 체커보드, 컬러박스
static widget_act wnd_lcd_test(uint16_t *buf, uint8_t* ctx, bool first_call) {
    if(first_call) {
        gfx_clear(buf, COLOR_BLACK);
        render_header(buf, "LCD TEST");

        int body_top = 18;
        int body_bottom = LCD_HEIGHT - 18;
        int body_h = body_bottom - body_top;

        // 화면 영역 테두리
        gfx_draw_rect(buf, 2, body_top, LCD_WIDTH - 4, body_h, COLOR_WHITE);
        gfx_draw_rect(buf, 4, body_top + 2, LCD_WIDTH - 8, body_h - 4, COLOR_RED);

        // 십자선
        gfx_draw_line(buf, LCD_WIDTH / 2, body_top + 2, LCD_WIDTH / 2, body_bottom - 2, COLOR_GREEN);
        gfx_draw_line(buf, 4, (body_top + body_bottom) / 2, LCD_WIDTH - 5, (body_top + body_bottom) / 2, COLOR_GREEN);

        // 대각선
        gfx_draw_line(buf, 4, body_top + 2, LCD_WIDTH - 5, body_bottom - 2, COLOR_DARKGRAY);
        gfx_draw_line(buf, 4, body_bottom - 2, LCD_WIDTH - 5, body_top + 2, COLOR_DARKGRAY);

        // 컬러 블록
        int block_w = (LCD_WIDTH >= 240) ? 40 : 20;
        int block_h = (LCD_HEIGHT >= 320) ? 25 : 15;
        int block_y = body_top + 10;
        int block_spacing = (LCD_WIDTH - 20) / 4;
        gfx_fill_rect(buf, 10 + 0 * block_spacing, block_y, block_w, block_h, COLOR_RED);
        gfx_fill_rect(buf, 10 + 1 * block_spacing, block_y, block_w, block_h, COLOR_GREEN);
        gfx_fill_rect(buf, 10 + 2 * block_spacing, block_y, block_w, block_h, COLOR_BLUE);
        gfx_fill_rect(buf, 10 + 3 * block_spacing, block_y, block_w, block_h, COLOR_YELLOW);

        // 미니 체커보드
        int checker_y_start = body_bottom - 35;
        int checker_x_start = LCD_WIDTH / 2 - 40;
        for (int y = checker_y_start; y < checker_y_start + 25; y += 5) {
            for (int x = checker_x_start; x < checker_x_start + 80; x += 5) {
                if (((x / 5) + (y / 5)) % 2 == 0) {
                    gfx_fill_rect(buf, x, y, 5, 5, COLOR_CYAN);
                }
            }
        }

        render_footer(buf, "Pattern OK", "[CANCEL]");
    }

    // CANCEL로 상위 메뉴 복귀
    if (button_was_pressed(BTN_CANCEL)) {
        return WG_EXIT;
    }
    return WG_NONE;
}

// 2) Display ID: 고유 보드 ID, 클럭, 패널 정보
static widget_act wnd_display_id(uint16_t *buf, uint8_t* ctx, bool first_call) {
    if(first_call) {
        gfx_clear(buf, COLOR_BLACK);
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

        char lcd_info[24];
        snprintf(lcd_info, sizeof(lcd_info), "LCD: %s", LCD_NAME);
        gfx_draw_string(buf, 4, 88, lcd_info, COLOR_GREEN, GFX_COLOR_TRANSPARENT, 1);

        uint8_t unique_id[8];
        flash_get_unique_id(unique_id);    

        char flash_buf[24];
        snprintf(flash_buf, sizeof(flash_buf), "F ID:%02X%02X_%02X%02X", 
                unique_id[0], unique_id[1], unique_id[2], unique_id[3]);

        gfx_draw_string(buf, 4, 100, flash_buf, COLOR_MAGENTA, GFX_COLOR_TRANSPARENT, 1);

        render_footer(buf, "Info Ready", "[CANCEL]");
    }

    if (button_was_pressed(BTN_CANCEL)) {
        return WG_EXIT;
    }
    return WG_NONE;
}

// 3) Display Time: 대형 디지털 시계 및 아날로그 시계 애니메이션
static widget_act wnd_display_time(uint16_t *buf, uint8_t* ctx, bool first_call) {
    gfx_clear(buf, COLOR_BLACK);
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

    if (button_was_pressed(BTN_CANCEL)) {
        return WG_EXIT;
    }
    return WG_NONE;
}

// 4) Display Color: 팔레트 및 그라디언트 바
static widget_act wnd_display_color(uint16_t *buf, uint8_t* ctx, bool first_call) {
    if(first_call) {
        gfx_clear(buf, COLOR_BLACK);
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

    if (button_was_pressed(BTN_CANCEL)) {
        return WG_EXIT;
    }
    return WG_NONE;
}

// 5) Display Pattern: 기하학적 도형 및 동심원 패턴
static widget_act wnd_display_pattern(uint16_t *buf, uint8_t* ctx, bool first_call) {
    struct {
        float anim_angle;
    } * my_ctx = ctx;

    if(first_call)
    {
        my_ctx->anim_angle = 0.0f;
    }

    gfx_clear(buf, COLOR_BLACK);
    render_header(buf, "PATTERNS");

    int center_x = 64;
    int center_y = 64;

    // 회전하는 방사형 선 패턴
    my_ctx->anim_angle += 0.05f;
    if (my_ctx->anim_angle > 6.28f) my_ctx->anim_angle = 0.0f;

    for (int i = 0; i < 12; i++) {
        float angle = my_ctx->anim_angle + (i * (6.28318f / 12.0f));
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

    if (button_was_pressed(BTN_CANCEL)) {
        return WG_EXIT;
    }
    return WG_NONE;
}

// 6) Display Progress: 게이지 바 및 회전 스피너
static widget_act wnd_display_progress(uint16_t *buf, uint8_t* ctx, bool first_call) {

    struct {
        float progress_val;
    } * my_ctx = ctx;

    if(first_call)
    {
        my_ctx->progress_val = 0.0f;
    }

    gfx_clear(buf, COLOR_BLACK);
    render_header(buf, "PROGRESS");

    // 프로그레스 증가
    my_ctx->progress_val += 1.2f;
    if (my_ctx->progress_val > 100.0f) my_ctx->progress_val = 0.0f;

    int p = (int)my_ctx->progress_val;

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
    float sp_angle = (my_ctx->progress_val * 3.6f) * 3.14159f / 180.0f;
    int sp_x = sp_cx + (int)(cosf(sp_angle) * sp_r);
    int sp_y = sp_cy + (int)(sinf(sp_angle) * sp_r);

    gfx_draw_circle(buf, sp_cx, sp_cy, sp_r, COLOR_DARKBLUE);
    gfx_fill_circle(buf, sp_x, sp_y, 3, COLOR_ORANGE);

    render_footer(buf, "Auto Running", "[CANCEL]");

    if (button_was_pressed(BTN_CANCEL)) {
        return WG_EXIT;
    }
    return WG_NONE;
}

// 7) Display Text: 다양한 크기 및 색상 텍스트 데모
static widget_act wnd_display_text(uint16_t *buf, uint8_t* ctx, bool first_call) {
    if(first_call) {
        gfx_clear(buf, COLOR_BLACK);
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

    if (button_was_pressed(BTN_CANCEL)) {
        return WG_EXIT;
    }
    return WG_NONE;
}

// 8) Display Menu: 계층형 서브메뉴 예시
static widget_act wnd_display_menu(uint16_t *buf, uint8_t* ctx, bool first_call) {
    gfx_clear(buf, COLOR_BLACK);
    render_header(buf, "SUB MENU");

    struct {
        int sub_menu_selected;
    }* my_ctx = ctx;

    if (first_call) {
        my_ctx->sub_menu_selected = 0;
    }

    // 서브메뉴 내에서의 UP / DOWN
    if (button_was_pressed(BTN_UP)) {
        if (my_ctx->sub_menu_selected > 0) my_ctx->sub_menu_selected--;
        else my_ctx->sub_menu_selected = 3;
    }
    if (button_was_pressed(BTN_DOWN)) {
        if (my_ctx->sub_menu_selected < 3) my_ctx->sub_menu_selected++;
        else my_ctx->sub_menu_selected = 0;
    }

    const char *sub_items[] = {
        "Sub Setting 1",
        "Sub Setting 2",
        "Sub Setting 3",
        "Sub Setting 4"
    };

    gfx_draw_string(buf, 6, 20, "Select Option:", COLOR_YELLOW, GFX_COLOR_TRANSPARENT, 1);

    for (int i = 0; i < 4; i++) {
        int y = 36 + (i * 18);
        bool is_sel = (i == my_ctx->sub_menu_selected);

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

    if (button_was_pressed(BTN_CANCEL)) {
        return WG_EXIT;
    }
    return WG_NONE;
}

// 9) Display Button: 4개 버튼의 실시간 눌림 상태를 그래픽 박스로 표시
static widget_act wnd_display_button(uint16_t *buf, uint8_t* ctx, bool first_call) {
    gfx_clear(buf, COLOR_BLACK);
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

    if (button_was_pressed(BTN_CANCEL)) {
        return WG_EXIT;
    }
    return WG_NONE;
}

// ---------------------------------------------------------------------------
// 메뉴 위젯: context stack 기반 list menu
// ---------------------------------------------------------------------------
// ctx_buf 레이아웃:
//   [main_ctx] [menu_ctx] [자식 위젯 ctx ...]
//              ^-- p_ctx 가 가리키는 위치
// ---------------------------------------------------------------------------
widget_act wnd_menu(uint16_t *buf, uint8_t* ctx, bool b_1st) {
    if (!buf) return WG_NONE;

    struct _menu_ctx {
        bool b_active;      // true: 메뉴 리스트 표시 중, false: 자식 위젯 활성
        int selected;       // 현재 선택된 항목 인덱스
        int scroll_top;     // 스크롤 시작 인덱스
    }* my_ctx = ctx;

    // 첫 진입: context 초기화 및 메뉴 리스트 그리기
    if (b_1st) {
        memset(my_ctx, 0x0, sizeof(struct _menu_ctx));
        my_ctx->b_active = true;
        my_ctx->selected = 0;
        my_ctx->scroll_top = 0;
        gfx_clear(buf, COLOR_BLACK);
        render_menu_list(buf, my_ctx->selected, my_ctx->scroll_top);
        return WG_NONE;
    }

    // 자식 위젯이 활성 상태인 경우 (b_active == false)
    if (!my_ctx->b_active) {
        widget_act ret = menus.item_func[my_ctx->selected](buf, my_ctx + 1, false);
        if (WG_EXIT == ret) {
            my_ctx->b_active = true;
            gfx_clear(buf, COLOR_BLACK);
            render_menu_list(buf, my_ctx->selected, my_ctx->scroll_top);
        }
        return WG_NONE;
    }

    // 입력 처리
    if (button_was_pressed(BTN_CANCEL)) {
        return WG_EXIT;
    }
    else if (button_was_pressed(BTN_OK)) {
        // 선택된 항목의 자식 위젯 진입
        my_ctx->b_active = false;
        // 자식 위젯 첫 호출
        menus.item_func[my_ctx->selected](buf, my_ctx + 1, true);
        return WG_NONE;
    }

    if (button_was_pressed(BTN_UP)) {
        if (my_ctx->selected > 0) {
            my_ctx->selected--;
            if (my_ctx->selected < my_ctx->scroll_top) {
                my_ctx->scroll_top = my_ctx->selected;
            }
        } else {
            // 맨 위에서 UP → 맨 아래로 래핑
            my_ctx->selected = menus.num_items - 1;
            if (menus.num_items > MENU_VISIBLE_COUNT) {
                my_ctx->scroll_top = menus.num_items - MENU_VISIBLE_COUNT;
            }
        }
    }

    if (button_was_pressed(BTN_DOWN)) {
        if (my_ctx->selected < menus.num_items - 1) {
            my_ctx->selected++;
            if (my_ctx->selected >= my_ctx->scroll_top + MENU_VISIBLE_COUNT) {
                my_ctx->scroll_top = my_ctx->selected - MENU_VISIBLE_COUNT + 1;
            }
        } else {
            // 맨 아래에서 DOWN → 맨 위로 래핑
            my_ctx->selected = 0;
            my_ctx->scroll_top = 0;
        }
    }

    // 메뉴 리스트가 활성 상태 (b_active == true)
    gfx_clear(buf, COLOR_BLACK);
    render_menu_list(buf, my_ctx->selected, my_ctx->scroll_top);

    return WG_NONE;
}

// ---------------------------------------------------------------------------
// 최상위 메인 화면 (대기/Standby)
// ---------------------------------------------------------------------------
static void render_main(uint16_t *buf) {
    gfx_clear(buf, COLOR_BLACK);
    render_header(buf, "STANDBY");

    // 중앙 안내 박스
    gfx_fill_rect(buf, 10, 35, 108, 60, COLOR_NAVY);
    gfx_draw_rect(buf, 10, 35, 108, 60, COLOR_CYAN);

    gfx_draw_string_centered(buf, 45, "SYSTEM IDLE", COLOR_YELLOW, GFX_COLOR_TRANSPARENT, 1);
    gfx_draw_string_centered(buf, 60, "Press [OK]", COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);
    gfx_draw_string_centered(buf, 75, "to Enter Menu", COLOR_WHITE, GFX_COLOR_TRANSPARENT, 1);

    render_footer(buf, "Idle Mode", "OK:Menu");
}

// ---------------------------------------------------------------------------
// Context buffer & 최상위 컨트롤러
// ---------------------------------------------------------------------------
// ctx_buf 레이아웃:
//   [main_ctx (4B)] [menu_ctx (12B)] [자식 위젯 ctx ...]
// ---------------------------------------------------------------------------
static uint8_t ctx_buf[512];

typedef struct _main_ctx {
    bool b_active;  // true: standby 화면, false: 메뉴 위젯 활성
    uint8_t reserved[3];
} main_ctx;

void ui_init(void) {
    memset(ctx_buf, 0, sizeof(ctx_buf));
    main_ctx* p = (main_ctx*)ctx_buf;
    p->b_active = true;
}

void ui_main(uint16_t* buf) {
    if (!buf) return;

    main_ctx* p_ctx = (main_ctx*)ctx_buf;

    button_update();

    if (p_ctx->b_active) {
        // Standby 화면
        if (button_was_pressed(BTN_OK)) {
            p_ctx->b_active = false;
            wnd_menu(buf, (uint8_t*)p_ctx + sizeof(main_ctx), true);
        }
        else{
            render_main(buf);
        }
    }
    else {
        // 메뉴 위젯 활성
        widget_act ret = wnd_menu(buf, p_ctx + 1, false);
        if (ret == WG_EXIT) {
            // 메뉴에서 Exit → Standby 화면으로
            p_ctx->b_active = true;
            render_main(buf);
        }
    }
}
