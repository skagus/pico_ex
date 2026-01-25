#include "tusb.h"
#include "usb_desc.h"
#include "board.h"
#include "kbd.h"


// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
	// not implemented, we only send REPORT_ID_KEYBOARD
	(void)instance;
	(void)len;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
	// TODO not Implemented
	(void)instance;
	(void)report_id;
	(void)report_type;
	(void)buffer;
	(void)reqlen;

	return 0;
}



//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
	board_set_led_period(BLINK_MOUNTED);
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
	board_set_led_period(BLINK_NOT_MOUNTED);
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
	(void)remote_wakeup_en;
	board_set_led_period(BLINK_SUSPENDED);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
	board_set_led_period(BLINK_MOUNTED);
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

			if(buffer[0] & KEYBOARD_LED_SCROLLLOCK)
			{
				// Capslock On: disable blink, turn led on
				board_set_led_period(0);
				board_led_write(true);
			}
			else
			{
				// Caplocks Off: back to normal blink
				board_led_write(false);
				board_set_led_period(BLINK_MOUNTED);
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


