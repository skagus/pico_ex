#pragma once
#include <inttypes.h>
#ifndef __cplusplus
#define false			(0)
#define true			(!false)
#endif

#define LOG(str)		{printf("\t%s(%d), %s\n", __FUNCTION__, __LINE__, str);}
#ifndef BIT
#define BIT(x)			(1 << (x))
#endif
#ifndef NULL
#define NULL			((void*)0)
#endif
#define FF32			0xFFFFFFFF

typedef uint8_t			uint8;
typedef int8_t			int8;
typedef uint16_t		uint16;
typedef int16_t			int16;
typedef uint32_t		uint32;
typedef int32_t			int32;
typedef uint64_t		uint64;
typedef int64_t			int64;

typedef void (*Cbf)(uint8 tag, uint8 result);
typedef void (*CbFunc)(void* pReq, uint32 nResult);
