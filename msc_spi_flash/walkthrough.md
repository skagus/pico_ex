# 외부 SPI Flash (25DF321 등) 및 SPI DMA 기반 USB MSC 구현 결과

[project.txt](file:///x:/VCS/pico_ex/msc_spi_flash/project.txt)의 요구사항에 따라 RP2040의 내부 XIP Flash 대신 외부 SPI 버스에 연결된 NOR Flash 및 SPI DMA를 활용하는 USB Mass Storage 기능을 구현하였습니다.

---

## 주요 구현 내용

### 1. 하드웨어 SPI 및 GPIO 핀 설정
- **SPI 인스턴스**: `spi0` (최대 31.25 MHz)
- **핀 맵**:
  - `CS_PIN` : GPIO 1 (소프트웨어 GPIO 출력 제어)
  - `SCK_PIN` : GPIO 2 (SPI0 SCK)
  - `MOSI_PIN` : GPIO 3 (SPI0 TX)
  - `MISO_PIN` : GPIO 4 (SPI0 RX)

### 2. SPI DMA 기반 I/O 구현
- `hardware_dma` 라이브러리를 사용하여 송수신 채널 구성
- **Read DMA**: RX DMA 수신 채널과 더미 바이트(0xFF) TX DMA 채널을 연동하여 고속 연속 읽기 처리
- **Page Program DMA**: TX DMA를 통한 최대 256바이트 페이지 쓰기 및 Busy 대기(RDSR 폴링) 처리

### 3. JEDEC ID 감지 및 25DF321 지원
- JEDEC Read ID(`0x9F`) 명령을 통해 Manufacturer ID, Memory Type, Capacity 식별
- 용량 ID에 따라 디스크 블록 수 (`용량 / 512`) 동적 계산 (25DF321: 4MB / 8,192 블록)
- 25DF321/AT25DF 계열에서 전원 인가 시 기본 설정될 수 있는 Write Protect 상태를 `spi_flash_global_unprotect()`를 통해 해제

### 4. USB MSC 디스크 레이어 및 캐시 연동
- [msc_disk.c](file:///x:/VCS/pico_ex/msc_spi_flash/src/msc_disk.c)에서 XIP 플래시 의존 코드를 제거하고 [spi_flash.h](file:///x:/VCS/pico_ex/msc_spi_flash/src/spi_flash.h) 인터페이스와 연결
- 4KB 섹터 단위 캐시를 유지하여 4KB Erase 및 256B Page Program을 최적화하여 플래시에 기록
- `tud_msc_capacity_cb`에서 감지된 플래시 용량을 반환하여 호스트에 올바른 디스크 크기 전달

---

## 변경된 파일 목록

| 파일 | 변경 요약 |
|---|---|
| [CMakeLists.txt](file:///x:/VCS/pico_ex/msc_spi_flash/CMakeLists.txt) | `spi_flash.c` 소스 및 `hardware_spi`, `hardware_dma` 링크 추가 |
| [src/spi_flash.h](file:///x:/VCS/pico_ex/msc_spi_flash/src/spi_flash.h) | SPI Flash 드라이버 헤더 및 구조체 선언 |
| [src/spi_flash.c](file:///x:/VCS/pico_ex/msc_spi_flash/src/spi_flash.c) | SPI0/DMA 초기화, JEDEC ID 파싱, DMA Read/Write, 4K Erase 구현 |
| [src/msc_disk.c](file:///x:/VCS/pico_ex/msc_spi_flash/src/msc_disk.c) | 외부 SPI Flash DMA 드라이버와 4KB 섹터 캐시 연동 |
| [src/main.c](file:///x:/VCS/pico_ex/msc_spi_flash/src/main.c) | 부팅 시 `spi_flash_init()` 호출 및 초기화 결과 출력 연동 |
