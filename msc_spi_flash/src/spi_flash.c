#include "spi_flash.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#define SPI_FLASH_SPI_INST       spi0
#define SPI_FLASH_BAUDRATE       (31250000) // 31.25 MHz

// SPI Flash 표준 명령어
#define CMD_WRITE_ENABLE         0x06
#define CMD_WRITE_DISABLE        0x04
#define CMD_READ_STATUS_1        0x05
#define CMD_WRITE_STATUS_1       0x01
#define CMD_READ_DATA            0x03
#define CMD_FAST_READ            0x0B
#define CMD_PAGE_PROGRAM         0x02
#define CMD_SECTOR_ERASE_4K      0x20
#define CMD_BLOCK_ERASE_32K      0x52
#define CMD_BLOCK_ERASE_64K      0xD8
#define CMD_CHIP_ERASE           0xC7
#define CMD_JEDEC_ID             0x9F

// Status Register 비트
#define STATUS_BUSY              (1 << 0)
#define STATUS_WEL               (1 << 1)

static spi_flash_info_t flash_info = {0};
static int dma_tx_chan = -1;
static int dma_rx_chan = -1;
static bool async_pgm_dma_active = false;

static inline void cs_select(void)
{
    gpio_put(SPI_FLASH_CS_PIN, 0);
    __asm volatile ("nop\n nop\n nop\n nop\n");
}

static inline void cs_deselect(void)
{
    __asm volatile ("nop\n nop\n nop\n nop\n");
    gpio_put(SPI_FLASH_CS_PIN, 1);
    __asm volatile ("nop\n nop\n nop\n nop\n");
}

static uint8_t spi_flash_read_status(void)
{
    uint8_t cmd = CMD_READ_STATUS_1;
    uint8_t status = 0;
    cs_select();
    spi_write_blocking(SPI_FLASH_SPI_INST, &cmd, 1);
    spi_read_blocking(SPI_FLASH_SPI_INST, 0x00, &status, 1);
    cs_deselect();
    return status;
}

bool spi_flash_is_busy(void)
{
    return (spi_flash_read_status() & STATUS_BUSY) != 0;
}

void spi_flash_wait_busy(void)
{
    while (spi_flash_is_busy())
    {
        tight_loop_contents();
    }
}

static void spi_flash_write_enable(void)
{
    uint8_t cmd = CMD_WRITE_ENABLE;
    cs_select();
    spi_write_blocking(SPI_FLASH_SPI_INST, &cmd, 1);
    cs_deselect();
}

static void spi_flash_global_unprotect(void)
{
    spi_flash_write_enable();
    uint8_t cmd[2] = { CMD_WRITE_STATUS_1, 0x00 };
    cs_select();
    spi_write_blocking(SPI_FLASH_SPI_INST, cmd, sizeof(cmd));
    cs_deselect();
    spi_flash_wait_busy();
}

static const char* parse_manufacturer(uint8_t manuf_id)
{
    switch (manuf_id)
    {
        case 0x1F: return "Adesto/Atmel";
        case 0x9D: return "ISSI";
        case 0xEF: return "Winbond";
        case 0xC8: return "GigaDevice";
        case 0xC2: return "Macronix";
        case 0x20: return "Micron/ST";
        default:   return "Unknown";
    }
}

