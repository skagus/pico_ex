#include <string.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "spi_man.h"

uint32_t gbm_free_spi_req = (1 << NUM_SPI_REQ) - 1;
spi_req_t ga_spi_reqs[NUM_SPI_REQ];
spi_req_t *g_req_queue_head = NULL;

static spi_req_t *g_req_queue_tail = NULL;
static spi_req_t *g_current_req = NULL;
static spi_inst_t *g_spi = NULL;
static int g_dma_tx_chan = -1;

static void spi_man_start_transfer(spi_req_t *req);
static void spi_man_dma_irq_handler(void);

spi_req_t* spi_alloc_req(void) {
    while (true) {
        uint32_t status = save_and_disable_interrupts();
        for (int i = 0; i < NUM_SPI_REQ; i++) {
            if (gbm_free_spi_req & (1u << i)) {
                gbm_free_spi_req &= ~(1u << i);
                restore_interrupts(status);
                
                spi_req_t *req = &ga_spi_reqs[i];
                memset(req, 0, sizeof(spi_req_t));
                return req;
            }
        }
        restore_interrupts(status);
        tight_loop_contents();
    }
}

void spi_free_req(spi_req_t *req) {
    if (req == NULL) return;
    
    int idx = (int)(req - ga_spi_reqs);
    if (idx >= 0 && idx < NUM_SPI_REQ) {
        uint32_t status = save_and_disable_interrupts();
        gbm_free_spi_req |= (1u << idx);
        restore_interrupts(status);
    }
}

static void spi_man_start_transfer(spi_req_t *req) {
    if (req == NULL || g_spi == NULL || g_dma_tx_chan < 0) return;

    // 1. Before SPI Transaction Callback
    if (req->pre_exec) {
        req->pre_exec(spi_get_hw(g_spi));
    }

    // 2. SPI 데이터 비트 폭 확인 (8비트 vs 16비트 등)
    uint8_t dss = (spi_get_hw(g_spi)->cr0 & SPI_SSPCR0_DSS_BITS) + 1;
    enum dma_channel_transfer_size dma_size = (dss > 8) ? DMA_SIZE_16 : DMA_SIZE_8;

    // 3. DMA 채널 설정 및 트리거
    dma_channel_config c = dma_channel_get_default_config(g_dma_tx_chan);
    channel_config_set_transfer_data_size(&c, dma_size);
    channel_config_set_dreq(&c, spi_get_dreq(g_spi, true));
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);

    dma_channel_configure(
        g_dma_tx_chan,
        &c,
        &spi_get_hw(g_spi)->dr,
        req->data,
        req->len,
        true
    );
}

static void spi_man_dma_irq_handler(void) {
    if (g_dma_tx_chan >= 0 && dma_channel_get_irq0_status(g_dma_tx_chan)) {
        dma_channel_acknowledge_irq0(g_dma_tx_chan);

        // SPI FIFO 및 전송 완료 대기
        while (spi_get_hw(g_spi)->sr & SPI_SSPSR_BSY_BITS) {
            tight_loop_contents();
        }

        spi_req_t *completed_req = g_current_req;

        // 1. After SPI Transaction Callback
        if (completed_req && completed_req->post_exec) {
            completed_req->post_exec(spi_get_hw(g_spi));
        }

        // 2. 요청자의 동기 완료 플래그 세팅
        if (completed_req && completed_req->p_done) {
            *(completed_req->p_done) = true;
        }

        // 3. 완료된 요청 반환
        if (completed_req) {
            spi_free_req(completed_req);
        }

        // 3. 다음 대기 요청 팝 및 전송 시작
        spi_req_t *next_req = g_req_queue_head;
        if (next_req != NULL) {
            g_req_queue_head = next_req->next;
            if (g_req_queue_head == NULL) {
                g_req_queue_tail = NULL;
            }
            next_req->next = NULL;
            g_current_req = next_req;

            spi_man_start_transfer(next_req);
        } else {
            g_current_req = NULL;
        }
    }
}

void spi_push_req(spi_req_t *req) {
    if (req == NULL) return;
    req->next = NULL;

    uint32_t status = save_and_disable_interrupts();
    if (g_current_req == NULL) {
        g_current_req = req;
        restore_interrupts(status);
        spi_man_start_transfer(req);
    } else {
        if (g_req_queue_head == NULL) {
            g_req_queue_head = req;
            g_req_queue_tail = req;
        } else {
            g_req_queue_tail->next = req;
            g_req_queue_tail = req;
        }
        restore_interrupts(status);
    }
}

void spi_man_init(spi_inst_t *spi) {
    g_spi = spi;
    g_req_queue_head = NULL;
    g_req_queue_tail = NULL;
    g_current_req = NULL;
    gbm_free_spi_req = (1 << NUM_SPI_REQ) - 1;

    // DMA TX 채널 할당 및 IRQ0 연결
    if (g_dma_tx_chan < 0) {
        g_dma_tx_chan = dma_claim_unused_channel(true);
        dma_channel_set_irq0_enabled(g_dma_tx_chan, true);
        irq_set_exclusive_handler(DMA_IRQ_0, spi_man_dma_irq_handler);
        irq_set_enabled(DMA_IRQ_0, true);
    }
}

bool spi_man_is_busy(void) {
    if (g_current_req != NULL) {
        return true;
    }
    if (g_dma_tx_chan >= 0 && dma_channel_is_busy(g_dma_tx_chan)) {
        return true;
    }
    if (g_spi != NULL && (spi_get_hw(g_spi)->sr & SPI_SSPSR_BSY_BITS)) {
        return true;
    }
    return false;
}

void spi_man_wait_idle(void) {
    while (spi_man_is_busy()) {
        tight_loop_contents();
    }
}
