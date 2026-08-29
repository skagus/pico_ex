#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/sync.h"

#define NUM_SPI_REQ     (4)

/**
 * SPI DMA request.
 */
typedef struct spi_req
{
    uint8_t *data;
    uint32_t len;
    void (*pre_exec)(spi_hw_t *hw);     // before SPI transaction.
    void (*post_exec)(spi_hw_t *hw);    // after SPI transaction.
    struct spi_req *next;
} spi_req_t;

extern uint32_t gbm_free_spi_req;
extern spi_req_t ga_spi_reqs[NUM_SPI_REQ];
extern spi_req_t *g_req_queue_head;

/**
 * @brief SPI Request Manager 초기화 (DMA 채널 할당 및 인터럽트 설정)
 * @param spi SPI 인스턴스 (예: spi0, spi1)
 */
void spi_man_init(spi_inst_t *spi);

/**
 * @brief 여유 있는 SPI 요청 구조체 할당
 * @return 할당된 spi_req_t 포인터 (여유 슬롯 없을 시 NULL)
 */
spi_req_t* spi_alloc_req(void);

/**
 * @brief SPI 요청 구조체 반환
 * @param req 반환할 spi_req_t 포인터
 */
void spi_free_req(spi_req_t *req);

/**
 * @brief SPI 요청을 큐에 등록하고 유휴 상태일 시 전송 시작
 * @param req 등록할 spi_req_t 포인터
 */
void spi_push_req(spi_req_t *req);

/**
 * @brief SPI DMA 및 큐 작업 진행 여부 확인
 */
bool spi_man_is_busy(void);

/**
 * @brief 모든 대기 큐 및 SPI DMA 전송이 완료될 때까지 대기
 */
void spi_man_wait_idle(void);
