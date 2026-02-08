#pragma once
#include <stdint.h>

#define ADDR_INA226 0x40

// INA226 레지스터 주소
#define REG_CONFIG	0x00
#define REG_SHUNTV	0x01
#define REG_BUSV	0x02
#define REG_CURRENT	0x04
#define REG_CALIB	0x05

void INA226_Init();
