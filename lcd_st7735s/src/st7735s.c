#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "st7735s.h"
#include "spi_man.h"

// ---------------------------------------------------------------------------
// Internal Variables & Helpers
// ---------------------------------------------------------------------------

// 더블 프레임 버퍼 (128 x 128 x 2버퍼, 64KB)
uint16_t g_frame_buffers[2][LCD_PIXELS];

// 동기 SPI 완료 플래그 (post_exec에 의해 세팅)
static volatile bool s_spi_done = false;

/**
 * @brief CS 핀을 Low로 설정 (SPI 전송 시작)
 */
static inline void lcd_cs_select(void) {
    gpio_put(PIN_CS, 0);
}

/**
 * @brief CS 핀을 High로 설정 (SPI 전송 종료)
 */
static inline void lcd_cs_deselect(void) {
    gpio_put(PIN_CS, 1);
}

/**
 * @brief DC 핀을 Low로 설정 (Command 모드)
 */
static inline void lcd_dc_command(void) {
    gpio_put(PIN_DC, 0);
}

/**
 * @brief DC 핀을 High로 설정 (Data 모드)
 */
static inline void lcd_dc_data(void) {
    gpio_put(PIN_DC, 1);
}

// ---------------------------------------------------------------------------
// Callback Functions for spi_man
// ---------------------------------------------------------------------------

