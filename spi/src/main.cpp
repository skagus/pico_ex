#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "spi.pio.h"	// PIO 어셈블리 파일로부터 자동 생성된 헤더
#include "spi_hw.h"
#include "flash.h"

// PIO 출력을 사용할 핀 정의 (Pico 온보드 LED)
#define CS_PIN		(1)
#define SCK_PIN		(2)
#define MOSI_PIN	(3)
#define MISO_PIN	(4)

void dump(const uint8_t* buf, size_t len)
{
	for(size_t i=0; i<len; i++)
	{
		printf("%02X ", buf[i]);
		if((i&0x0F) == 0x0F) printf("\n");
	}
	if(len & 0x0F) printf("\n");
}

#define CMP_CMD(cmd_str, input)	(strncmp(cmd_str, input, strlen(cmd_str))==0)

int str_token(char* tokens[], char* str, int max_tokens)
{
	int i = 0;
	while(*str)
	{
		while(isspace(*str))  // Remove whitespace.
		{
			*str = '\0';
			str++;
		}
		if(i >= max_tokens) break;
		if(*str == '\0') break;
		tokens[i] = str;
		i++;
		// Continue until space or end.
		while(*str && *str != ' ') str++;
	}
	return i;
}

void spi_peri()
{
	my_spi_t real_spi_hw;
	my_spi_t* spi_hw = &real_spi_hw;
	flash_init(spi_hw);

	// Main loop.
	uint32_t addr = 0;
	uint32_t size = 16;
	uint8_t key = 0xFF;
	uint8_t buf[256];
	char line_buf[32];
#define MAX_TOKENS	(5)
	char* tokens[MAX_TOKENS];
	while(true)
	{
		gets(line_buf);
		printf("You entered: %s\n", line_buf);
		int num_tokens = str_token(tokens, line_buf, MAX_TOKENS);
		if(num_tokens < 1) continue;

		if(CMP_CMD("pgm", tokens[0]))
		{
			addr = strtol(tokens[1] + 2, NULL, 16);
			size = strtol(tokens[2], NULL, 10);
			key = (num_tokens >= 4) ? strtol(tokens[3] + 2, NULL, 16) : 0xFF;
			printf("Program Data: 0x%6X, %d\n", addr, size);
			flash_pgm(spi_hw, addr, size, key); // Read Data 명령
		}
		else if(CMP_CMD("read", tokens[0]))
		{
			addr = strtol(tokens[1] + 2, NULL, 16);
			size = strtol(tokens[2], NULL, 10);
			flash_read(spi_hw, addr, size, buf); // Read Data 명령
			dump(buf, size);
		}
		else if(CMP_CMD("erase", tokens[0]))
		{
			addr = strtol(tokens[1] + 2, NULL, 16);
			size = strtol(tokens[2], NULL, 10);
			flash_erase(spi_hw, addr, size); // Sector Erase 명령
		}
		else if(CMP_CMD("id", tokens[0]))
		{
			uint8_t aCmd[4] = {0x00, 0x00, 0x00, 0x00}; // JEDEC ID 읽기 명령
			
			printf("JEDEC ID\n");
			aCmd[0] = 0x9F; // JEDEC ID 읽기 명령
			spi_hw_write_read(spi_hw, aCmd, 1, buf, 4);
			dump(buf, 4);

			printf("AT25 Manu/Dev ID\n");
			aCmd[0] = 0x9F; // Manufacturer/Device ID 읽기 명령
			spi_hw_write_read(spi_hw, aCmd, 1, buf, 4);
			dump(buf, 4);

			printf("AT25 OTP Security Addr0\n");
			aCmd[0] = 0x77; // Unique ID 읽기 명령 cmd 1, Addr 3, dummy 2, data 1+
			spi_hw_write_read(spi_hw, aCmd, 6, buf, 8);
			dump(buf, 8);
		}
		else if(CMP_CMD("rsta", tokens[0]))
		{
			uint8_t cmd;
			printf("Read Status-1\n");
			cmd = 0x05; // Read Status Register
			spi_hw_write_read(spi_hw, &cmd, 1, buf, 1);
			dump(buf, 1);

			printf("Read Status-2\n");
			cmd = 0x35; // Read Status Register
			spi_hw_write_read(spi_hw, &cmd, 1, buf, 1);
			dump(buf, 1);
		}
		else if(CMP_CMD("wsta", tokens[0]))
		{
			uint8_t status = strtol(tokens[1] + 2, NULL, 16);
			uint8_t tx_buf[2] = {0x01, status}; // Write Status Register command
			flash_wren(spi_hw); // Must enable writes first
			spi_hw_write_read(spi_hw, tx_buf, 2, nullptr, 0);
			flash_wait_wip(spi_hw); // Wait for the write to complete
		}
		else if(CMP_CMD("prot", tokens[0]))
		{
			printf("Read Sector Protect Status\n");
			const uint32_t size = 4 * 1024 * 1024; // 4MB
			// 4MB / 64KB = 64 sectors
			int buf_size = size / (8 * 64 * 1024);
			flash_read_sect_protect(spi_hw, size, buf);
			dump(buf, buf_size);
		}
		else
		{
			printf("Unknown command\n\n");
			printf("pgm 0xADDR SIZE [0xKEY]\nread 0xADDR SIZE\nerase 0xADDR SIZE\n");
			printf("id\nstatus\nwsta 0xSTATUS\n");
		}
		sleep_ms(1000);
	}
}