bool spi_flash_init(void)
{
    // 1. GPIO 및 SPI 초기화
    gpio_init(SPI_FLASH_CS_PIN);
    gpio_set_dir(SPI_FLASH_CS_PIN, GPIO_OUT);
    gpio_put(SPI_FLASH_CS_PIN, 1);

    gpio_set_function(SPI_FLASH_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_FLASH_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_FLASH_MISO_PIN, GPIO_FUNC_SPI);

    spi_init(SPI_FLASH_SPI_INST, SPI_FLASH_BAUDRATE);
    spi_set_format(SPI_FLASH_SPI_INST, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // 2. DMA 채널 할당
    if (dma_tx_chan < 0) {
        dma_tx_chan = dma_claim_unused_channel(true);
    }
    if (dma_rx_chan < 0) {
        dma_rx_chan = dma_claim_unused_channel(true);
    }

    // 3. JEDEC ID 읽기
    uint8_t cmd = CMD_JEDEC_ID;
    uint8_t id[3] = {0};
    cs_select();
    spi_write_blocking(SPI_FLASH_SPI_INST, &cmd, 1);
    spi_read_blocking(SPI_FLASH_SPI_INST, 0x00, id, 3);
    cs_deselect();

    flash_info.manuf_id = id[0];
    flash_info.mem_type = id[1];
    flash_info.capacity_id = id[2];
    flash_info.model_name = parse_manufacturer(id[0]);

    if (id[2] >= 0x10 && id[2] <= 0x24)
    {
        flash_info.total_size_bytes = (1u << id[2]);
    }
    else
    {
        // 25DF321 기본 4MB (32Mbit)
        flash_info.total_size_bytes = 4 * 1024 * 1024;
    }

    flash_info.total_blocks_512 = flash_info.total_size_bytes / 512;
    flash_info.initialized = true;

    // 4. 섹터 보호 해제
    spi_flash_global_unprotect();

    printf("[SPI_FLASH] JEDEC ID: %02X %02X %02X (Manuf: %s, Size: %lu KB, Blocks: %lu)\n",
           flash_info.manuf_id, flash_info.mem_type, flash_info.capacity_id,
           flash_info.model_name, (unsigned long)(flash_info.total_size_bytes / 1024),
           (unsigned long)flash_info.total_blocks_512);

    return true;
}

const spi_flash_info_t* spi_flash_get_info(void)
{
    return &flash_info;
}

uint32_t spi_flash_get_capacity_bytes(void)
{
    return flash_info.total_size_bytes;
}

uint32_t spi_flash_get_block_count(void)
{
    return flash_info.total_blocks_512;
}

void spi_flash_read(uint32_t addr, uint8_t *buf, size_t len)
{
    if (len == 0 || buf == NULL) return;

    // 플래시가 쓰기 중이면 완료될 때까지 대기
    spi_flash_wait_busy();

    uint8_t cmd[4];
    cmd[0] = CMD_READ_DATA;
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)(addr >> 0);

    cs_select();
    spi_write_blocking(SPI_FLASH_SPI_INST, cmd, 4);

    static const uint8_t dummy_tx = 0xFF;

    dma_channel_config tx_config = dma_channel_get_default_config(dma_tx_chan);
    channel_config_set_transfer_data_size(&tx_config, DMA_SIZE_8);
    channel_config_set_dreq(&tx_config, spi_get_dreq(SPI_FLASH_SPI_INST, true));
    channel_config_set_read_increment(&tx_config, false);
    channel_config_set_write_increment(&tx_config, false);

    dma_channel_config rx_config = dma_channel_get_default_config(dma_rx_chan);
    channel_config_set_transfer_data_size(&rx_config, DMA_SIZE_8);
    channel_config_set_dreq(&rx_config, spi_get_dreq(SPI_FLASH_SPI_INST, false));
    channel_config_set_read_increment(&rx_config, false);
    channel_config_set_write_increment(&rx_config, true);

    dma_channel_configure(
        dma_rx_chan,
        &rx_config,
        buf,
        &spi_get_hw(SPI_FLASH_SPI_INST)->dr,
        len,
        false
    );

    dma_channel_configure(
        dma_tx_chan,
        &tx_config,
        &spi_get_hw(SPI_FLASH_SPI_INST)->dr,
        &dummy_tx,
        len,
        false
    );

    dma_channel_start(dma_rx_chan);
    dma_channel_start(dma_tx_chan);

    dma_channel_wait_for_finish_blocking(dma_rx_chan);
    dma_channel_wait_for_finish_blocking(dma_tx_chan);

    cs_deselect();
}

void spi_flash_sector_erase_4k_async(uint32_t addr)
{
    spi_flash_wait_busy();
    spi_flash_write_enable();

    uint8_t cmd[4];
    cmd[0] = CMD_SECTOR_ERASE_4K;
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)(addr >> 0);

    cs_select();
    spi_write_blocking(SPI_FLASH_SPI_INST, cmd, 4);
    cs_deselect();
    // Non-blocking: 바로 리턴
}

void spi_flash_sector_erase_4k(uint32_t addr)
{
    spi_flash_sector_erase_4k_async(addr);
    spi_flash_wait_busy();
}

void spi_flash_page_program_dma_start(uint32_t addr, const uint8_t *buf, size_t len)
{
    if (len == 0 || buf == NULL) return;

    spi_flash_wait_busy();
    spi_flash_write_enable();

    uint8_t cmd[4];
    cmd[0] = CMD_PAGE_PROGRAM;
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)(addr >> 0);

    cs_select();
    spi_write_blocking(SPI_FLASH_SPI_INST, cmd, 4);

    dma_channel_config tx_config = dma_channel_get_default_config(dma_tx_chan);
    channel_config_set_transfer_data_size(&tx_config, DMA_SIZE_8);
    channel_config_set_dreq(&tx_config, spi_get_dreq(SPI_FLASH_SPI_INST, true));
    channel_config_set_read_increment(&tx_config, true);
    channel_config_set_write_increment(&tx_config, false);

    dma_channel_configure(
        dma_tx_chan,
        &tx_config,
        &spi_get_hw(SPI_FLASH_SPI_INST)->dr,
        buf,
        len,
        true
    );

    async_pgm_dma_active = true;
}

bool spi_flash_page_program_dma_is_done(void)
{
    if (!async_pgm_dma_active) return true;

    if (dma_channel_is_busy(dma_tx_chan)) {
        return false;
    }

    // SPI FIFO 버스 완료 대기
    if (spi_is_busy(SPI_FLASH_SPI_INST)) {
        return false;
    }

    // FIFO 잔여 데이터 비우기 및 CS 해제
    while (spi_is_readable(SPI_FLASH_SPI_INST)) {
        (void)spi_get_hw(SPI_FLASH_SPI_INST)->dr;
    }

    cs_deselect();
    async_pgm_dma_active = false;
    return true;
}

void spi_flash_page_program(uint32_t addr, const uint8_t *buf, size_t len)
{
    spi_flash_page_program_dma_start(addr, buf, len);
    while (!spi_flash_page_program_dma_is_done()) {
        tight_loop_contents();
    }
    spi_flash_wait_busy();
}

void spi_flash_write(uint32_t addr, const uint8_t *buf, size_t len)
{
    while (len > 0)
    {
        uint32_t page_offset = addr % SPI_FLASH_PAGE_SIZE;
        size_t chunk_len = SPI_FLASH_PAGE_SIZE - page_offset;
        if (chunk_len > len) {
            chunk_len = len;
        }

        spi_flash_page_program(addr, buf, chunk_len);

        addr += chunk_len;
        buf += chunk_len;
        len -= chunk_len;
    }
}
