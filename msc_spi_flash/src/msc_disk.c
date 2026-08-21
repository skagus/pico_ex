/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"
#include "spi_flash.h"
#include "msc_disk.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#define DISK_BLOCK_SIZE     (512)
#define BLOCKS_PER_SECTOR   (SPI_FLASH_SECTOR_SIZE / DISK_BLOCK_SIZE) // 8
#define FULL_SECT_BITMAP    ((1 << BLOCKS_PER_SECTOR) - 1)            // 0xFF

#define LOG(...)            printf(__VA_ARGS__)

typedef enum {
    BUF_STATE_FREE,             // 비어있음
    BUF_STATE_ACTIVE,           // 현재 USB 호스트로부터 쓰기 수신 중
    BUF_STATE_DIRTY_PENDING,    // 플래시 기록 대기 중
    BUF_STATE_FLUSHING          // 백그라운드에서 SPI Flash로 기록 중
} buf_state_t;

typedef struct {
    uint8_t     data[SPI_FLASH_SECTOR_SIZE];
    int32_t     sector_addr;    // 4KB 정렬 플래시 주소 (-1이면 무효)
    uint8_t     dirty_mask;     // 512B 블록 단위 더티 비트맵 (8비트)
    buf_state_t state;
} sector_buf_t;

typedef enum {
    FLUSH_STATE_IDLE,
    FLUSH_STATE_ERASE_START,
    FLUSH_STATE_ERASE_WAIT,
    FLUSH_STATE_PAGE_START,
    FLUSH_STATE_PAGE_DMA_WAIT,
    FLUSH_STATE_PAGE_BUSY_WAIT,
} flush_state_t;

// 더블 버퍼링 (4KB x 2 = 8KB)
static sector_buf_t buffers[2] = {
    { .sector_addr = -1, .dirty_mask = 0, .state = BUF_STATE_FREE },
    { .sector_addr = -1, .dirty_mask = 0, .state = BUF_STATE_FREE }
};
static int active_idx = 0;

static flush_state_t flush_state = FLUSH_STATE_IDLE;
static int flush_idx = -1;
static uint32_t flush_page_idx = 0; // 0 ~ 15

static bool ejected = false;

//--------------------------------------------------------------------+
// 백그라운드 비동기 플러시 상태 머신
//--------------------------------------------------------------------+
void msc_disk_task(void)
{
    switch (flush_state)
    {
        case FLUSH_STATE_IDLE:
        {
            // 플래시 기록 대기 중인 버퍼 찾기
            int target = -1;
            for (int i = 0; i < 2; i++)
            {
                if (buffers[i].state == BUF_STATE_DIRTY_PENDING)
                {
                    target = i;
                    break;
                }
            }

            if (target >= 0)
            {
                flush_idx = target;
                buffers[flush_idx].state = BUF_STATE_FLUSHING;
                flush_state = FLUSH_STATE_ERASE_START;
            }
            break;
        }

        case FLUSH_STATE_ERASE_START:
        {
            // [지연 읽기 및 병합]: 4KB가 전부 채워지지 않은 부분 쓰기인 경우,
            // 호스트가 수정하지 않은 빈 블록만 원본 플래시에서 읽어와 채워 넣음(Merge)
            if (buffers[flush_idx].dirty_mask != FULL_SECT_BITMAP)
            {
                for (int b = 0; b < BLOCKS_PER_SECTOR; b++)
                {
                    if (!(buffers[flush_idx].dirty_mask & (1u << b)))
                    {
                        uint32_t block_addr = (uint32_t)buffers[flush_idx].sector_addr + (b * DISK_BLOCK_SIZE);
                        spi_flash_read(block_addr, buffers[flush_idx].data + (b * DISK_BLOCK_SIZE), DISK_BLOCK_SIZE);
                    }
                }
                buffers[flush_idx].dirty_mask = FULL_SECT_BITMAP;
            }

            // 비동기 4KB 섹터 Erase 시작 (논블로킹)
            spi_flash_sector_erase_4k_async((uint32_t)buffers[flush_idx].sector_addr);
            flush_state = FLUSH_STATE_ERASE_WAIT;
            break;
        }

        case FLUSH_STATE_ERASE_WAIT:
        {
            if (!spi_flash_is_busy())
            {
                flush_page_idx = 0;
                flush_state = FLUSH_STATE_PAGE_START;
            }
            break;
        }

        case FLUSH_STATE_PAGE_START:
        {
            uint32_t page_addr = (uint32_t)buffers[flush_idx].sector_addr + (flush_page_idx * SPI_FLASH_PAGE_SIZE);
            const uint8_t *page_data = buffers[flush_idx].data + (flush_page_idx * SPI_FLASH_PAGE_SIZE);

            spi_flash_page_program_dma_start(page_addr, page_data, SPI_FLASH_PAGE_SIZE);
            flush_state = FLUSH_STATE_PAGE_DMA_WAIT;
            break;
        }

        case FLUSH_STATE_PAGE_DMA_WAIT:
        {
            if (spi_flash_page_program_dma_is_done())
            {
                flush_state = FLUSH_STATE_PAGE_BUSY_WAIT;
            }
            break;
        }

        case FLUSH_STATE_PAGE_BUSY_WAIT:
        {
            if (!spi_flash_is_busy())
            {
                flush_page_idx++;
                if (flush_page_idx < (SPI_FLASH_SECTOR_SIZE / SPI_FLASH_PAGE_SIZE)) // 16개 페이지 (4KB)
                {
                    flush_state = FLUSH_STATE_PAGE_START;
                }
                else
                {
                    // 4KB 섹터 플러시 완료
                    buffers[flush_idx].dirty_mask = 0;
                    buffers[flush_idx].state = BUF_STATE_FREE;
                    LOG("F%d\n", flush_idx);
                    flush_idx = -1;
                    flush_state = FLUSH_STATE_IDLE;
                }
            }
            break;
        }
    }
}

