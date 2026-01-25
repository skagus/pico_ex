#include "board.h"
#include "tusb.h"
#include "tud_if.h"
#include "usb_desc.h"
#include "kbd.h"

extern uint32_t blink_interval_ms;

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
	bool const keys_pressed = kbd_scan(key_codes);

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


