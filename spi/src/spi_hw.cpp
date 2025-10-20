#include <stdio.h>
#include <stdint.h>
#include "spi_hw.h"
#include "spi.pio.h"	// PIO 어셈블리 파일로부터 자동 생성된 헤더


#if (1 == USE_PIO_FOR_FLASH)

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

#endif


void spi_hw_init(my_spi_t* spi_hw)
{
#if (1 == USE_PIO_FOR_FLASH)
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
	spi_hw->pio = pio;
	spi_hw->sm = sm;
	spi_hw->cs_pin = CS_PIN;
#else
	spi_hw->spi = spi0;
	spi_hw->cs_pin = CS_PIN;

	gpio_init(spi_hw->cs_pin);
	gpio_set_dir(spi_hw->cs_pin, GPIO_OUT);
	gpio_put(spi_hw->cs_pin, 1); // CS High

	gpio_init(SCK_PIN);
	gpio_set_dir(SCK_PIN, GPIO_OUT);
	gpio_set_function(SCK_PIN, GPIO_FUNC_SPI);
	gpio_put(SCK_PIN, 0);	// SCK Low

	gpio_init(MOSI_PIN);
	gpio_set_dir(MOSI_PIN, GPIO_OUT);
	gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
	gpio_put(MOSI_PIN, 0);	// MOSI Low

	gpio_init(MISO_PIN);
	gpio_set_dir(MISO_PIN, GPIO_IN);
	gpio_set_function(MISO_PIN, GPIO_FUNC_SPI);

	spi_init(spi_hw->spi, 2 * 1000 * 1000); // 10MHz
	spi_set_format(spi_hw->spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	spi_set_slave(spi_hw->spi, false); // Master mode
#endif
}

void spi_hw_write_read(my_spi_t* spi_hw,
	uint8_t* tx_buf, size_t tx_len, 
	uint8_t* rx_buf, size_t rx_len)
{
#if (1 == USE_PIO_FOR_FLASH)

	pio_sm_put_blocking(spi_hw->pio, spi_hw->sm, (tx_len << 16) | (rx_len)); // x --> tx byte count.

	uint32_t rx_dreq = pio_get_dreq(spi_hw->pio, spi_hw->sm, false);
	uint8_t* rx_fifo = (uint8_t *)&(spi_hw->pio->rxf[spi_hw->sm]);
	int rx_dma_ch = set_peri_rx_dma(rx_fifo, rx_buf, rx_dreq, rx_len);

	uint8_t* tx_fifo = (uint8_t*)&(spi_hw->pio->txf[spi_hw->sm]);
	uint32_t tx_dreq = pio_get_dreq(spi_hw->pio, spi_hw->sm, true);
	int tx_dma_ch = set_peri_tx_dma(tx_buf, tx_fifo, tx_dreq, tx_len);
	
	// DMA 완료 대기
	wait_dma_done(tx_dma_ch);
	wait_dma_done(rx_dma_ch);

#else
	gpio_put(spi_hw->cs_pin, 0); // CS Low
	if(tx_len != 0)
	{
		spi_write_blocking(spi_hw->spi, tx_buf, tx_len);
	}
	if(rx_len != 0)
	{
		spi_read_blocking(spi_hw->spi, 0x00, rx_buf, rx_len);
	}
	gpio_put(spi_hw->cs_pin, 1); // CS High
#endif
}
