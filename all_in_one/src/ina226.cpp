#include <stdio.h>
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "cli.h"
#include "ina226.h"



uint8_t g_addr;
// 16비트 데이터를 빅 엔디언으로 쓰고 읽기 위한 함수
void ina226_write_reg(uint8_t reg, uint16_t value)
{
	uint8_t buf[3];
	buf[0] = reg;
	buf[1] = (value >> 8) & 0xFF; // MSB
	buf[2] = value & 0xFF;        // LSB
	int ret = i2c_write_blocking(I2C_PORT, g_addr, buf, 3, false);
	if(ret < 0) printf("i2c write reg failed:[%X %X] -> %d\n", reg, value, ret);
}

uint16_t ina226_read_reg(uint8_t reg)
{
	uint8_t buf[2];
	int ret = i2c_write_blocking(I2C_PORT, g_addr, &reg, 1, true); // 레지스터 선택
	if(ret < 0) printf("i2c write failed: %X -> %d\n", reg, ret);
	i2c_read_blocking(I2C_PORT, g_addr, buf, 2, false);  // 데이터 읽기
	return (buf[0] << 8) | buf[1]; // 바이트 병합
}

void ina226_Cmd(uint8_t argc, char* argv[])
{
	uint16_t busv_raw = ina226_read_reg(REG_BUSV);
	float busv_V = busv_raw * 1.25f * 1e-3f; // 1.25mV/LSB

	uint16_t sntv_raw = ina226_read_reg(REG_SHUNTV);
	float sntv_mV = sntv_raw * 2.5e-3f; // 2.5uV/LSB

	int16_t curr_raw = (int16_t)ina226_read_reg(REG_CURRENT);
	float curr_mA = curr_raw * 1.0f; // 1mA/LSB

	printf("BUS      %5u, %6.2f V\n", busv_raw, busv_V);
	printf("SHUNT    %5u, %6.2f mV\n", sntv_raw, sntv_mV);
	printf("Current  %5d, %6.2f mA\n", curr_raw, curr_mA);
}

void _i2c_init()
{
	// I2C 초기화 (400kHz)
	i2c_init(I2C_PORT, 40 * 1000);

	gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(SDA_PIN);
	gpio_pull_up(SCL_PIN);
}

void INA226_Init()
{
	g_addr = ADDR_INA226;
	_i2c_init();
	
	// INA226 설정: 평균 16회 측정, 컨버전 타임 1.1ms
	// [11:9] AVG=16 (b010), 
	// [8:6] VBUS CT=1.1ms, (b100)
	// [5:3] VSH CT=1.1ms, (b100)
	// [2:0] MODE=Shunt and Bus, Continuous (b111)
	ina226_write_reg(REG_CONFIG, 0x4527);
	// Calibration 값 설정 (0.1ohm 션트, Cur LSB=1.0mA)
	ina226_write_reg(REG_CALIB, 51);

	CLI_Register("ina", ina226_Cmd);
}
