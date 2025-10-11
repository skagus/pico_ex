#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
// PIO 어셈블리 파일로부터 자동 생성된 헤더
#include "blink.pio.h" 

// PIO 출력을 사용할 핀 정의 (Pico 온보드 LED)
#define LED_PIN (2) 

int main() {
	// 1. 기본 설정
	stdio_init_all();
	sleep_ms(1000);

	uint32_t freq = clock_get_hz(clk_sys);
	printf("Blink start, sys clock: %u Hz, %u\n", freq, SYS_CLK_MHZ);

	// 2. PIO 프로그램 로드 및 상태 머신 설정
	PIO pio = pio0;
	uint sm = pio_claim_unused_sm(pio, true);
	uint offset = pio_add_program(pio, &blink_program);

	blink_program_init(pio, sm, offset, LED_PIN);

	// 6. 데이터 전송 루프 (Blink 속도 변경)
	uint32_t delay_counts[] = {
		12,  // 7
		6,   // 1
	};
	int num_delays = sizeof(delay_counts) / sizeof(delay_counts[0]);
	int delay_duration_ms = 2000; // 각 속도를 2초간 유지

	for(int i = 0; ; i = (i + 1) % num_delays) {
		uint32_t current_delay = delay_counts[i];

		printf("PIO delay count: %lu. Wait for %d ms.\n", current_delay, delay_duration_ms);

		// TX FIFO가 가득 차면 대기했다가 값을 넣습니다.
		// 이 값이 PIO 프로그램의 Y 레지스터로 PULL됩니다.
		pio_sm_put_blocking(pio, sm, current_delay - 5);
		// 새로운 듀티 사이클로 잠시 실행되도록 대기합니다.
		sleep_ms(delay_duration_ms);
	}

	return 0;
}