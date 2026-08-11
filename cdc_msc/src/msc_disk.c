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
#include "hardware/flash.h"
#include "hardware/sync.h"


#define DISK_BLOCK_NUM      (3072)
#define DISK_BLOCK_SIZE     (512)
#define FULL_SECT_BITMAP    ((1 << (FLASH_SECTOR_SIZE / DISK_BLOCK_SIZE)) - 1) // 4KB 섹터를 512바이트 단위로 나눈 비트맵
#ifndef FLASH_TARGET_OFFSET
#define FLASH_TARGET_OFFSET  ((PICO_FLASH_SIZE_BYTES - (DISK_BLOCK_NUM * DISK_BLOCK_SIZE)) & ~(FLASH_SECTOR_SIZE - 1))
#endif

#define LOG(...)      printf(__VA_ARGS__)

 // whether host does safe-eject
static bool ejected = false;
// 4KB 섹터 단위로 읽고/쓰는 작업을 안전하게 처리하기 위한 캐시
static uint8_t sector_cache[FLASH_SECTOR_SIZE];
static int32_t cached_sector_addr = -1;
static uint8_t dirty_bitmask = 0;

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  (void)lun;

  const char vid[] = "TinyUSB";
  const char pid[] = "Mass Storage";
  const char rev[] = "1.0";

  memcpy(vendor_id, vid, strlen(vid));
  memcpy(product_id, pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  (void)lun;

  // RAM disk is ready until ejected
  if(ejected) {
    // Additional Sense 3A-00 is NOT_FOUND
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return false;
  }

  return true;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
  (void)lun;

  *block_count = DISK_BLOCK_NUM;
  *block_size = DISK_BLOCK_SIZE;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
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
      // unload disk storage
      ejected = true;
    }
  }

  return true;
}

bool flash_read_cache(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t size)
{
  uint32_t addr = FLASH_TARGET_OFFSET + (lba * DISK_BLOCK_SIZE) + offset;
  uint32_t sector_start_addr = addr & ~(FLASH_SECTOR_SIZE - 1);
  uint32_t offset_in_sector = addr - sector_start_addr;

  if(cached_sector_addr == sector_start_addr)
  {
    memcpy(buffer, sector_cache + offset_in_sector, size);
    return true;
  }
  return false;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  (void) lun;

  if (lba >= DISK_BLOCK_NUM) return -1;
  if (offset > DISK_BLOCK_SIZE) return -1;
  if (bufsize > (DISK_BLOCK_SIZE - offset)) return -1;

  if(!flash_read_cache(lba, offset, buffer, bufsize))
  {
    uint8_t const* flash_target_contents = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET + (lba * DISK_BLOCK_SIZE) + offset);
    memcpy(buffer, flash_target_contents, bufsize);
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
  if (lba >= DISK_BLOCK_NUM) return false;
  if (offset > DISK_BLOCK_SIZE) return false;
  if (bufsize > (DISK_BLOCK_SIZE - offset)) return false;
  return true;
}

void flash_disk_flush_cache(void)
{
  if (cached_sector_addr == -1) return;

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase((uint32_t) cached_sector_addr, FLASH_SECTOR_SIZE);
  flash_range_program((uint32_t) cached_sector_addr, sector_cache, FLASH_SECTOR_SIZE);
  restore_interrupts(ints);

  cached_sector_addr = -1;
  dirty_bitmask = 0;
  LOG("F\n");
}

// 플래시 쓰기 함수는 무조건 RAM에서 실행
int32_t __not_in_flash_func(tud_msc_write10_cb)(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
  (void) lun;

  if (!flash_disk_range_valid(lba, offset, bufsize)) return -1;

  uint32_t write_addr = FLASH_TARGET_OFFSET + (lba * DISK_BLOCK_SIZE) + offset;
  uint32_t sector_start_addr = write_addr & ~(FLASH_SECTOR_SIZE - 1);
  uint32_t offset_in_sector = write_addr - sector_start_addr;

  if(bufsize != DISK_BLOCK_SIZE)
  {
    LOG("W %X o %X sz %d\n", lba, offset, bufsize);
  }
  else
  {
    LOG("W");
  }

  if (cached_sector_addr != -1 && cached_sector_addr != (int32_t) sector_start_addr)
  {
    flash_disk_flush_cache();
  }

  if (cached_sector_addr == -1) // 캐시가 비어있으면 섹터를 읽어서 캐시에 저장
  {
    cached_sector_addr = (int32_t) sector_start_addr;
    memcpy(sector_cache, (const void*)(XIP_BASE + sector_start_addr), FLASH_SECTOR_SIZE);
  }
  memcpy(sector_cache + offset_in_sector, buffer, bufsize);

  uint32_t block_index = offset_in_sector / DISK_BLOCK_SIZE;
  uint32_t block_count = (bufsize + DISK_BLOCK_SIZE - 1) / DISK_BLOCK_SIZE;
  for(uint32_t i = 0; i < block_count; i++)
  {
    dirty_bitmask |= (1u << (block_index + i));
  }

  if(dirty_bitmask == FULL_SECT_BITMAP) // 4KB 섹터가 모두 dirty 상태이면 바로 플래시에 기록
  {
    flash_disk_flush_cache();
  }

  return (int32_t) bufsize;
}


// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
  // read10 & write10 has their own callback and MUST not be handled here

  void const* response = NULL;
  int32_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch(scsi_cmd[0])
  {
    case SCSI_CMD_READ_CAPACITY_10:
    {
      uint32_t block_count = DISK_BLOCK_NUM - 1;
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

    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
      break;
  }

  // return resplen must not larger than bufsize
  if(resplen > bufsize) resplen = bufsize;

  if(response && (resplen > 0))
  {
    if(in_xfer)
    {
      memcpy(buffer, response, (size_t)resplen);
    }
    else
    {
      // SCSI output
    }
  }

  return (int32_t)resplen;
}
