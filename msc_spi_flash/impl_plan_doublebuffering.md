# 더블 버퍼링 기반 USB 전송 & SPI PGM 오버랩 구현 계획

USB Mass Storage 쓰기 동작 시, USB 호스트로부터 데이터를 수신하는 작업과 SPI Flash에 4KB Erase 및 256B Page Program을 실행하는 작업을 병렬로 오버랩(Overlap)시키기 위해 **더블 버퍼링 (Ping-Pong 4KB x 2) 및 비동기 상태 머신**을 적용합니다.

---

## 핵심 아키텍처 및 동작 원리

```
[ USB Host (tud_msc_write10_cb) ]
              │
              ▼ (수신)
    ┌──────────────────┐           ┌──────────────────┐
    │ Buffer 0 (4KB)   │  (Active) │ Buffer 1 (4KB)   │ (Flushing)
    │ USB 데이터 수신  │ ◀───────► │ SPI DMA PGM 중...│ ──► [ SPI Flash ]
    └──────────────────┘  (Swap)   └──────────────────┘
```

1. **더블 버퍼 (Ping-Pong Buffer) 구성**:
   - `sector_buffer[2][4096]` (총 8KB SRAM)
   - 각 버퍼 상태: `BUF_STATE_FREE`, `BUF_STATE_ACTIVE` (수신 중), `BUF_STATE_DIRTY_PENDING` (플러시 대기), `BUF_STATE_FLUSHING` (백그라운드 기록 중)
2. **비동기 상태 머신 (`msc_disk_task()`)**:
   - 메인 루프에서 비차단(Non-blocking)으로 호출
   - 단계: `IDLE` ➔ `ERASE_START` ➔ `ERASE_WAIT` ➔ `PAGE_PGM_START (0~15)` ➔ `PAGE_PGM_WAIT` ➔ `COMPLETE`
   - 플래시 비지 체크(`spi_flash_is_busy()`)를 상태 머신에서 폴링하여 메인 스레드와 TinyUSB 스택이 블로킹 없이 원활하게 구동
3. **USB 쓰기 오버랩**:
   - 버퍼 0이 4KB 가득 차면, 버퍼 0을 백그라운드 플러시로 넘기고 `active_buf`를 즉시 버퍼 1로 전환
   - USB 호스트는 딜레이 없이 버퍼 1에 다음 4KB 쓰기 가능
4. **동기화 및 안전성**:
   - 버퍼 1도 꽉 차서 버퍼 0으로 돌아와야 할 때만, 이전 버퍼 0의 플래시 기록 완료를 대기
   - `tud_msc_read10_cb` 시 두 버퍼를 모두 검사하여 최신 캐시 데이터 반환 (Cache Coherency)
   - USB 언마운트(`tud_umount_cb`) 시 남아있는 모든 더티 버퍼의 플러시 완료 보장

---

## 변경 대상 파일

### 1. SPI Flash 드라이버 비동기 API 확장
#### [MODIFY] [spi_flash.h](file:///x:/VCS/pico_ex/msc_spi_flash/src/spi_flash.h)
- 비동기/논블로킹 함수 선언 추가:
  - `bool spi_flash_is_busy(void)`
  - `void spi_flash_sector_erase_4k_async(uint32_t addr)`
  - `void spi_flash_page_program_dma_start(uint32_t addr, const uint8_t *buf, size_t len)`
  - `bool spi_flash_page_program_dma_is_done(void)`

#### [MODIFY] [spi_flash.c](file:///x:/VCS/pico_ex/msc_spi_flash/src/spi_flash.c)
- `spi_flash_is_busy()`: Status Register WIP(Bit 0) 논블로킹 확인
- `spi_flash_sector_erase_4k_async()`: WREN + Erase 명령 전송 후 즉시 리턴
- `spi_flash_page_program_dma_start()`: WREN + Page Program 커맨드 전송 후 TX DMA 비동기 시작
- `spi_flash_page_program_dma_is_done()`: TX DMA 완료 및 SPI 버스 Busy 해제 여부 검사

---

### 2. 더블 버퍼링 및 비동기 섹터 캐시 상태 머신
#### [MODIFY] [msc_disk.c](file:///x:/VCS/pico_ex/msc_spi_flash/src/msc_disk.c)
- 4KB 더블 버퍼 구조체 (`sector_buf_t buffers[2]`) 구현
- `msc_disk_task()` 비동기 상태 머신 구현
- `tud_msc_write10_cb`에서 버퍼 스왑 및 즉시 반환 로직 적용
- `tud_msc_read10_cb`에서 더블 버퍼 캐시 히트 검사
- `flash_disk_flush_cache()` 및 `flash_disk_sync()` 동기화 처리

---

### 3. 메인 어플리케이션 연동
#### [MODIFY] [main.c](file:///x:/VCS/pico_ex/msc_spi_flash/src/main.c)
- `main()` 루프 내에 `msc_disk_task()` 호출 추가하여 백그라운드 플래시 PGM 상태 머신 실행

---

## 검증 계획

- **오버랩 동작 검증**: USB 수신 중에 백그라운드로 SPI DMA & 플래시 기록이 진행되는지 확인
- **데이터 무결성 검증**: 더블 버퍼 스왑 시 LBA 주소 매핑 및 4KB 섹터 경계 처리 정확성 확인
- **캐시 일관성 검증**: PGM 진행 중인 버퍼에 대한 Read 요청 시 최신 데이터 반환 여부 확인
- **언마운트 시 안전 플러시 검증**: 호스트 언마운트 시 버퍼에 남은 잔여 더티 데이터의 완전 플러시 보장
