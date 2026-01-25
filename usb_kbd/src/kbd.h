#pragma once

#include <stdint.h>

#define LONG_PRESS_TIMEOUT_MS	500

#define KEY_1_PIN			(4)
#define KEY_2_PIN			(5)
#define KEY_3_PIN			(6)
#define KEY_4_PIN			(7)

#define NUM_BUTTONS		4
#define NUM_MAPSETS		2

void kbd_proc_cmd(const uint8_t* data, uint32_t length);
bool kbd_scan(uint8_t key_codes[]);
void kbd_init(void);