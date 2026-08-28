#ifndef ST7735S_H
#define ST7735S_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hardware/spi.h"

// ---------------------------------------------------------------------------
// Pin Definitions (project.txt 기준)
// ---------------------------------------------------------------------------
#define LCD_SPI_PORT    spi0
#define LCD_SPI_FREQ    (21 * 1000 * 1000)  // 32 MHz (실제 31.25 MHz로 분주)

#define PIN_CS          1   // GP1 - Chip Select (SW 제어)
#define PIN_SCK         2   // GP2 - SPI Clock
#define PIN_MOSI        3   // GP3 - SPI TX (SDA)
#define PIN_DC          4   // GP4 - Data/Command
#define PIN_RES         5   // GP5 - Hardware Reset
#define PIN_BL          6   // GP6 - Backlight

// ---------------------------------------------------------------------------
// LCD Resolution & Offsets (128x128 1.44" 패널 기준, 180도 회전 적용)
// ---------------------------------------------------------------------------
#define LCD_WIDTH       128
#define LCD_HEIGHT      128
#define LCD_PIXELS      (LCD_WIDTH * LCD_HEIGHT)
#define LCD_X_OFFSET    2   // 128x128 모듈 X 오프셋 (180도 회전 시: 2)
#define LCD_Y_OFFSET    2   // 128x128 모듈 Y 오프셋 (180도 회전 시: 2)

// ST7735S 컨트롤러 전체 GDDRAM 크기
#define ST7735_RAM_WIDTH   132
#define ST7735_RAM_HEIGHT  162

// ---------------------------------------------------------------------------
// ST7735S Command Definitions
// ---------------------------------------------------------------------------
#define ST7735_NOP      0x00
#define ST7735_SWRESET  0x01    // Software Reset
#define ST7735_SLPIN    0x10    // Sleep In
#define ST7735_SLPOUT   0x11    // Sleep Out
#define ST7735_PTLON    0x12    // Partial Mode ON
#define ST7735_NORON    0x13    // Normal Display Mode ON
#define ST7735_INVOFF   0x20    // Display Inversion OFF
#define ST7735_INVON    0x21    // Display Inversion ON
#define ST7735_DISPOFF  0x28    // Display OFF
#define ST7735_DISPON   0x29    // Display ON
#define ST7735_CASET    0x2A    // Column Address Set
#define ST7735_RASET    0x2B    // Row Address Set
#define ST7735_RAMWR    0x2C    // Memory Write
#define ST7735_MADCTL   0x36    // Memory Data Access Control
#define ST7735_COLMOD   0x3A    // Interface Pixel Format
#define ST7735_FRMCTR1  0xB1    // Frame Rate Control (Normal)
#define ST7735_FRMCTR2  0xB2    // Frame Rate Control (Idle)
#define ST7735_FRMCTR3  0xB3    // Frame Rate Control (Partial)
#define ST7735_INVCTR   0xB4    // Display Inversion Control
#define ST7735_PWCTR1   0xC0    // Power Control 1
#define ST7735_PWCTR2   0xC1    // Power Control 2
#define ST7735_PWCTR3   0xC2    // Power Control 3
#define ST7735_PWCTR4   0xC3    // Power Control 4
#define ST7735_PWCTR5   0xC4    // Power Control 5
#define ST7735_VMCTR1   0xC5    // VCOM Control 1
#define ST7735_GMCTRP1  0xE0    // Positive Gamma Correction
#define ST7735_GMCTRN1  0xE1    // Negative Gamma Correction

// ---------------------------------------------------------------------------
// MADCTL Bits
// ---------------------------------------------------------------------------
#define MADCTL_MY       0x80    // Row Address Order
#define MADCTL_MX       0x40    // Column Address Order
#define MADCTL_MV       0x20    // Row/Column Exchange
#define MADCTL_ML       0x10    // Vertical Refresh Order
#define MADCTL_RGB      0x00    // RGB Color Order
#define MADCTL_BGR      0x08    // BGR Color Order

// ---------------------------------------------------------------------------
// Common RGB565 Colors
// ---------------------------------------------------------------------------
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_YELLOW    0xFFE0
#define COLOR_ORANGE    0xFD20
#define COLOR_GRAY      0x8410
#define COLOR_DARKGRAY  0x39E7
#define COLOR_LIGHTGRAY 0xC618
#define COLOR_NAVY      0x000F
#define COLOR_DARKBLUE  0x0010
#define COLOR_DARKGREEN 0x03E0
#define COLOR_PURPLE    0x780F

// ---------------------------------------------------------------------------
// Double Frame Buffer (128x128 x 2 버퍼, RGB565)
// ---------------------------------------------------------------------------
extern uint16_t g_frame_buffers[2][LCD_PIXELS];

// ---------------------------------------------------------------------------
// Function Prototypes
// ---------------------------------------------------------------------------

/**
 * @brief LCD 초기화 (SPI, DMA, GPIO, LCD 초기화 시퀀스 포함)
 */
void lcd_init(void);

/**
 * @brief LCD 전체 화면을 지정 색상으로 채움
 * @param color RGB565 포맷 색상값
 */
void lcd_fill_color(uint16_t color);

/**
 * @brief 지정된 프레임 버퍼를 16비트 SPI DMA로 전송 (Fire & Forget)
 * @param fb 16비트 픽셀 버퍼 포인터 (LCD_PIXELS 크기)
 */
void lcd_draw_frame_buffer(const uint16_t *fb);

/**
 * @brief 진행 중인 DMA/SPI 전송 대기
 */
void lcd_wait_idle(void);

/**
 * @brief DMA 전송 진행 여부 확인
 */
bool lcd_is_busy(void);

/**
 * @brief LCD에 커맨드 1바이트 전송 (동기 블로킹)
 * @param cmd 커맨드 바이트
 */
void lcd_write_cmd(uint8_t cmd);

/**
 * @brief LCD에 8비트 데이터 전송 (블로킹)
 * @param data 데이터 버퍼 포인터
 * @param len  데이터 길이 (바이트)
 */
void lcd_write_data(const uint8_t *data, size_t len);

#endif // ST7735S_H
