/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//#include "bsp/board.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "pico/stdio/driver.h"
#include "usb_desc.h"
#include "tud_if.h"

#define KEY_1_PIN			(4)
#define KEY_2_PIN			(5)
#define KEY_3_PIN			(6)
#define KEY_4_PIN			(7)

typedef struct _keymap_t
{
	uint8_t pin_no;
	uint8_t short_key;
	uint8_t long_key;
} keymap_t;

keymap_t keymap[] =
{
	{KEY_1_PIN, HID_KEY_A, 0},
	{KEY_2_PIN, HID_KEY_B, 0},
	{KEY_3_PIN, HID_KEY_C, 0},
	{KEY_4_PIN, HID_KEY_D, 0},
};

uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void hid_task(void);
void cdc_task(void);


#define PICO_DEFAULT_LED_PIN	(1)


void board_led_init(void)
{
	gpio_init(PICO_DEFAULT_LED_PIN); // Initialize the GPIO pin
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT); // Set direction to output

	for(uint8_t i = 0; i < sizeof(keymap)/sizeof(keymap_t); i++)
	{
		gpio_init(keymap[i].pin_no); // Initialize the GPIO pin
		gpio_set_dir(keymap[i].pin_no, GPIO_IN); // Set direction to input
		gpio_pull_up(keymap[i].pin_no); // Enable internal pull-up resistor
	}
}

void board_led_write(bool state)
{
	gpio_put(PICO_DEFAULT_LED_PIN, state ? 1 : 0);
}

uint32_t board_millis(void)
{
	return to_ms_since_boot(get_absolute_time());
}

bool scan(uint8_t key_codes[])
{
	static uint32_t start_ms = 0;
	const uint32_t interval_ms = 50;
	static uint32_t prv_but_state = 0x0; // all released

	if(board_millis() - start_ms < interval_ms)
	{
		return false; // not enough time
	}
	start_ms = board_millis();

	memset(key_codes, 0, 6); // clear key codes
	uint32_t cur_but = 0;
	for(int i= 0; i < sizeof(keymap)/sizeof(keymap_t); i++)
	{
		if( 0 == gpio_get(keymap[i].pin_no)) // active low
		{
			cur_but |= (1 << i);
		}
	}

	int key_count = 0;
	for(int i= 0; i < sizeof(keymap)/sizeof(keymap_t); i++)
	{
		if((cur_but & (1 << i)) && !(prv_but_state & (1 << i)))
		{
			// key pressed
			key_codes[key_count] = keymap[i].short_key;
			prv_but_state |= (1 << i);
			key_count++;
		}
	}

	prv_but_state = cur_but;
	// if blink is disabled, send key
	return key_count > 0;
}


// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
	(void)instance;

	if(report_type == HID_REPORT_TYPE_OUTPUT)
	{
		// Set keyboard LED e.g Capslock, Numlock etc...
		if(report_id == REPORT_ID_KEYBOARD)
		{
			// bufsize should be (at least) 1
			if(bufsize < 1)
				return;

			uint8_t const kbd_leds = buffer[0];

			if(kbd_leds & KEYBOARD_LED_SCROLLLOCK)
			{
				// Capslock On: disable blink, turn led on
				blink_interval_ms = 0;
				board_led_write(true);
			}
			else
			{
				// Caplocks Off: back to normal blink
				board_led_write(false);
				blink_interval_ms = BLINK_MOUNTED;
			}
		}
	}
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+


static void send_hid_report(bool keys_pressed, uint8_t key_codes[])
{
	// skip if hid is not ready yet
	if(!tud_hid_ready())
	{
		return;
	}

	// avoid sending multiple zero reports
	static bool send_empty = false;

	if(keys_pressed)
	{
		tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, key_codes);
		send_empty = true;
	}
	else
	{
		// send empty key report if previously has key pressed
		if(send_empty)
		{
			tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
		}
		send_empty = false;
	}
}

// Every 10ms, we poll the pins and send a report
void hid_task(void)
{
	// Poll every 10ms
	const uint32_t interval_ms = 10;
	static uint32_t start_ms = 0;
	static uint8_t key_codes[6] = {0};

	if(board_millis() - start_ms < interval_ms)
	{
		return; // not enough time
	}
	start_ms += interval_ms;

	// Check for keys pressed
	bool const keys_pressed = scan(key_codes);

	// Remote wakeup
	if(tud_suspended() && keys_pressed)
	{
		// Wake up host if we are in suspend mode
		// and REMOTE_WAKEUP feature is enabled by host
		tud_remote_wakeup();
	}
	else
	{
		// send a report
		send_hid_report(keys_pressed, key_codes);
	}
}


void handle_received_data(const uint8_t* data, uint32_t length)
{
	int idx = data[0] - '0';
	int value = atoi((const char*)&data[2]);

	if(idx < 0 || idx >= (int)(sizeof(keymap)/sizeof(keymap_t)))
	{
		printf("Invalid key index: %d\n", idx);
		return;
	}

	keymap[idx].short_key = (uint8_t)value;
	printf("Key %d mapped to HID code: %d\n", idx, value);
}

#define BUF_SIZE 64
void cdc_task(void)
{
	static int len = 0;
	static char buf[BUF_SIZE];

	if(tud_cdc_connected())
	{
		// PC에서 데이터를 받았을 때
		if(tud_cdc_available())
		{
			uint32_t count = tud_cdc_read(buf + len, BUF_SIZE - len);
			len += count;
			if(buf[len - 1] == '\n' || len >= (BUF_SIZE - 1))
			{
				buf[len] = '\0'; // null-terminate the string
				handle_received_data((const uint8_t*)buf, len);
				len = 0; // 버퍼 초기화
			}
		}
	}
}

// 1. stdio 출력을 담당할 함수 (printf -> CDC)
void stdio_usb_out_chars(const char* buf, int length)
{
	if(tud_cdc_connected())
	{
		for(int i = 0; i < length; i++)
		{
			tud_cdc_write_char(buf[i]);
		}
		tud_cdc_write_flush();
	}
}

// 2. stdio 입력을 담당할 함수 (CDC -> scanf/getchar)
int stdio_usb_in_chars(char* buf, int length)
{
	if(tud_cdc_connected() && tud_cdc_available())
	{
		return (int)tud_cdc_read(buf, (uint32_t)length);
	}
	return PICO_ERROR_NO_DATA;
}

// 3. stdio 드라이버 구조체 정의
stdio_driver_t stdio_usb_custom = {
	.out_chars = stdio_usb_out_chars,
	.in_chars = stdio_usb_in_chars,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
	.crlf_enabled = true
#endif
};

void stdio_usb_init(void)
{
	stdio_set_driver_enabled(&stdio_usb_custom, true);
}
//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
	static uint32_t start_ms = 0;
	static bool led_state = false;

	// blink is disabled
	if(!blink_interval_ms)
		return;

	// Blink every interval ms
	if(board_millis() - start_ms < blink_interval_ms)
		return; // not enough time
	start_ms += blink_interval_ms;

	board_led_write(led_state);
	led_state = 1 - led_state; // toggle
}


/*------------- MAIN -------------*/
int main(void)
{
	board_led_init();
	//	board_init();
	tusb_init();
	
	stdio_set_driver_enabled(&stdio_usb_custom, true);

	while(1)
	{
		tud_task(); // tinyusb device task

		led_blinking_task();
		hid_task(); // keyboard implementation
		cdc_task();
	}

	return 0;
}