static void lcd_cmd_pre_exec(spi_hw_t *hw) {
    (void)hw;
    spi_set_format(LCD_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    lcd_dc_command();
    lcd_cs_select();
}

static void lcd_cmd_post_exec(spi_hw_t *hw) {
    (void)hw;
    lcd_cs_deselect();
    s_spi_done = true;
}

static void lcd_data_pre_exec(spi_hw_t *hw) {
    (void)hw;
    spi_set_format(LCD_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    lcd_dc_data();
    lcd_cs_select();
}

static void lcd_data_post_exec(spi_hw_t *hw) {
    (void)hw;
    lcd_cs_deselect();
    s_spi_done = true;
}

static void lcd_fb_pre_exec(spi_hw_t *hw) {
    (void)hw;
    spi_set_format(LCD_SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    lcd_dc_data();
    lcd_cs_select();
}

// LCD 비동기 프레임버퍼 DMA 전송 완료 플래그 (post_exec에 의해 클리어)
static volatile bool s_lcd_busy = false;

static void lcd_fb_post_exec(spi_hw_t *hw) {
    (void)hw;
    lcd_cs_deselect();
    spi_set_format(LCD_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    s_lcd_busy = false;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool lcd_is_busy(void) {
    return s_lcd_busy;
}

static void lcd_wait_idle(void) {
    while (s_lcd_busy) {
        tight_loop_contents();
    }
}

static void lcd_write_cmd(uint8_t cmd) {
    static uint8_t s_cmd;
    s_cmd = cmd;
    s_spi_done = false;

    spi_req_t *req = spi_alloc_req();
    req->data = &s_cmd;
    req->len = 1;
    req->pre_exec = lcd_cmd_pre_exec;
    req->post_exec = lcd_cmd_post_exec;

    spi_push_req(req);

    // post_exec에 의한 SPI completion 대기
    while (!s_spi_done) {
        tight_loop_contents();
    }
}

static void lcd_write_data(const uint8_t *data, size_t len) {
    if (len == 0) return;
    s_spi_done = false;

    spi_req_t *req = spi_alloc_req();
    req->data = (uint8_t *)data;
    req->len = len;
    req->pre_exec = lcd_data_pre_exec;
    req->post_exec = lcd_data_post_exec;

    spi_push_req(req);

    // post_exec에 의한 SPI completion 대기
    while (!s_spi_done) {
        tight_loop_contents();
    }
}

static void lcd_write_data_byte(uint8_t data) {
    static uint8_t s_byte;
    s_byte = data;
    s_spi_done = false;

    spi_req_t *req = spi_alloc_req();
    req->data = &s_byte;
    req->len = 1;
    req->pre_exec = lcd_data_pre_exec;
    req->post_exec = lcd_data_post_exec;

    spi_push_req(req);

    // post_exec에 의한 SPI completion 대기
    while (!s_spi_done) {
        tight_loop_contents();
    }
}

static void lcd_hw_reset(void) {
    gpio_put(PIN_RES, 1);
    sleep_ms(10);
    gpio_put(PIN_RES, 0);
    sleep_ms(10);
    gpio_put(PIN_RES, 1);
    sleep_ms(120);
}

static void lcd_set_window_raw(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    lcd_write_cmd(ST7735_CASET);
    uint8_t col_data[] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    for (int i = 0; i < 4; i++) {
        lcd_write_data_byte(col_data[i]);
    }

    lcd_write_cmd(ST7735_RASET);
    uint8_t row_data[] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    for (int i = 0; i < 4; i++) {
        lcd_write_data_byte(row_data[i]);
    }

    lcd_write_cmd(ST7735_RAMWR);
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    lcd_set_window_raw(x0 + LCD_X_OFFSET, y0 + LCD_Y_OFFSET,
                       x1 + LCD_X_OFFSET, y1 + LCD_Y_OFFSET);
}

void lcd_draw_frame_buffer(const uint16_t *fb) {
    lcd_wait_idle();
    s_lcd_busy = true;

    // 윈도우 설정 (0,0)~(127,127) 및 RAMWR 커맨드 전송
    lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    // 16비트 SPI DMA 전송 요청 등록 (비동기 처리)
    spi_req_t *req = spi_alloc_req();
    req->data = (uint8_t *)fb;
    req->len = LCD_PIXELS; // 16비트 워드 수
    req->pre_exec = lcd_fb_pre_exec;
    req->post_exec = lcd_fb_post_exec;

    spi_push_req(req);
}

void lcd_fill_color(uint16_t *fb, uint16_t color) {
    for (size_t i = 0; i < LCD_PIXELS; i++) {
        fb[i] = color;
    }
}

void lcd_init(void) {
    // -----------------------------------------------------------------------
    // 1. SPI 초기화 (기본 8비트 모드)
    // -----------------------------------------------------------------------
    spi_init(LCD_SPI_PORT, LCD_SPI_FREQ);
    spi_set_format(LCD_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // -----------------------------------------------------------------------
    // 2. GPIO 초기화 (CS, DC, RES, BL)
    // -----------------------------------------------------------------------
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);    // CS idle high

    gpio_init(PIN_DC);
    gpio_set_dir(PIN_DC, GPIO_OUT);

    gpio_init(PIN_RES);
    gpio_set_dir(PIN_RES, GPIO_OUT);

    gpio_init(PIN_BL);
    gpio_set_dir(PIN_BL, GPIO_OUT);
    gpio_put(PIN_BL, 1);   // Backlight ON

    // -----------------------------------------------------------------------
    // 3. SPI Request Manager 및 DMA 초기화
    // -----------------------------------------------------------------------
    spi_man_init(LCD_SPI_PORT);

    // -----------------------------------------------------------------------
    // 4. 하드웨어 리셋
    // -----------------------------------------------------------------------
    lcd_hw_reset();

    // -----------------------------------------------------------------------
    // 5. LCD 초기화 시퀀스
    // -----------------------------------------------------------------------

    // Software Reset
    lcd_write_cmd(ST7735_SWRESET);
    sleep_ms(150);

    // Sleep Out
    lcd_write_cmd(ST7735_SLPOUT);
    sleep_ms(150);

    // Frame Rate Control (Normal mode) - 최대 갱신 주기를 위해 설정
    lcd_write_cmd(ST7735_FRMCTR1);
    lcd_write_data_byte(0x01);  // RTNA
    lcd_write_data_byte(0x2C);  // Front porch
    lcd_write_data_byte(0x2D);  // Back porch

    // Frame Rate Control (Idle mode)
    lcd_write_cmd(ST7735_FRMCTR2);
    lcd_write_data_byte(0x01);
    lcd_write_data_byte(0x2C);
    lcd_write_data_byte(0x2D);

    // Frame Rate Control (Partial mode)
    lcd_write_cmd(ST7735_FRMCTR3);
    lcd_write_data_byte(0x01);
    lcd_write_data_byte(0x2C);
    lcd_write_data_byte(0x2D);
    lcd_write_data_byte(0x01);
    lcd_write_data_byte(0x2C);
    lcd_write_data_byte(0x2D);

    // Display Inversion Control
    lcd_write_cmd(ST7735_INVCTR);
    lcd_write_data_byte(0x07);

    // Power Control 1
    lcd_write_cmd(ST7735_PWCTR1);
    lcd_write_data_byte(0xA2);
    lcd_write_data_byte(0x02);
    lcd_write_data_byte(0x84);

    // Power Control 2
    lcd_write_cmd(ST7735_PWCTR2);
    lcd_write_data_byte(0xC5);

    // Power Control 3 (Normal mode)
    lcd_write_cmd(ST7735_PWCTR3);
    lcd_write_data_byte(0x0A);
    lcd_write_data_byte(0x00);

    // Power Control 4 (Idle mode)
    lcd_write_cmd(ST7735_PWCTR4);
    lcd_write_data_byte(0x8A);
    lcd_write_data_byte(0x2A);

    // Power Control 5 (Partial mode)
    lcd_write_cmd(ST7735_PWCTR5);
    lcd_write_data_byte(0x8A);
    lcd_write_data_byte(0xEE);

    // VCOM Control
    lcd_write_cmd(ST7735_VMCTR1);
    lcd_write_data_byte(0x0E);

    // Display Inversion OFF
    lcd_write_cmd(ST7735_INVOFF);

    // Memory Data Access Control (180도 회전: Row/Column Order 반전, BGR color order)
    lcd_write_cmd(ST7735_MADCTL);
    lcd_write_data_byte(MADCTL_MY | MADCTL_MX | MADCTL_BGR);

    // Interface Pixel Format: 16-bit (RGB565)
    lcd_write_cmd(ST7735_COLMOD);
    lcd_write_data_byte(0x05);

    // Positive Gamma Correction
    lcd_write_cmd(ST7735_GMCTRP1);
    {
        uint8_t gamma_p[] = {
            0x02, 0x1C, 0x07, 0x12,
            0x37, 0x32, 0x29, 0x2D,
            0x29, 0x25, 0x2B, 0x39,
            0x00, 0x01, 0x03, 0x10
        };
        for (size_t i = 0; i < sizeof(gamma_p); i++) {
            lcd_write_data_byte(gamma_p[i]);
        }
    }

    // Negative Gamma Correction
    lcd_write_cmd(ST7735_GMCTRN1);
    {
        uint8_t gamma_n[] = {
            0x03, 0x1D, 0x07, 0x06,
            0x2E, 0x2C, 0x29, 0x2D,
            0x2E, 0x2E, 0x37, 0x3F,
            0x00, 0x00, 0x02, 0x10
        };
        for (size_t i = 0; i < sizeof(gamma_n); i++) {
            lcd_write_data_byte(gamma_n[i]);
        }
    }

    // Normal Display Mode ON
    lcd_write_cmd(ST7735_NORON);
    sleep_ms(10);

    // Display ON
    lcd_write_cmd(ST7735_DISPON);
    sleep_ms(100);

    // ST7735S 전체 GDDRAM(132x162)을 검은색으로 밀어서 테두리 쓰레기값 완전 제거
    lcd_set_window_raw(0, 0, ST7735_RAM_WIDTH - 1, ST7735_RAM_HEIGHT - 1);
    uint16_t *zero_buf = g_frame_buffers[0];
    memset(zero_buf, 0, sizeof(g_frame_buffers[0]));
    for (int y = 0; y < ST7735_RAM_HEIGHT; y++) {
        lcd_write_data((const uint8_t *)zero_buf, sizeof(g_frame_buffers[0]));
    }

    // 초기 화면을 검은색으로 클리어
    lcd_fill_color(g_frame_buffers[0], COLOR_BLACK);
    lcd_draw_frame_buffer(g_frame_buffers[0]);
}
