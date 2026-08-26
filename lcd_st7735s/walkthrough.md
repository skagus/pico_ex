# ST7735S LCD 프로젝트 Walkthrough

## 변경 요약

Raspberry Pi Pico (RP2040)에서 ST7735S LCD를 SPI로 구동하여 R→G→B 색상을 1초 간격으로 무한 반복하는 펌웨어를 구현했습니다.

## 생성/수정된 파일

### 신규 생성

| 파일 | 용도 |
|------|------|
| [`st7735s.h`](file:///x:/VCS/pico_ex/lcd_st7735s/src/st7735s.h) | ST7735S 레지스터 정의, 핀 매핑, RGB565 색상 상수, API 선언 |
| [`st7735s.c`](file:///x:/VCS/pico_ex/lcd_st7735s/src/st7735s.c) | SPI/GPIO 초기화, LCD 초기화 시퀀스, 화면 채우기 드라이버 |
| [`main.c`](file:///x:/VCS/pico_ex/lcd_st7735s/src/main.c) | 메인 루프 — LCD 초기화 후 R→G→B 1초 간격 무한 반복 |

### 수정

| 파일 | 변경 내용 |
|------|-----------|
| [`CMakeLists.txt`](file:///x:/VCS/pico_ex/lcd_st7735s/CMakeLists.txt) | `add_executable`에 `src/main.c`, `src/st7735s.c` 추가 |

## 핵심 구현 사항

### 핀 매핑 (SPI0)
- **CS**(GP1): GPIO 소프트웨어 제어
- **SCK**(GP2), **MOSI**(GP3): `spi0` HW 기능
- **DC**(GP4): Data/Command 선택
- **RES**(GP5): 하드웨어 리셋
- **BL**(GP6): 백라이트 ON

### LCD 초기화 시퀀스
1. HW Reset (RES 토글) → SW Reset → Sleep Out
2. Frame Rate, Power Control, VCOM 설정
3. MADCTL (RGB 순서), COLMOD (RGB565/16bit) 설정
4. Gamma 보정 → Normal Mode → Display ON
5. 초기 화면 검은색 클리어

### 화면 채우기 (`lcd_fill_color`)
- 128 픽셀(1줄) 분량의 라인 버퍼를 만들어 160줄 반복 전송
- 전체 화면 버퍼(40KB)를 잡지 않아 메모리 효율적

## 빌드 결과

```
[100%] Built target st7735
```

| 산출물 | 크기 |
|--------|------|
| `st7735.uf2` | 68,096 bytes |
| `st7735.elf` | 74,136 bytes |
| `st7735.bin` | 34,004 bytes |

## 플래시 방법

1. Pico의 **BOOTSEL** 버튼을 누른 채 USB 연결
2. 마운트된 `RPI-RP2` 드라이브에 [`st7735.uf2`](file:///x:/VCS/pico_ex/lcd_st7735s/build/st7735.uf2) 복사
3. 자동 재부팅 후 LCD에 R→G→B 색상이 1초 간격으로 반복 표시됨

> [!TIP]
> USB CDC 시리얼 모니터를 연결하면 현재 표시 중인 색상 이름(`RED`, `GREEN`, `BLUE`)이 출력됩니다.
