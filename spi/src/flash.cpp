
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "spi_hw.h"
#include "flash.h"


void flash_unprot(my_spi_t* spi_hw, uint32_t addr)
{
	flash_wren(spi_hw); // Write Enable
	uint8_t aCmd[4] = {0x39, }; // Global Unprotect
	aCmd[1] = (uint8_t)(addr >> 16);
	aCmd[2] = (uint8_t)(addr >> 8);
	aCmd[3] = (uint8_t)(addr);
	spi_hw_write_read(spi_hw, aCmd, 4, nullptr, 0);
}

void flash_init(my_spi_t* spi_hw)
{
	spi_hw_init(spi_hw);
}

void flash_wren(my_spi_t* spi_hw)
{
	uint8_t cmd = 0x06; // Write Enable
	spi_hw_write_read(spi_hw, &cmd, 1, nullptr, 0);
}

void flash_wait_wip(my_spi_t* spi_hw)
{
	while(true)
	{
		uint8_t status[2];
		uint8_t cmd = 0x05; // Read Status Register
		spi_hw_write_read(spi_hw, &cmd, 1, status, 2);
		if((status[1] & 0x01) == 0) break; // WIP 비트가 클리어 될 때까지 대기.
		sleep_us(100);
	}
}

void flash_pgm(my_spi_t* spi_hw, uint32_t addr, uint32_t size, uint8_t key)
{
	uint8_t tx_buf[5 + 256]; // Command + Address + Data
	memset(tx_buf, 0xFF, sizeof(tx_buf));
	if(size > 256) size = 256;
	tx_buf[0] = 0x02; // Page Program
	tx_buf[1] = (uint8_t)(addr >> 16);
	tx_buf[2] = (uint8_t)(addr >> 8);
	tx_buf[3] = (uint8_t)(addr);
	uint32_t* int_buf = (uint32_t*)(tx_buf + 4);
	for(int i = 0; i < size / 4; i++)
	{
		int_buf[i] = (addr << 8) | key;
		addr++;
	}
	flash_unprot(spi_hw, addr);
	
	flash_wren(spi_hw); // Write Enable
	spi_hw_write_read(spi_hw, tx_buf, 4 + size, nullptr, 0);
	flash_wait_wip(spi_hw);
}

void flash_read(my_spi_t* spi_hw, uint32_t addr, uint32_t size, uint8_t* buf)
{
	printf("Read Data: 0x%6X, %d\n", addr, size);

	uint8_t tx_buf[4] = {0x03, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)(addr)};
	if(size >= 256) size = 256;
	memset(buf, 0, size);

	spi_hw_write_read(spi_hw, tx_buf, 4, buf, size); // Data
}

void _erase(my_spi_t* spi_hw, uint8_t code, uint32_t addr)
{
	printf("Erase: %02X, %06X\n", code, addr);
	flash_unprot(spi_hw, addr);
	uint8_t tx_buf[4] = {code, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)(addr)};
	flash_wren(spi_hw); // Write Enable
	spi_hw_write_read(spi_hw, tx_buf, 4, nullptr, 0);
	flash_wait_wip(spi_hw);
}

void flash_erase(my_spi_t* spi_hw, uint32_t addr, uint32_t size)
{
	printf("Erase Data: 0x%6X, %d\n", addr, size);

	uint32_t un_aligned_addr = addr & (4096 - 1);
	if(un_aligned_addr > 0)
	{
		uint32_t align_size = 4096 - un_aligned_addr;
		addr += align_size;
		size -= align_size;
	}

	while(size >= 4 * 1024)
	{
		_erase(spi_hw, 0x20, addr); // Block Erase (64KB)
		addr += 4 * 1024;
		size -= 4 * 1024;
	}
}

void flash_read_sect_protect(my_spi_t* spi_hw, uint32_t size, uint8_t* a_state)
{
	int bit_idx = 0;
	uint8_t ret=0;
	memset(a_state, 0, size / (8 * 64 * 1024));
	for(int addr = 0; addr < size; addr += 64 * 1024)
	{
		uint8_t aCmd[4] = {0x3C, }; // Sector Protect Status
		aCmd[1] = (uint8_t)(addr >> 16);
		aCmd[2] = (uint8_t)(addr >> 8);
		aCmd[3] = (uint8_t)(addr);
		spi_hw_write_read(spi_hw, aCmd, 4, &ret, 1);
		if(ret == 0xFF)
		{
			a_state[bit_idx / 8] |= (1 << (bit_idx % 8));
		}
		else
		{
			a_state[bit_idx / 8] &= ~(1 << (bit_idx % 8));
		}
		bit_idx++;
	}
}


//////////////////////////////////////////////////////////