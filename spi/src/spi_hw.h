#pragma once

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

#define CS_PIN		(1)
#define SCK_PIN		(2)
#define MOSI_PIN	(3)
#define MISO_PIN	(4)

#define USE_PIO_FOR_FLASH	(1)

#if (1 == USE_PIO_FOR_FLASH)
struct my_spi_t {
	PIO pio;
	uint sm;
	uint32_t cs_pin;
};
#else
struct my_spi_t {
	spi_inst_t* spi;
	uint32_t cs_pin;
};
#endif

void spi_hw_init(my_spi_t* spi_hw);
void spi_hw_write_read(my_spi_t* spi_hw,
					   uint8_t* tx_buf, size_t tx_len,
					   uint8_t* rx_buf, size_t rx_len);
