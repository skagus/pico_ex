#include <stdio.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "led.pio.h" 
#include "cli.h"
#include "led.h"

#define LED_PIN (1) 

#define PIO_INST pio0
uint g_sm;


void led_Cmd(uint8_t argc, char* argv[])
{
	if(argc > 1)
	{
		uint32_t interval = CLI_GetInt(argv[1]);
		if(interval != NOT_NUMBER)
		{
			LED_Blink(interval);
		}
		else
		{
			printf("led <interval_ms>\n");
		}
	}
	else
	{
		printf("led <interval_ms>\n");
	}
}

void LED_Blink(uint32_t interval_ms)
{
	pio_sm_put_blocking(PIO_INST, g_sm, interval_ms - 5);
}


void LED_Init(void)
{
	uint32_t freq = clock_get_hz(clk_sys);
	printf("Blink start, sys clock: %u Hz, %u\n", freq, SYS_CLK_MHZ);

	// 2. PIO 프로그램 로드 및 상태 머신 설정
	g_sm = pio_claim_unused_sm(PIO_INST, true);
	uint offset = pio_add_program(PIO_INST, &blink_program);
	led_pio_init(PIO_INST, g_sm, offset, LED_PIN);

	CLI_Register("led", led_Cmd);
}
