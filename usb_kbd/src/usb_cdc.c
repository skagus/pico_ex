
#include "pico/stdio/driver.h"
#include "tusb.h"
#include "kbd.h"


// 1. stdio 출력을 담당할 함수 (printf -> CDC)
static void stdio_usb_out_chars(const char* buf, int length)
{
	if(tud_cdc_connected())
	{
		for(int i = 0; i < length; i++)
		{
			tud_cdc_write_char(buf[i]);
		}
		tud_cdc_write_flush();
	}
}

// 2. stdio 입력을 담당할 함수 (CDC -> scanf/getchar)
static int stdio_usb_in_chars(char* buf, int length)
{
	if(tud_cdc_connected() && tud_cdc_available())
	{
		return (int)tud_cdc_read(buf, (uint32_t)length);
	}
	return PICO_ERROR_NO_DATA;
}

// 3. stdio 드라이버 구조체 정의
stdio_driver_t stdio_usb_custom = 
{
	.out_chars = stdio_usb_out_chars,
	.in_chars = stdio_usb_in_chars,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
	.crlf_enabled = true
#endif
};


#define BUF_SIZE 64
void cdc_task(void)
{
	static int len = 0;
	static char buf[BUF_SIZE];

	if(tud_cdc_connected())
	{
		// PC에서 데이터를 받았을 때
		if(tud_cdc_available())
		{
			uint32_t count = tud_cdc_read(buf + len, BUF_SIZE - len);
			len += count;
			if(buf[len - 1] == '\n' || len >= (BUF_SIZE - 1))
			{
				buf[len] = '\0'; // null-terminate the string
				kbd_proc_cmd((const uint8_t*)buf, len);
				len = 0; // 버퍼 초기화
			}
		}
	}
}

void cdc_init(void)
{
	stdio_set_driver_enabled(&stdio_usb_custom, true);
}


