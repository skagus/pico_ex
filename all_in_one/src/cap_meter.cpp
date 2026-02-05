#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "sched.h"
#include "cli.h"
#include "cap_meter.h"

#define RESISTOR_VALUE (20000.0f) // 20k 옴 저항
#define TARGET_ADC_VALUE (2588) // 12bits(4K) * 0.632.

static bool g_bCapMeterRunning = false;

inline void discharge()
{
	gpio_put(DISCHARGE_PIN, 0);             // 방전 핀을 GND로
	gpio_set_dir(DISCHARGE_PIN, GPIO_OUT);

	// 완전히 방전될 때까지 대기 (용량이 크면 더 길게)
	while(adc_read() > 10) {
		sleep_ms(10);
	}
	gpio_set_dir(DISCHARGE_PIN, GPIO_IN);   // 방전 중단
}

float measure_Capacitance()
{
	discharge();
	// 측정 시작
	gpio_put(CHARGE_PIN, 1);                // 충전 시작 
	uint64_t start_time = time_us_64();     // 시작 시간 기록
	gpio_set_dir(CHARGE_PIN, GPIO_OUT);

	// 타겟 전압(63.2%)에 도달할 때까지 대기
	while(adc_read() < TARGET_ADC_VALUE) {
		tight_loop_contents(); // 루프 최적화 방지
	}
	uint64_t end_time = time_us_64();
	gpio_set_dir(CHARGE_PIN, GPIO_IN);      // 충전 핀을 고임피던스로

	uint64_t duration = end_time - start_time;
	float capacitance_uf = ((float)duration / RESISTOR_VALUE); // C = t / R (결과는 Farads, 1e6을 곱해 uF으로 변환)

	return capacitance_uf;
}

void cap_Cmd(uint8_t argc, char* argv[])
{
	if(argc > 1)
	{
		if(0 == strcmp(argv[1], "start"))
		{
			g_bCapMeterRunning = true;
			printf("Capacitance meter started.\n");
			Sched_TrigSyncEvt(BIT(EVT_CAP));
		}
		else if(0 == strcmp(argv[1], "stop"))
		{
			g_bCapMeterRunning = false;
			printf("Capacitance meter stopped.\n");
		}
		else
		{
			printf("Usage: cap [start|stop]\n");
		}
	}
	else
	{
		float result = measure_Capacitance();
		if(result < 1.0f)
		{
			printf("Capacitance: %.3f nF\n", result * 1000.0f);
		}
		else
		{
			printf("Capacitance: %.3f uF\n", result);
		}
	}
}

void cap_Run(Evts bmEvt)
{
	if(!g_bCapMeterRunning)
	{
		Sched_Wait(BIT(EVT_CAP), 0);
		return;
	}

	float result = measure_Capacitance();
	if(result < 1.0f)
	{
		printf("Capacitance: %.3f nF\n", result * 1000.0f);
	}
	else
	{
		printf("Capacitance: %.3f uF\n", result);
	}
	Sched_Wait(0, CAP_PERIOD);
}

void CAPMETER_Init(void)
{
	gpio_init(CHARGE_PIN);
	gpio_set_dir(CHARGE_PIN, GPIO_IN);
	gpio_put(CHARGE_PIN, 0);

	gpio_init(DISCHARGE_PIN);
	gpio_disable_pulls(DISCHARGE_PIN);
	gpio_set_dir(DISCHARGE_PIN, GPIO_IN);
	gpio_put(DISCHARGE_PIN, 0);

	// ADC 초기화
	adc_init();
	adc_gpio_init(ADC_PIN);
	adc_select_input(ADC_NUM);

	CLI_Register("cap", cap_Cmd);
	Sched_Register(TID_CAP, cap_Run, (1 << MODE_NORMAL));
}


