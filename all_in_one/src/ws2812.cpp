
#include <stdio.h>
#include <math.h>
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "ws2812.pio.h"
#include "cli.h"
#include "ws2812.h"

static PIO pio;
static int g_sm;
const float pi = 3.14159f;

// DMA Configurations
#define STREAM_LEN				(256)
#define NUM_STREAM				(4)
#define TOTAL_STREAM_LEN		(STREAM_LEN * NUM_STREAM)
#define MAX_BRIGHTNESS 			(0x40) 	// 0x00 - 0xff
#define MAKE_RGB(r, g, b) (uint32_t)((((((g) << 8) | (r)) << 8) | (b)) << 8)	

static uint32_t pixel_values[TOTAL_STREAM_LEN]  __attribute__((aligned(4096)));

void _setPixels()
{
	uint32_t* col_val = pixel_values;
	for(int c = 0; c < NUM_STREAM; c++) {
		for(int i = 0; i < STREAM_LEN; i++) {
			float sinf_val = sinf(pi * (float)i / 256.0f);
			uint8_t val = (uint8_t)(sinf_val * (float)MAX_BRIGHTNESS);
			uint32_t tmp;
			switch(c) {
				case 0:
				tmp = MAKE_RGB(val, 0x0, 0x0);
				break;
				case 1:
				tmp = MAKE_RGB(0x0, val, 0x0);
				break;
				case 2:
				tmp = MAKE_RGB(0x0, 0x0, val);
				break;
				case 3:
				tmp = MAKE_RGB(val / 3, val / 3, val / 3);
				break;
			}
			*col_val = tmp;
			col_val++;
		}
	}
}

void ws_Cmd(uint8_t argc, char* argv[])
{
	if(argc > 1)
	{
		uint32_t color = CLI_GetInt(argv[1]);
		if(color != NOT_NUMBER)
		{
			WS_SetColor(color);
		}
		else
		{
			printf("ws <24b:GRB>\n");
		}
	}
	else // DMA Demo.
	{
		int dma_timer = dma_claim_unused_timer(true);
		// --- DMA 타이머 설정 ---
		// 10ms (100Hz) 주기로 DREQ를 발생시키도록 설정합니다.
		// 시스템 클럭이 125MHz일 때, 125,000,000 / 1,250,000 = 100Hz
	//	dma_timer_set_fraction(dma_timer, 1, 12500);  // 12.5K / 125M = 1/10K = 0.1ms
		dma_timer_set_fraction(dma_timer, 1, 65535);  // 65.5K / 125M = 1/10K = 0.524ms

		// DMA 채널 설정 (아직 시작은 안 함)
		int dma_chan = dma_claim_unused_channel(true);
		dma_channel_config c = dma_channel_get_default_config(dma_chan);
		channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
		channel_config_set_read_increment(&c, true);
		channel_config_set_write_increment(&c, false);
		channel_config_set_dreq(&c, dma_get_timer_dreq(dma_timer)); // 타이머 1의 DREQ 사용

		// DMA 채널을 링(ring) 모드로 설정하여 버퍼의 끝에 도달하면 다시 처음으로 돌아가게 합니다.
		// 이렇게 하면 전송이 계속 반복됩니다.
		channel_config_set_ring(&c, false, 12);

		dma_channel_configure(dma_chan, &c,
							  &pio->txf[g_sm],      // 목적지: PIO FIFO
							  pixel_values,               // 소스: 나중에 설정 (전송 시작 시)
							  TOTAL_STREAM_LEN * 10,         // 전송 횟수 (32비트 단위)
							  true);               // 시작
	}
}

void WS_SetColor(uint32_t color)
{
	pio->txf[g_sm] = color << 8; // MAKE_RGB(color, 0x0, 0x0);
}

void WS_Init(void)
{
	pio = pio0;
	g_sm = 0;
	uint offset = pio_add_program(pio, &ws2812_program);
	ws2812_pio_init(pio, g_sm, offset, PICO_WS2812_PIN, 800000);

	_setPixels();
	CLI_Register("ws", ws_Cmd);
}
