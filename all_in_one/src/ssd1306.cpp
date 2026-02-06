#include <string.h>
#include <stdio.h>
#include "hardware/i2c.h"
#include "cli.h"
#include "ssd1306.h"



static uint8_t g_addr = 0x3C; // SSD1306 I2C 주소 (일반적으로 0x3C 또는 0x3D)
static uint8_t g_width;
static uint8_t g_height;
static PMode g_powermode = ACTIVE;
static uint8_t g_fb[1 + 128 * 64 / 8];

void _WriteData(size_t length)
{
	g_fb[0] = 0x40; // 데이터 스트림 모드
	i2c_write_blocking(I2C_PORT, g_addr, g_fb, length + 1, false);
}

void _WriteCmd(uint8_t* cmds, size_t length)
{
	uint8_t buf[2];
	for(int i = 0; i < length; i++)
	{
		buf[0] = 0x00; // 명령어 스트림 모드
		buf[1] = cmds[i];
		i2c_write_blocking(I2C_PORT, g_addr, buf, 2, false);
	}
}

void ssd1306_Cmd(uint8_t argc, char* argv[])
{
	if(argc >= 3)
	{
		if(SAME_STR("cont", argv[1]))
		{
			uint8_t contrast = (uint8_t)CLI_GetInt(argv[2]);
			SSD1306_Contrast(contrast);
			printf("SSD1306 Contrast set to %u\n", contrast);
		}
		else if(SAME_STR("fill", argv[1]))
		{
			uint8_t* buf = SSD1306_GetFB();
			if(SAME_STR("inc", argv[2]))
			{
				for(int i = 0; i < g_width * g_height / 8; i++)
				{
					buf[i] = (uint8_t)i;
				}
				printf("SSD1306 Screen filled with incrementing pattern\n");
			}
			else
			{
				uint8_t pat = (uint8_t)CLI_GetInt(argv[2]);
				memset(buf, pat, g_width * g_height / 8);
				printf("SSD1306 Screen filled with 0x%02X\n", pat);
			}
			SSD1306_Update();
		}
		else if(SAME_STR("pwr", argv[1]))
		{
			uint8_t mode = (uint8_t)CLI_GetInt(argv[2]);
			if(mode > DEEP_SLEEP)
			{
				printf("Invalid power mode. Use 0:ACTIVE, 1:SLEEP, 2:DEEP_SLEEP\n");
				return;
			}
			SSD1306_Powermode((PMode)mode);
			printf("SSD1306 Power mode set to %u\n", mode);
		}
	}
	else if(argc == 2)
	{
		if(SAME_STR("clear", argv[1]))
		{
			uint8_t* buf = SSD1306_GetFB();
			memset(buf, CLEAR_COLOR, g_width * g_height / 8);
			SSD1306_Update();
			printf("SSD1306 Screen cleared\n");
		}
	}
}

void SSD1306_Update()
{
	uint8_t setup_cmds[] = {
		0x21, 0, g_width - 1,       // Column Address range (0-127)
		0x22, 0, (g_height / 8) - 1,          // Page Address range (0-7)
	};
	_WriteCmd(setup_cmds, sizeof(setup_cmds));
	_WriteData(g_width * g_height / 8);
}

void SSD1306_Contrast(uint8_t contrast)
{
	uint8_t cmd[] = {0x81, contrast};
	_WriteCmd(cmd, sizeof(cmd));
}

uint8_t* SSD1306_GetFB()
{
	return g_fb + 1;
}

void SSD1306_Powermode(PMode mode)
{
	uint8_t cmd[2];
	switch(mode)
	{
		case ACTIVE:
			cmd[0] = 0x8D;
			cmd[1] = 0x14; // Charge Pump ON
			_WriteCmd(cmd, 2);
			cmd[0] = 0xAF; // Display ON
			_WriteCmd(cmd, 1);
			break;

		case SLEEP:
			cmd[0] = 0x8D;
			cmd[1] = 0x14; // Charge Pump ON
			_WriteCmd(cmd, 2);
			cmd[0] = 0xAE; // Display OFF
			_WriteCmd(cmd, 1);
			break;

		case DEEP_SLEEP:
			cmd[0] = 0x8D;
			cmd[1] = 0x10; // Charge Pump OFF
			_WriteCmd(cmd, 2);
			cmd[0] = 0xAE; // Display OFF
			_WriteCmd(cmd, 1);
			break;

		default:
			return;
	}
}

const uint8_t g_init_cmds[] = {
	0xAE,             // Display OFF (안전을 위해 시작 시 OFF)
	0xD5, 0x80,       // Clock divide ratio
	0xA8, 0x1F,       // Multiplex Ratio를 31 (32-1)로 설정 ★중요
	0xD3, 0x00,       // No offset
	0x40,             // Start line 0
	0x8D, 0x14,       // Charge pump ON
	0x20, 0x00,       // Horizontal Addressing Mode
	0xA1,             // Segment re-map (좌우 반전 시 0xA0)
	0xC8,             // COM scan direction (상하 반전 시 0xC0)
	0xDA, 0x02,       // COM pins hardware config ★128x32는 0x02
	0x81, 0x8F,       // Contrast (0xCF는 너무 밝을 수 있어 0x8F 권장)
	0xD9, 0xF1,       // Pre-charge period
	0xDB, 0x40,       // VCOMH deselect level
	0xA4,             // Resume to RAM content
	0xA6,             // Normal display
	0xAF              // Display ON ★반드시 필요
};


void SSD1306_Init(uint8_t width, uint8_t height)
{
	g_width = width;
	g_height = height;

	_WriteCmd((uint8_t*)g_init_cmds, sizeof(g_init_cmds));

	uint8_t* buf = SSD1306_GetFB();
	memset(buf, 0xff, width * height / 8);
	SSD1306_Update();
	CLI_Register("ssd", ssd1306_Cmd);
}

