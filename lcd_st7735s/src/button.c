#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "button.h"

static const uint8_t btn_pins[BTN_COUNT] = {
    BTN_PIN_UP,
    BTN_PIN_DOWN,
    BTN_PIN_CANCEL,
    BTN_PIN_OK
};

typedef struct {
    bool raw_state;        // true: pressed (active-low 0), false: released (1)
    bool debounced_state;  // 안정화된 상태
    bool prev_state;       // 이전 프레임의 debounced 상태
    bool clicked_flag;     // 단발 클릭 플래그
    uint32_t last_debounce_time_ms;
    uint32_t press_start_time_ms;
    uint32_t last_repeat_time_ms;
} button_state_t;

static button_state_t btn_states[BTN_COUNT];

#define DEBOUNCE_DELAY_MS   30
#define REPEAT_DELAY_MS     400
#define REPEAT_RATE_MS      150

void button_init(void) {
    for (int i = 0; i < BTN_COUNT; i++) {
        uint pin = btn_pins[i];
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin); // 내부 풀업 저항 활성화 (눌렀을 때 0V / LOW)
        
        btn_states[i].raw_state = false;
        btn_states[i].debounced_state = false;
        btn_states[i].prev_state = false;
        btn_states[i].clicked_flag = false;
        btn_states[i].last_debounce_time_ms = 0;
        btn_states[i].press_start_time_ms = 0;
        btn_states[i].last_repeat_time_ms = 0;
    }
}

void button_update(void) {
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    for (int i = 0; i < BTN_COUNT; i++) {
        uint pin = btn_pins[i];
        // Active-low: gpio_get() == 1 이면 눌림(true)
        bool current_raw = (gpio_get(pin) == 1);

        if (current_raw != btn_states[i].raw_state) {
            btn_states[i].raw_state = current_raw;
            btn_states[i].last_debounce_time_ms = now_ms;
        }

        if ((now_ms - btn_states[i].last_debounce_time_ms) >= DEBOUNCE_DELAY_MS) {
            if (current_raw != btn_states[i].debounced_state) {
                btn_states[i].debounced_state = current_raw;

                // 뗌 -> 눌림 엣지 (Falling edge on pin / Rising edge of pressed state)
                if (btn_states[i].debounced_state) {
                    btn_states[i].clicked_flag = true;
                    btn_states[i].press_start_time_ms = now_ms;
                    btn_states[i].last_repeat_time_ms = now_ms;
                }
            } else if (btn_states[i].debounced_state) {
                // 길게 누를 때 자동 반복 (UP/DOWN 스크롤 편의성)
                if ((i == BTN_UP || i == BTN_DOWN) &&
                    (now_ms - btn_states[i].press_start_time_ms) >= REPEAT_DELAY_MS) {
                    if ((now_ms - btn_states[i].last_repeat_time_ms) >= REPEAT_RATE_MS) {
                        btn_states[i].clicked_flag = true;
                        btn_states[i].last_repeat_time_ms = now_ms;
                    }
                }
            }
        }
    }
}

bool button_was_pressed(button_id_t btn) {
    if (btn >= BTN_COUNT) return false;
    if (btn_states[btn].clicked_flag) {
        btn_states[btn].clicked_flag = false;
        return true;
    }
    return false;
}

bool button_is_down(button_id_t btn) {
    if (btn >= BTN_COUNT) return false;
    return btn_states[btn].debounced_state;
}
