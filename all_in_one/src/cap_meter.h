#pragma once
#include <stdint.h>

#define CHARGE_PIN		(14)
#define DISCHARGE_PIN 	(15)
#define ADC_PIN 		(26) // ADC0 핀
#define ADC_NUM 		(0)  // ADC 채널 0	

#define CAP_PERIOD		SCHED_MSEC(1000)

void CAPMETER_Init(void);