#if 0
void pio_push(PIO pio, uint sm, uint8_t data)
{
	while(pio_sm_is_tx_fifo_full(pio, sm));
	pio->txf[sm] = (uint32_t)data;
}


int set_peri_rx_dma(uint8_t* peri_addr, uint8_t* buf_addr, uint32_t dreq, size_t cnt_xfer)
{
	int dma_chan = dma_claim_unused_channel(true);
	dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

	// 4. DMA 상세 설정
	// - 전송 단위: 8비트
	// - 읽기 주소: 고정 (항상 같은 PIO RX FIFO 주소에서 읽음)
	// - 쓰기 주소: 전송 후 자동으로 증가 (버퍼에 순차적으로 씀)
	// - DREQ: PIO의 RX DREQ 신호에 맞춰 전송 트리거
	channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
	channel_config_set_read_increment(&cfg, false);
	channel_config_set_write_increment(&cfg, true);
	channel_config_set_dreq(&cfg, dreq); // RX DREQ (false)

	// 5. DMA 전송 시작
	printf("Starting DMA capture...\n");
	dma_channel_configure(
		dma_chan,
		&cfg,
		buf_addr,      // 쓰기 주소: 데이터를 저장할 버퍼
		peri_addr,     // 읽기 주소: PIO State Machine의 RX FIFO
		cnt_xfer,         // 전송 횟수
		true               // 즉시 시작
	);
	return dma_chan;
}


int set_peri_tx_dma(uint8_t* buf_addr, uint8_t* peri_addr, uint32_t dreq, size_t cnt_xfer)
{
	int dma_chan = dma_claim_unused_channel(true);
	dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

	// 4. DMA 상세 설정
	// - 전송 단위: 8비트
	// - 읽기 주소: 고정 (항상 같은 PIO RX FIFO 주소에서 읽음)
	// - 쓰기 주소: 전송 후 자동으로 증가 (버퍼에 순차적으로 씀)
	// - DREQ: PIO의 RX DREQ 신호에 맞춰 전송 트리거
	channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
	channel_config_set_read_increment(&cfg, true);
	channel_config_set_write_increment(&cfg, false);
	channel_config_set_dreq(&cfg, dreq); //

	// 5. DMA 전송 시작
	dma_channel_configure(
		dma_chan,
		&cfg,
		peri_addr,      // 쓰기 주소: 데이터를 저장할 버퍼
		buf_addr,     // 읽기 주소: PIO State Machine의 RX FIFO
		cnt_xfer,         // 전송 횟수
		true               // 즉시 시작
	);
	return dma_chan;
}

void wait_dma_done(int dma_chan)
{
	// 6. DMA 전송 완료 대기
	dma_channel_wait_for_finish_blocking(dma_chan);
	dma_channel_unclaim(dma_chan);
}

