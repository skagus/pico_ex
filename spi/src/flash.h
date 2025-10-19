#pragma once

#include "spi_hw.h"

#define USE_PIO_FOR_FLASH	(0)

void flash_init(my_spi_t* spi_hw);
void flash_pgm(my_spi_t* spi_hw, uint32_t addr, uint32_t size, uint8_t key);
void flash_read(my_spi_t* spi_hw, uint32_t addr, uint32_t size, uint8_t* buf);
void flash_erase(my_spi_t* spi_hw, uint32_t addr, uint32_t size);
void flash_wait_wip(my_spi_t* spi_hw);
void flash_wren(my_spi_t* spi_hw);
