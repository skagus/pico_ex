#pragma once

#include <stdint.h>

#define PICO_WS2812_PIN			(15)

void WS_Init(void);
void WS_SetColor(uint32_t interval_ms);
