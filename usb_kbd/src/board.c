#include "pico/stdio/driver.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include "board.h"

#define LED_PIN	(1)

uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void board_set_led_period(uint32_t interval_ms)
{
	blink_interval_ms = interval_ms;
}

void board_init(void)
{
	gpio_init(LED_PIN); // Initialize the GPIO pin
	gpio_set_dir(LED_PIN, GPIO_OUT); // Set direction to output
}

void board_led_write(bool state)
{
	gpio_put(LED_PIN, state ? 1 : 0);
}

uint32_t board_millis(void)
{
	return to_ms_since_boot(get_absolute_time());
}


//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void board_task(void)
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