static void wait_buffer_free(int idx)
{
    while (buffers[idx].state == BUF_STATE_DIRTY_PENDING || buffers[idx].state == BUF_STATE_FLUSHING)
    {
        msc_disk_task();
        tight_loop_contents();
    }
}

void flash_disk_sync(void)
{
    // 현재 활성 버퍼에 기록되지 않은 더티 데이터가 있으면 플러시 대기로 등록
    if (buffers[active_idx].state == BUF_STATE_ACTIVE && buffers[active_idx].dirty_mask != 0)
    {
        buffers[active_idx].state = BUF_STATE_DIRTY_PENDING;
    }

    // 모든 버퍼가 FREE 또는 완료될 때까지 상태 머신을 구동하여 동기화
    while (flush_state != FLUSH_STATE_IDLE ||
           buffers[0].state == BUF_STATE_DIRTY_PENDING || buffers[0].state == BUF_STATE_FLUSHING ||
           buffers[1].state == BUF_STATE_DIRTY_PENDING || buffers[1].state == BUF_STATE_FLUSHING)
    {
        msc_disk_task();
        tight_loop_contents();
    }
}

//--------------------------------------------------------------------+
// TinyUSB MSC 콜백
//--------------------------------------------------------------------+

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  (void)lun;

  const char vid[] = "TinyUSB";
  const char pid[] = "SPI Flash MSC";
  const char rev[] = "1.0";

  memcpy(vendor_id, vid, strlen(vid));
  memcpy(product_id, pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  (void)lun;

  if(ejected) {
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return false;
  }

  return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
  (void)lun;

  *block_count = spi_flash_get_block_count();
  *block_size = DISK_BLOCK_SIZE;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  (void)lun;
  (void)power_condition;

  if(load_eject)
  {
    if(start)
    {
      // load disk storage
    }
    else
    {
      // 언마운트 / 꺼내기 시 미기록 데이터 완전 플러시
      flash_disk_sync();
      ejected = true;
    }
  }

  return true;
}

// 더블 버퍼 중 지정 주소의 섹터를 들고 있는 버퍼 검색
static int find_cached_buffer(uint32_t sector_addr)
{
    for (int i = 0; i < 2; i++)
    {
        if (buffers[i].sector_addr == (int32_t)sector_addr && buffers[i].state != BUF_STATE_FREE)
        {
            return i;
        }
    }
    return -1;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  (void) lun;

  uint32_t total_blocks = spi_flash_get_block_count();
  if (lba >= total_blocks) return -1;
  if (offset > DISK_BLOCK_SIZE) return -1;
  if (bufsize > (DISK_BLOCK_SIZE - offset)) return -1;

  uint32_t addr = (lba * DISK_BLOCK_SIZE) + offset;
  uint32_t sector_start_addr = addr & ~(SPI_FLASH_SECTOR_SIZE - 1);
  uint32_t offset_in_sector = addr - sector_start_addr;
  uint32_t block_index = offset_in_sector / DISK_BLOCK_SIZE;

  int hit_idx = find_cached_buffer(sector_start_addr);
  // 캐시 버퍼에 존재하고 해당 블록이 수정(더티)된 상태인 경우 버퍼에서 즉시 반환
  if (hit_idx >= 0 && (buffers[hit_idx].dirty_mask & (1u << block_index)))
  {
    memcpy(buffer, buffers[hit_idx].data + offset_in_sector, bufsize);
  }
  else
  {
    // 캐시 미스이거나 해당 블록이 아직 쓰이지 않은 상태이면 SPI Flash에서 읽기
    spi_flash_read(addr, (uint8_t*)buffer, bufsize);
  }

  return (int32_t)bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun)
{
  (void) lun;
  return true;
}

