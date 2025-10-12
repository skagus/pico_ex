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

int fw_pwm(int pin, int period_ms, float duty_cycle, int cnt)
{
	int on_time = (int)(period_ms * duty_cycle);
	int off_time = period_ms - on_time;
	while(cnt--)
	{
		gpio_put(pin, 1);
		sleep_ms(on_time);
		gpio_put(pin, 0);
		sleep_ms(off_time);
	}
	return 0;
}

int hw_test()
{
	/// \tag::setup_pwm[]

	// Tell GPIO 0 and 1 they are allocated to the PWM
	gpio_set_function(LED_PIN, GPIO_FUNC_PWM);

	// Find out which PWM slice is connected to GPIO 0 (it's slice 0)
	uint slice_num = pwm_gpio_to_slice_num(LED_PIN);
	uint chan = pwm_gpio_to_channel(LED_PIN);


	// Set period of 4 cycles (0 to 3 inclusive)
	pwm_set_wrap(slice_num, 10000);
	// Set channel A output high for one cycle before dropping
	pwm_set_chan_level(slice_num, chan, 3000);
	// Set the PWM running
	pwm_set_enabled(slice_num, true);
	/// \end::setup_pwm[]
	sleep_ms(10);
	pwm_set_enabled(slice_num, false);
	sleep_ms(20);

	// Note we could also use pwm_set_gpio_level(gpio, x) which looks up the
	// correct slice and channel for a given GPIO.
}

int hw_pwm(int pin, int period_ms, int cnt)
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

	for(int i = 0; i < cnt; i++)
	{
		float duty_cycle = (float)i / cnt;
		// 듀티 사이클 설정
		int level = (int)(wrap * duty_cycle);
		printf("Level: %u\n", level);
		pwm_set_chan_level(slice_num, chan, level);
		sleep_ms(period_ms);
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
		hw_pwm(LED_PIN, 10, 10);  // 10ms period, 0~90% 듀티 사이클
		//hw_test();
		sleep_ms(2000);
	}
	return 0;
}

