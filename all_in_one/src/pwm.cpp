
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "cli.h"
#include "pwm.h"



void pwm_Cmd(uint8_t argc, char* argv[])
{
	if(argc > 1)
	{
		if(0 == strcmp(argv[1], "stop"))
		{
			gpio_set_function(PWM_PIN, GPIO_FUNC_SIO); // GPIO로 다시 설정
			gpio_put(PWM_PIN, 0); // 핀을 LOW로 설정
			gpio_set_dir(PWM_PIN, GPIO_IN);
			printf("PWM stopped\n");
			return;
		}
		uint32_t duty = CLI_GetInt(argv[1]);
		if(duty == NOT_NUMBER || duty > 100)
		{
			printf("Usage: pwm <duty_cycle_percent (0-100)>\n");
			return;
		}

		gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
		uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
		pwm_set_wrap(slice_num, 65535); // 최대 카운트 값 설정
		uint16_t level = (duty * 65535) / 100; // 0-65535 범위로 변환
		pwm_set_chan_level(slice_num, PWM_CHAN_B, level);
		pwm_set_enabled(slice_num, true);
		printf("PWM duty cycle set to %u%%\n", duty);
	}
	else
	{
		printf("Usage: pwm <duty_cycle_percent (0-100) | stop>\n");
	}
}

void PWM_Init(void)
{
	CLI_Register("pwm", pwm_Cmd);

}

