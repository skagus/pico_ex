# 외부 SPI Flash 및 SPI DMA 기반 USB MSC 구현 계획

기존 RP2040 내부 XIP Flash를 활용하던 USB MSC 방식을, `project.txt`에 명시된 하드웨어 핀 및 SPI DMA 기반 외부 NOR SPI Flash(25DF321 등) 제어 방식으로 변경합니다. 또한 JEDEC ID 판별 로직을 통해 다양한 용량의 SPI Flash를 유연하게 지원하도록 구현합니다.

## 제안 사항 및 하드웨어 사양

1. **핀 배치 (RP2040 SPI0)**
   - `CS_PIN` : GPIO 1 (소프트웨어 GPIO 출력 제어)
   - `SCK_PIN` : GPIO 2 (SPI0 SCK)
   - `MOSI_PIN` : GPIO 3 (SPI0 TX)
   - `MISO_PIN` : GPIO 4 (SPI0 RX)

2. **SPI DMA 통신 구조**
   - `hardware_spi` 및 `hardware_dma` 사용
   - 고속 SPI 클럭 설정 (예: 25~31.25 MHz)
   - Read DMA: TX에 더미 바이트(0xFF)를 연속 송신하고 RX DMA로 Flash 데이터 버퍼 수신
   - Page Program DMA: TX DMA로 커맨드/주소 및 데이터 버퍼 연속 전송

3. **SPI Flash 장치 지원 및 JEDEC ID 판별**
   - JEDEC Read ID 커맨드(`0x9F`)로 Manufacturer ID, Memory Type, Capacity 코드 획득
   - 기본 대상: **25DF321 (32Mbit = 4MB)**
   - 용량 필드(`0x16` -> 4MB, `0x17` -> 8MB, `0x18` -> 16MB 등)를 기반으로 디스크 총 블록 수 자동 계산 (`Capacity / 512`)
   - 25DF321/AT25DF 계열의 특성상 부팅 시 섹터 보호가 걸려 있을 수 있으므로 `Global Unprotect` / `Write Status Register` 명령으로 프로텍션 해제 수행

4. **MSC 디스크 캐시 및 I/O 처리**
   - 4KB 섹터 단위 캐시 유지: Flash는 4KB 단위로 Erase해야 하므로 기존 4KB 섹터 캐시 구조를 재활용하여 SPI DMA Erase / Program과 연동
   - LUN 용량 콜백(`tud_msc_capacity_cb`)에서 Flash 실측 용량 반영

---

## 변경 대상 파일

### 1. CMake 빌드 설정
#### [MODIFY] [CMakeLists.txt](file:///x:/VCS/pico_ex/msc_spi_flash/CMakeLists.txt)
- `src/spi_flash.c` 소스 파일 목록에 추가
- `target_link_libraries`에 `hardware_spi`, `hardware_dma` 추가

---

### 2. SPI Flash DMA 드라이버
#### [NEW] [spi_flash.h](file:///x:/VCS/pico_ex/msc_spi_flash/src/spi_flash.h)
- SPI Flash 명령어 정의 (`0x9F`, `0x06`, `0x05`, `0x03`, `0x02`, `0x20` 등)
- 핀 매크로 정의 (`CS=1`, `SCK=2`, `MOSI=3`, `MISO=4`)
- 플래시 정보 구조체 (`spi_flash_info_t`) 및 함수 선언:
  - `bool spi_flash_init(void)`
  - `uint32_t spi_flash_get_capacity_bytes(void)`
  - `uint32_t spi_flash_get_block_count(void)`
  - `void spi_flash_read(uint32_t addr, uint8_t *buf, size_t len)`
  - `void spi_flash_sector_erase_4k(uint32_t addr)`
  - `void spi_flash_page_program(uint32_t addr, const uint8_t *buf, size_t len)`
  - `void spi_flash_write(uint32_t addr, const uint8_t *buf, size_t len)`

#### [NEW] [spi_flash.c](file:///x:/VCS/pico_ex/msc_spi_flash/src/spi_flash.c)
- SPI0 및 GPIO(GP1, GP2, GP3, GP4) 초기화
- DMA 송수신 채널 할당 및 설정
- JEDEC ID 읽기 및 모델/용량 자동 감지
- Write Enable, Busy Polling, Erase, DMA Read/Write 구현

---

### 3. USB MSC 스토리지 레이어
#### [MODIFY] [msc_disk.c](file:///x:/VCS/pico_ex/msc_spi_flash/src/msc_disk.c)
- XIP 플래시 직접 참조(`XIP_BASE`, `flash_range_erase`, `flash_range_program`) 제거
- `spi_flash.h` 인터페이스 연동
- `tud_msc_capacity_cb`에서 `spi_flash_get_block_count()`로 동적 용량 전달
- 섹터 캐시 플러시 시 `spi_flash_sector_erase_4k()` 및 `spi_flash_write()`(Page Program) 호출
- `tud_msc_read10_cb`에서 캐시 미스 시 `spi_flash_read()` 호출

---

### 4. 메인 어플리케이션
#### [MODIFY] [main.c](file:///x:/VCS/pico_ex/msc_spi_flash/src/main.c)
- `board_init()` 또는 시작 시 `spi_flash_init()` 호출 및 초기화 결과 출력
- 기존 QSPI 핀(핀 1)을 직접 조작하던 `read_bootsel_button()` 및 `wait_boot_sel()` 정리 (CS핀 GPIO 1과 겹칠 수 있으므로 안전하게 조정)

---

## 검증 계획

- **코드 구조 검증**: SPI 핀 번호(1, 2, 3, 4), SPI0 인스턴스, DMA DREQ(DREQ_SPI0_TX/RX) 설정의 정확성 확인
- **JEDEC ID 파싱 로직 검증**: 25DF321 (32Mbit) 및 기타 용량 계산식 검증
- **캐시 및 블록 매핑 검증**: 512B LBA -> 4KB 섹터 및 256B 페이지 경계 쓰기 처리 정합성 검증
