#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "tusb.h"
#include "board.h"
#include "usb_hid.h"
#include "kbd.h"

/**
 * Key state transition.
 * - R:eleased
 * - P:ressed
 * - L:ong Pressed
 *
 * R --(but pushed)--> P: nothing
 * P --(released)--> R: Short key send
 * P --(timeout)--> L: Long key send
 * L --(released)--> R: nothing
 */

#define STR_CMP(a, b)		(strncmp((const char*)a, (char*)b, strlen((const char*)a)) == 0)

#define KEY_DEF(mod, key)	(((mod) << 8) | key)

typedef enum {
	RELEASED = 0,
	PRESSED,
	LONG_PRESSED
} key_state;

typedef struct key_status_t
{
	key_state state;
	uint32_t pressed_time;
} key_status_t;

typedef struct
{
	char name[32];
	uint16_t short_key[NUM_BUTTONS];
	uint16_t long_key[NUM_BUTTONS];
} mapgroup_t;

#define BIT(n) (1U << (n))

#define LCTRL	BIT(0)	///< Left Control
#define LSHIFT	BIT(1)	///< Left Shift
#define LALT	BIT(2)	///< Left Alt
#define LGUI	BIT(3)	///< Left Window
#define RCTRL	BIT(4)	///< Right Control
#define RSHIFT	BIT(5)	///< Right Shift
#define RALT	BIT(6)	///< Right Alt
#define RGUI	BIT(7)	///< Right Window


const static uint8_t g_but_list[NUM_BUTTONS] = {KEY_1_PIN, KEY_2_PIN, KEY_3_PIN, KEY_4_PIN};

static mapgroup_t ga_groups[NUM_MAPSETS] =
{
	{
		.name = "Default",
		.short_key = {
			KEY_DEF(0, HID_KEY_A),
			KEY_DEF(0, HID_KEY_B),
			KEY_DEF(LCTRL, HID_KEY_C),
			KEY_DEF(0, HID_KEY_D),
		},
		.long_key = {
			KEY_DEF(LCTRL, HID_KEY_V),
			KEY_DEF(0, HID_KEY_W),
			KEY_DEF(0, HID_KEY_X),
			KEY_DEF(0, HID_KEY_Y),
		}
	},
	{
		.name = "Numpad",
		.short_key = {
			KEY_DEF(0, HID_KEY_KEYPAD_1),
			KEY_DEF(0, HID_KEY_KEYPAD_2),
			KEY_DEF(0, HID_KEY_KEYPAD_3),
			KEY_DEF(0, HID_KEY_KEYPAD_4)
		},
		.long_key = {
			KEY_DEF(0, HID_KEY_KEYPAD_7),
			KEY_DEF(0, HID_KEY_KEYPAD_8),
			KEY_DEF(0, HID_KEY_KEYPAD_9),
			KEY_DEF(0, HID_KEY_KEYPAD_0)
		}
	}
};

static mapgroup_t* gp_active_group = &ga_groups[0];
static key_status_t key_status[NUM_BUTTONS] = {0};

uint16_t kbd_scan()
{
	static uint32_t start_ms = 0;
	const uint32_t interval_ms = 50;

	if(board_millis() - start_ms < interval_ms)
	{
		return 0; // not enough time
	}
	start_ms = board_millis();

	uint32_t cur_but = 0;
	for(int i = 0; i < NUM_BUTTONS; i++)
	{
		key_status_t* status = key_status + i;
		bool is_pressed = (0 == gpio_get(g_but_list[i])); // active low

		switch(status->state)
		{
			case RELEASED:
			if(is_pressed)
			{
				// key pressed
				status->state = PRESSED;
				status->pressed_time = board_millis();
			}
			break;
			case PRESSED:
			if(!is_pressed)
			{
				// short press
				status->state = RELEASED;
				return gp_active_group->short_key[i];
			}
			else if(board_millis() - status->pressed_time >= LONG_PRESS_TIMEOUT_MS)
			{
				// long press
				status->state = LONG_PRESSED;
				return gp_active_group->long_key[i];
			}
			break;
			case LONG_PRESSED:
			if(!is_pressed)
			{
				status->state = RELEASED;
			}
			break;
			default:
			status->state = RELEASED;
			break;
		}
	}
	// if blink is disabled, send key
	return 0;
}

bool kbd_scan_hid(uint8_t key_codes[])
{
	uint16_t keycode16 = kbd_scan(&keycode16);
	if(keycode16 != 0)
	{
		memset(key_codes, 0, KEY_CODE_SIZE); // clear key codes
		key_codes[0] = (uint8_t)((keycode16 >> 8) & 0xFF); // modifier
		key_codes[1] = (uint8_t)(keycode16 & 0xFF); // key code
		return true;
	}
	return false;
}



void kbd_proc_cmd(const uint8_t* data, uint32_t length)
{
	if(STR_CMP("set", data) && (length >= 8))
	{
		//set <grp> [s|l] <idx> <val>
		int grp = atoi((const char*)&data[4]);
		bool is_short_key = (data[6] == 's');
		int idx = data[8] - '0';
		int value = atoi((const char*)&data[10]);

		if(data[6] != 's' && data[6] != 'l')
		{
			printf("Invalid key type. Use 's' for short key or 'l' for long key.\n");
			return;
		}

		if(idx < 0 || idx >= NUM_BUTTONS)
		{
			printf("Invalid key index: %d\n", idx);
			return;
		}

		if(is_short_key)
		{
			ga_groups[grp].short_key[idx] = (uint8_t)value;
			printf("Short key %d mapped to HID code: %d\n", idx, value);
		}
		else
		{
			ga_groups[grp].long_key[idx] = (uint8_t)value;
			printf("Long key %d mapped to HID code: %d\n", idx, value);
		}
	}
	else if(STR_CMP("act", data) && (length >= 5))
	{
		int grp = atoi((const char*)&data[4]);
		if(grp < 0 || grp >= NUM_MAPSETS)
		{
			printf("Invalid Group index: %d\n", grp);
			return;
		}
		gp_active_group = &ga_groups[grp];
		printf("Group Activating %d\n", grp);
	}
	else if(STR_CMP("list", data))
	{
		printf("Defined Mapping\n");
		tud_task();
		for(int i = 0; i < NUM_MAPSETS; i++)
		{
			printf("Group %d: %s\n", i, ga_groups[i].name);
			printf("  Short Keys: ");
			tud_task();
			for(int j = 0; j < NUM_BUTTONS; j++)
			{
				printf("%4X ", ga_groups[i].short_key[j]);
			}
			tud_task();
			printf("\n");
			printf("  Long Keys:  ");
			tud_task();
			for(int j = 0; j < NUM_BUTTONS; j++)
			{
				printf("%4X ", ga_groups[i].long_key[j]);
			}
			printf("\n");
		}
	}
	else
	{
		printf("set <grp> [s|l] <idx> <val> : Set [short|long] key mapping ");
		tud_task();
		printf("at index <idx> group <grp> to HID code <val>\n");
	}
}

void kbd_init(void)
{
	for(uint8_t i = 0; i < NUM_BUTTONS; i++)
	{
		gpio_init(g_but_list[i]); // Initialize the GPIO pin
		gpio_set_dir(g_but_list[i], GPIO_IN); // Set direction to input
		gpio_pull_up(g_but_list[i]); // Enable internal pull-up resistor
	}
}

