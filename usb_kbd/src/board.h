#pragma once

#include <stdint.h>
#include <stdbool.h>


// Blink pattern
enum
{
	BLINK_NOT_MOUNTED = 250, // device not mounted
	BLINK_MOUNTED = 1000,    // device mounted
	BLINK_SUSPENDED = 2500,  // device is suspended
};

uint32_t board_millis(void);
void board_led_write(bool state);
void board_set_led_period(uint32_t interval_ms);
void board_init(void);
void board_task(void);
