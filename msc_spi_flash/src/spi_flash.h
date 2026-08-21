#ifndef _SPI_FLASH_H_
#define _SPI_FLASH_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// SPI 핀 매핑 (project.txt 명세)
#define SPI_FLASH_CS_PIN      1
#define SPI_FLASH_SCK_PIN     2
#define SPI_FLASH_MOSI_PIN    3
#define SPI_FLASH_MISO_PIN    4

#define SPI_FLASH_SECTOR_SIZE 4096
#define SPI_FLASH_PAGE_SIZE   256

// 플래시 정보 구조체
typedef struct {
    uint8_t  manuf_id;
    uint8_t  mem_type;
    uint8_t  capacity_id;
    uint32_t total_size_bytes;
    uint32_t total_blocks_512;
    const char *model_name;
    bool     initialized;
} spi_flash_info_t;

/**
 * @brief SPI Flash 및 DMA를 초기화하고 JEDEC ID를 판별합니다.
 * @return true 초기화 및 장치 감지 성공, false 실패
 */
bool spi_flash_init(void);

/**
 * @brief 감지된 SPI Flash 장치 정보를 반환합니다.
 */
const spi_flash_info_t* spi_flash_get_info(void);

/**
 * @brief 전체 플래시 용량(바이트 단위)을 반환합니다.
 */
uint32_t spi_flash_get_capacity_bytes(void);

/**
 * @brief 512바이트 블록 단위의 전체 블록 개수를 반환합니다.
 */
uint32_t spi_flash_get_block_count(void);

/**
 * @brief 플래시 칩의 Busy(WIP) 상태를 논블로킹으로 확인합니다.
 * @return true 동작 중 (Busy), false 대기/유휴 (Ready)
 */
bool spi_flash_is_busy(void);

/**
 * @brief 플래시 칩의 Busy 상태가 해제될 때까지 블로킹 대기합니다.
 */
void spi_flash_wait_busy(void);

/**
 * @brief SPI DMA를 이용하여 플래시에서 데이터를 읽어옵니다.
 * @param addr 플래시 내 시작 바이트 주소
 * @param buf 데이터를 저장할 버퍼
 * @param len 읽을 바이트 수
 */
void spi_flash_read(uint32_t addr, uint8_t *buf, size_t len);

/**
 * @brief 4KB 섹터를 동기식(블로킹)으로 Erase합니다.
 * @param addr 4KB로 정렬된 시작 주소
 */
void spi_flash_sector_erase_4k(uint32_t addr);

/**
 * @brief 4KB 섹터 Erase 명령을 전송하고 즉시 반환합니다 (논블로킹).
 *        완료 여부는 spi_flash_is_busy()로 확인합니다.
 * @param addr 4KB로 정렬된 시작 주소
 */
void spi_flash_sector_erase_4k_async(uint32_t addr);

/**
 * @brief 1개 페이지(최대 256바이트)를 DMA를 이용하여 동기식으로 플래시에 씁니다.
 * @param addr 플래시 내 시작 주소
 * @param buf 기록할 데이터 버퍼
 * @param len 기록할 바이트 수 (페이지 경계를 넘지 않아야 함)
 */
void spi_flash_page_program(uint32_t addr, const uint8_t *buf, size_t len);

/**
 * @brief 1개 페이지(최대 256바이트) 쓰기 DMA를 시작하고 즉시 반환합니다 (비동기).
 * @param addr 플래시 내 시작 주소
 * @param buf 기록할 데이터 버퍼 (쓰기 완료 시까지 유효해야 함)
 * @param len 기록할 바이트 수 (최대 256바이트)
 */
void spi_flash_page_program_dma_start(uint32_t addr, const uint8_t *buf, size_t len);

/**
 * @brief page_program_dma_start로 시작된 DMA 및 버스 전송이 완료되었는지 확인합니다.
 *        주의: DMA 완료 후에도 플래시 내부 기록(WIP) 완료는 spi_flash_is_busy()로 확인해야 합니다.
 * @return true DMA 및 SPI 버스 전송 완료, false 전송 중
 */
bool spi_flash_page_program_dma_is_done(void);

/**
 * @brief 임의 길이의 데이터를 동기식으로 플래시에 기록합니다.
 * @param addr 플래시 내 시작 주소
 * @param buf 기록할 데이터 버퍼
 * @param len 기록할 바이트 수
 */
void spi_flash_write(uint32_t addr, const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // _SPI_FLASH_H_
