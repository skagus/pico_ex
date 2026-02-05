#include <stdio.h>
#include "pico/stdlib.h"
#include "sched.h"
#include "cli.h"
#include "led.h"
#include "cap_meter.h"
#include "ws2812.h"
#include "pwm.h"

Cbf g_CbfTick;
CliHandler g_CliHandle;

// 틱마다 호출될 콜백 함수
bool repeating_timer_callback(struct repeating_timer* t)
{
	//	printf(".");
	g_CbfTick(0, 0); // 스케줄러 틱 함수 호출
	return true; // 다음 틱을 위해 타이머를 계속 실행합니다.
}


int main()
{
	static struct repeating_timer timer;

	// 1. 기본 설정
	stdio_init_all();
	sleep_ms(1000);

	g_CbfTick = Sched_Init();

	if(!add_repeating_timer_ms(-(MS_PER_TICK), repeating_timer_callback, NULL, &timer)) {
		printf("Failed to add repeating timer\n");
		return 1;
	}
	g_CliHandle = CLI_Init();
	
	LED_Init();
	CAPMETER_Init();
	WS_Init();
	PWM_Init();

	while(true)
	{
		int c = getchar_timeout_us(0);
		if(c != PICO_ERROR_TIMEOUT) {
			g_CliHandle((char)c);
		}

		Sched_Run();
	}

	return 0;
}
