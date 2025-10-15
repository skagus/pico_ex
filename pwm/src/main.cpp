#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

// PIO 출력을 사용할 핀 정의 (Pico 온보드 LED)
#define LED_PIN		(28)

int fw_pwm(int pin, int time_ms)
{
	int time_us = time_ms * 1000;
	int period_us = 1000; // 10ms 주기
	int cycles = time_us / period_us; // 
	for(int i = 0; i < cycles; i++)
	{
		int on_time = period_us * i / cycles;
		gpio_put(pin, 1);
		sleep_us(on_time);
		gpio_put(pin, 0);
		sleep_us(period_us - on_time);
	}
	return 0;
}


int hw_pwm(int pin, int time_ms)
{
	// PWM 하드웨어 초기화
	gpio_set_function(pin, GPIO_FUNC_PWM);
	uint slice_num = pwm_gpio_to_slice_num(pin);
	uint chan = pwm_gpio_to_channel(pin);

	int freq = 10000; // 10KHz
	uint32_t clk_freq = clock_get_hz(clk_sys); // 시스템 클럭 주파수
	uint16_t wrap = clk_freq / freq - 1;

	pwm_set_wrap(slice_num, wrap);
	// Set channel A output high for one cycle before dropping
	pwm_set_chan_level(slice_num, chan, 0);
	// Set the PWM running
	pwm_set_enabled(slice_num, true);

	for(int i = 0; i < time_ms; i++)
	{
		float duty_cycle = (float)i / time_ms;
		// 듀티 사이클 설정
		int level = (int)(wrap * duty_cycle);
		printf("Level: %u\n", level);
		pwm_set_chan_level(slice_num, chan, level);
		sleep_ms(1);
		// PWM 시작
	}
	pwm_set_enabled(slice_num, false);

	// PWM 정지
	gpio_set_function(pin, GPIO_FUNC_SIO); // GPIO로 다시 설정
	gpio_put(pin, 0); // 핀을 LOW로 설정
	return 0;
}

int main() {
	// 1. 기본 설정
	stdio_init_all();
	sleep_ms(1000);

	int freq = clock_get_hz(clk_sys);
	printf("\n\nSys clock: %u Hz, %u\n", freq, SYS_CLK_MHZ);

	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);
	while(true)
	{
		hw_pwm(LED_PIN, 5000);  // 10ms period, 0~90% 듀티 사이클
		fw_pwm(LED_PIN, 5000);
		sleep_ms(2000);
	}
	return 0;
}

