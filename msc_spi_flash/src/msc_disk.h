#ifndef _MSC_DISK_H_
#define _MSC_DISK_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB MSC 디스크 백그라운드 비동기 태스크 (더블 버퍼링 플래시 PGM 상태 머신 구동)
 */
void msc_disk_task(void);

/**
 * @brief 모든 대기 중인 더티 버퍼를 플래시에 완전히 기록하고 동기화합니다.
 */
void flash_disk_sync(void);

#ifdef __cplusplus
}
#endif

#endif // _MSC_DISK_H_
