#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "st7735s.h"
#include "spi_man.h"

// ---------------------------------------------------------------------------
// Internal Variables & Helpers
// ---------------------------------------------------------------------------

// 프레임 버퍼 (ST7735S: 더블 버퍼 64KB, ST7789: 싱글 버퍼 153.6KB)
uint16_t g_frame_buffers[LCD_NUM_BUFFERS][LCD_PIXELS];

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
    busy_wait_us(1);
}

static void lcd_cmd_post_exec(spi_hw_t *hw) {
    (void)hw;
    lcd_cs_deselect();
    busy_wait_us(1);
}

static void lcd_data_pre_exec(spi_hw_t *hw) {
    (void)hw;
    spi_set_format(LCD_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    lcd_dc_data();
    lcd_cs_select();
    busy_wait_us(1);
}

static void lcd_data_post_exec(spi_hw_t *hw) {
    (void)hw;
    lcd_cs_deselect();
    busy_wait_us(1);
}

static void lcd_fb_pre_exec(spi_hw_t *hw) {
    (void)hw;
    spi_set_format(LCD_SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    lcd_dc_data();
    lcd_cs_select();
    busy_wait_us(1);
}

// LCD 비동기 프레임버퍼 DMA 전송 완료 플래그 (post_exec에 의해 클리어)
static volatile bool s_lcd_busy = false;

static void lcd_fb_post_exec(spi_hw_t *hw) {
    (void)hw;
    lcd_cs_deselect();
    spi_set_format(LCD_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    s_lcd_busy = false;
    busy_wait_us(1);
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
    volatile bool done = false;

    spi_req_t *req = spi_alloc_req();
    req->inline_buf[0] = cmd;
    req->data = req->inline_buf;
    req->len = 1;
    req->pre_exec = lcd_cmd_pre_exec;
    req->post_exec = lcd_cmd_post_exec;
    req->p_done = &done;

    spi_push_req(req);

    // post_exec 완료 대기
    while (!done) {
        tight_loop_contents();
    }
}

static void lcd_write_data(const uint8_t *data, size_t len) {
    if (len == 0) return;
    volatile bool done = false;

    spi_req_t *req = spi_alloc_req();
    if (len <= sizeof(req->inline_buf)) {
        memcpy(req->inline_buf, data, len);
        req->data = req->inline_buf;
    } else {
        req->data = (uint8_t *)data;
    }
    req->len = len;
    req->pre_exec = lcd_data_pre_exec;
    req->post_exec = lcd_data_post_exec;
    req->p_done = &done;

    spi_push_req(req);

    // post_exec 완료 대기
    while (!done) {
        tight_loop_contents();
    }
}

static void lcd_write_data_byte(uint8_t data) {
    volatile bool done = false;

    spi_req_t *req = spi_alloc_req();
    req->inline_buf[0] = data;
    req->data = req->inline_buf;
    req->len = 1;
    req->pre_exec = lcd_data_pre_exec;
    req->post_exec = lcd_data_post_exec;
    req->p_done = &done;

    spi_push_req(req);

    // post_exec 완료 대기
    while (!done) {
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
    uint8_t col_data[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    lcd_write_data(col_data, 4);

    lcd_write_cmd(ST7735_RASET);
    uint8_t row_data[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    lcd_write_data(row_data, 4);

    lcd_write_cmd(ST7735_RAMWR);
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    lcd_set_window_raw(x0 + LCD_X_OFFSET, y0 + LCD_Y_OFFSET,
                       x1 + LCD_X_OFFSET, y1 + LCD_Y_OFFSET);
}

void lcd_draw_frame_buffer(const uint16_t *fb) {
    lcd_wait_idle();
    s_lcd_busy = true;

    // 윈도우 설정 (0,0)~(WIDTH-1, HEIGHT-1) 및 RAMWR 커맨드 전송
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

#if defined(USE_LCD_ST7789)
// ---------------------------------------------------------------------------
// ST7789 (2.4" 240x320 세로 모드) 초기화 시퀀스
// ---------------------------------------------------------------------------
static void lcd_init_st7789(void) {
    // Software Reset
    lcd_write_cmd(ST7735_SWRESET);
    sleep_ms(150);

    // Sleep Out
    lcd_write_cmd(ST7735_SLPOUT);
    sleep_ms(150);

    // Interface Pixel Format: 16-bit (RGB565)
    lcd_write_cmd(ST7735_COLMOD);
    lcd_write_data_byte(0x55);

    // Memory Data Access Control (세로 모드 240x320: RGB Color Order)
    lcd_write_cmd(ST7735_MADCTL);
    lcd_write_data_byte(0x00);

    // Porch Setting
    lcd_write_cmd(0xB2);
    {
        uint8_t porch[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
        for (size_t i = 0; i < sizeof(porch); i++) lcd_write_data_byte(porch[i]);
    }

    // Gate Control
    lcd_write_cmd(0xB7);
    lcd_write_data_byte(0x35);

    // VCOM Setting
    lcd_write_cmd(0xBB);
    lcd_write_data_byte(0x19);

    // LCM Control
    lcd_write_cmd(0xC0);
    lcd_write_data_byte(0x2C);

    // VDV and VRH Command Enable
    lcd_write_cmd(0xC2);
    lcd_write_data_byte(0x01);

    // VRH Set
    lcd_write_cmd(0xC3);
    lcd_write_data_byte(0x12);

    // VDV Set
    lcd_write_cmd(0xC4);
    lcd_write_data_byte(0x20);

    // Frame Rate Control in Normal Mode (60Hz)
    lcd_write_cmd(0xC6);
    lcd_write_data_byte(0x0F);

    // Power Control 1
    lcd_write_cmd(0xD0);
    lcd_write_data_byte(0xA4);
    lcd_write_data_byte(0xA1);

    // Positive Voltage Gamma Control
    lcd_write_cmd(ST7735_GMCTRP1);
    {
        uint8_t pv_gamma[] = {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
        for (size_t i = 0; i < sizeof(pv_gamma); i++) lcd_write_data_byte(pv_gamma[i]);
    }

    // Negative Voltage Gamma Control
    lcd_write_cmd(ST7735_GMCTRN1);
    {
        uint8_t nv_gamma[] = {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};
        for (size_t i = 0; i < sizeof(nv_gamma); i++) lcd_write_data_byte(nv_gamma[i]);
    }

    // Display Inversion OFF (표준 TFT 모드: Black=0x0000, White=0xFFFF)
    lcd_write_cmd(ST7735_INVOFF);
    sleep_ms(10);

    // Normal Display Mode ON
    lcd_write_cmd(ST7735_NORON);
    sleep_ms(10);

    // Display ON
    lcd_write_cmd(ST7735_DISPON);
    sleep_ms(100);
}

#else
// ---------------------------------------------------------------------------
// ST7735S (1.44" 128x128 180도 회전 모드) 초기화 시퀀스
// ---------------------------------------------------------------------------
static void lcd_init_st7735s(void) {
    // Software Reset
    lcd_write_cmd(ST7735_SWRESET);
    sleep_ms(150);

    // Sleep Out
    lcd_write_cmd(ST7735_SLPOUT);
    sleep_ms(150);

    // Frame Rate Control (Normal mode)
    lcd_write_cmd(ST7735_FRMCTR1);
    lcd_write_data_byte(0x01);
    lcd_write_data_byte(0x2C);
    lcd_write_data_byte(0x2D);

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
        for (size_t i = 0; i < sizeof(gamma_p); i++) lcd_write_data_byte(gamma_p[i]);
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
        for (size_t i = 0; i < sizeof(gamma_n); i++) lcd_write_data_byte(gamma_n[i]);
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
}
#endif

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
    // 5. 선택된 LCD 모델별 초기화 시퀀스
    // -----------------------------------------------------------------------
#if defined(USE_LCD_ST7789)
    lcd_init_st7789();
#else
    lcd_init_st7735s();
#endif

    // 초기 화면을 검은색으로 클리어
    lcd_fill_color(g_frame_buffers[0], COLOR_BLACK);
    lcd_draw_frame_buffer(g_frame_buffers[0]);
}
