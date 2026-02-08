
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hw_cfg.h"


void _i2c_init()
{
	// I2C 초기화 (400kHz)
	i2c_init(I2C_PORT, 40 * 1000);

	gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(SDA_PIN);
	gpio_pull_up(SCL_PIN);
}

void hw_cfg()
{
	_i2c_init();
}