static bool flash_disk_range_valid(uint32_t lba, uint32_t offset, uint32_t bufsize)
{
  uint32_t total_blocks = spi_flash_get_block_count();
  if (lba >= total_blocks) return false;
  if (offset > DISK_BLOCK_SIZE) return false;
  if (bufsize > (DISK_BLOCK_SIZE - offset)) return false;
  return true;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
  (void) lun;

  if (!flash_disk_range_valid(lba, offset, bufsize)) return -1;

  uint32_t write_addr = (lba * DISK_BLOCK_SIZE) + offset;
  uint32_t sector_start_addr = write_addr & ~(SPI_FLASH_SECTOR_SIZE - 1);
  uint32_t offset_in_sector = write_addr - sector_start_addr;

  // 현재 활성 버퍼가 다른 섹터를 가리키고 있는 경우 스왑 및 플러시 대기 처리
  if (buffers[active_idx].sector_addr != (int32_t)sector_start_addr)
  {
    if (buffers[active_idx].state == BUF_STATE_ACTIVE && buffers[active_idx].dirty_mask != 0)
    {
        // 현재 버퍼를 백그라운드 플러시로 넘김
        buffers[active_idx].state = BUF_STATE_DIRTY_PENDING;
    }

    // 반대편 버퍼로 전환
    int next_idx = 1 - active_idx;
    // 반대편 버퍼가 아직 플러시 중이면 완료 대기
    wait_buffer_free(next_idx);

    active_idx = next_idx;
    buffers[active_idx].sector_addr = (int32_t)sector_start_addr;
    buffers[active_idx].dirty_mask = 0;
    buffers[active_idx].state = BUF_STATE_ACTIVE;

    // [지연 읽기]: 새 섹터 할당 시 플래시에서 읽어오지 않음 (호스트 데이터 바로 수신)
  }

  // 버퍼에 데이터 복사
  memcpy(buffers[active_idx].data + offset_in_sector, buffer, bufsize);

  // 더티 비트마스크 설정
  uint32_t block_index = offset_in_sector / DISK_BLOCK_SIZE;
  uint32_t block_count = (bufsize + DISK_BLOCK_SIZE - 1) / DISK_BLOCK_SIZE;
  for (uint32_t i = 0; i < block_count; i++)
  {
    buffers[active_idx].dirty_mask |= (1u << (block_index + i));
  }

  // 4KB 섹터가 꽉 찼으면 즉시 백그라운드 플러시 등록 및 버퍼 스왑 준비
  if (buffers[active_idx].dirty_mask == FULL_SECT_BITMAP)
  {
    buffers[active_idx].state = BUF_STATE_DIRTY_PENDING;
    msc_disk_task(); // 즉시 상태 머신 1회 구동하여 Erase/PGM 트리거

    // 다음 쓰기를 위해 미리 반대편 버퍼로 스왑 준비
    int next_idx = 1 - active_idx;
    active_idx = next_idx;
  }

  // 상태 머신 주기적 구동
  msc_disk_task();

  return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
  void const* response = NULL;
  int32_t resplen = 0;
  bool in_xfer = true;

  switch(scsi_cmd[0])
  {
    case SCSI_CMD_READ_CAPACITY_10:
    {
      uint32_t block_count = spi_flash_get_block_count() - 1;
      uint16_t block_size = DISK_BLOCK_SIZE;
      uint8_t* p = (uint8_t*) buffer;

      p[0] = (uint8_t) (block_count >> 24);
      p[1] = (uint8_t) (block_count >> 16);
      p[2] = (uint8_t) (block_count >> 8);
      p[3] = (uint8_t) (block_count >> 0);
      p[4] = 0;
      p[5] = 0;
      p[6] = 0;
      p[7] = 0;
      p[8] = (uint8_t) (block_size >> 8);
      p[9] = (uint8_t) (block_size >> 0);
      p[10] = 0;
      p[11] = 0;
      resplen = 12;
      in_xfer = true;
      break;
    }

    case SCSI_CMD_INQUIRY:
      resplen = 0;
      break;

    case SCSI_CMD_REQUEST_SENSE:
      resplen = 0;
      break;

    case 0x35: // SCSI_CMD_SYNCHRONIZE_CACHE_10
      flash_disk_sync();
      resplen = 0;
      break;

    default:
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
      resplen = -1;
      break;
  }

  if(resplen > bufsize) resplen = bufsize;

  if(response && (resplen > 0))
  {
    if(in_xfer)
    {
      memcpy(buffer, response, (size_t)resplen);
    }
  }

  return (int32_t)resplen;
}