void spi_pio()
{
	// 1. Init SPI pins
	gpio_init(CS_PIN);
	gpio_put(CS_PIN, 0);	// CS High
	gpio_set_dir(CS_PIN, GPIO_OUT);
	gpio_set_drive_strength(CS_PIN, GPIO_DRIVE_STRENGTH_12MA);

	gpio_init(MISO_PIN);
	gpio_set_dir(MISO_PIN, GPIO_IN);
	gpio_set_function(MISO_PIN, GPIO_FUNC_PIO0);

	// 2. Setup PIO
	PIO pio = pio0;
	int sm = 0;
	uint offset = pio_add_program(pio, &spi_program);

	spi_program_init(pio, sm, offset, CS_PIN, MOSI_PIN, MISO_PIN); // , 2 * 1000 * 1000); // 1MHz

#if 0	
	uint8_t tx_buf[5] = {0x00, 0xF1, 0x03, 0x07, 0xFF}; // JEDEC ID
	tx_buf[0] = sizeof(tx_buf) - 2;

//	pio_sm_put_blocking(pio, sm, 4<<16 | 8); // x --> tx byte count.
	for(int i = 0; i < sizeof(tx_buf); i++)
	{
		pio_sm_put_blocking(pio, sm, 0xFFFFFFFF); // 0xAABBCC00 | tx_buf[i]);
	}


#elif 0
	uint32_t tx_cnt;
	uint32_t rx_cnt;

	uint8_t tx_buf[4] = {0x03, 0x00, 0x01, 0x00};
	tx_cnt = 4;
	rx_cnt = 16;
	pio_sm_put_blocking(pio, sm, (tx_cnt - 1) << 16 | (rx_cnt - 1)); // x --> tx byte count.
	for(int i = 0; i < tx_cnt; i++)
	{
		pio_push(pio, sm, tx_buf[i]);
		//		pio_sm_put_blocking(pio, sm, *(uint32_t*)bytes); // x --> tx byte count.
	}
	for(int i = 0; i < rx_cnt; i++)
	{
		uint32_t nRead = pio_sm_get_blocking(pio, sm); // Read data.
		printf("Read Data[%d]: %02X\n", i, nRead & 0xFF);
	}
#else
	uint8_t addr_buf[3] = {0x00, 0x01, 0x00}; // Address.
	uint8_t rx_buf[16];

	uint32_t tx_cnt = sizeof(addr_buf) + 1; // 명령 + 주소(1B)
	uint32_t rx_cnt = sizeof(rx_buf);
	
	pio_sm_put_blocking(pio, sm, (tx_cnt-1) << 16 | (rx_cnt-1)); // High 16: TX count, Low 16: RX count.
	pio_sm_put_blocking(pio, sm, 0x03); // x --> tx byte count.

	uint32_t rx_dreq = pio_get_dreq(pio, sm, false);
	int rx_dma_ch = set_peri_rx_dma((uint8_t*)&pio->rxf[sm], rx_buf, rx_dreq, sizeof(rx_buf));
	uint32_t tx_dreq = pio_get_dreq(pio, sm, true);
	int tx_dma_ch = set_peri_tx_dma(addr_buf, (uint8_t*)&pio->txf[sm], tx_dreq, sizeof(addr_buf));
	// DMA 완료 대기
	wait_dma_done(tx_dma_ch);
	wait_dma_done(rx_dma_ch);

	printf("DMA Read Data:\n");
	dump(rx_buf, rx_cnt);
#endif
//	uint32_t nRead = pio_sm_get_blocking(pio, sm); // Read data.
//	printf("Read Data: %08X\n", nRead);
	printf("Done\n");
}
#endif

int main() {
	// 1. 기본 설정
	stdio_init_all();
	sleep_ms(1000);

	uint32_t freq = clock_get_hz(clk_sys);
	printf("\n\nSys clock: %u Hz, %u\n", freq, SYS_CLK_MHZ);

	spi_peri();
//	spi_pio();

	return 0;
}