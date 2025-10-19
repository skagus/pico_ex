#include "spi_hw.h"

void spi_hw_init(my_spi_t* spi_hw)
{
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
}

void spi_hw_write_read(my_spi_t* spi_hw,
	uint8_t* tx_buf, size_t tx_len, 
	uint8_t* rx_buf, size_t rx_len)
{
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
}
