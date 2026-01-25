#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "tusb.h"
#include "board.h"
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


#define STR_CMP(a, b)	(strncmp((const char*)a, (char*)b, strlen((const char*)a)) == 0)

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
	uint8_t short_key[NUM_BUTTONS];
	uint8_t long_key[NUM_BUTTONS];
} mapgroup_t;


const static uint8_t g_but_list[NUM_BUTTONS] = {KEY_1_PIN, KEY_2_PIN, KEY_3_PIN, KEY_4_PIN};

static mapgroup_t ga_groups[NUM_MAPSETS] =
{
	{
		.name = "Default",
		.short_key = {HID_KEY_A, HID_KEY_B, HID_KEY_C, HID_KEY_D},
		.long_key = {HID_KEY_V, HID_KEY_W, HID_KEY_X, HID_KEY_Y}
	},
	{
		.name = "Numpad",
		.short_key = {HID_KEY_KEYPAD_1, HID_KEY_KEYPAD_2, HID_KEY_KEYPAD_3, HID_KEY_KEYPAD_4},
		.long_key = {HID_KEY_KEYPAD_7, HID_KEY_KEYPAD_8, HID_KEY_KEYPAD_9, HID_KEY_KEYPAD_0}
	}
};

static mapgroup_t* gp_active_group = &ga_groups[0];
static key_status_t key_status[NUM_BUTTONS] = {0};

bool kbd_scan(uint8_t key_codes[])
{
	static uint32_t start_ms = 0;
	const uint32_t interval_ms = 50;

	if(board_millis() - start_ms < interval_ms)
	{
		return false; // not enough time
	}
	start_ms = board_millis();

	memset(key_codes, 0, 6); // clear key codes

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
				key_codes[0] = gp_active_group->short_key[i];
				status->state = RELEASED;
				return true;
			}
			else if(board_millis() - status->pressed_time >= LONG_PRESS_TIMEOUT_MS)
			{
				// long press
				key_codes[0] = gp_active_group->long_key[i];
				status->state = LONG_PRESSED;
				return true;
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
		printf("Activated Group %d\n", grp);
	}
	else if(STR_CMP("list", data))
	{
		printf("Available Mapping Groups:\n");
		for(int i = 0; i < NUM_MAPSETS; i++)
		{
			printf("Group %d: %s\n", i, ga_groups[i].name);
			printf("  Short Keys: ");
			for(int j = 0; j < NUM_BUTTONS; j++)
			{
				printf("%d ", ga_groups[i].short_key[j]);
			}
			tud_task();
			printf("\n");
			printf("  Long Keys:  ");
			for(int j = 0; j < NUM_BUTTONS; j++)
			{
				printf("%d ", ga_groups[i].long_key[j]);
			}
			printf("\n");
		}
	}
	else
	{
		printf("set <grp> [s|l] <idx> <val> : Set [short|long] key mapping at index <idx> group <grp> to HID code <val>\n");
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

