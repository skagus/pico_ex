#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cli.h"

//////////////////////////////////
#define LEN_LINE            (64)
#define MAX_CMD_COUNT       (32)
#define MAX_ARG_TOKEN       (8)

#define COUNT_LINE_BUF       (8)

#define PUTCHAR(x)				{putchar(x);}

/**
 * history.
*/
char gaHistBuf[COUNT_LINE_BUF][LEN_LINE];
uint8_t gnPrvHist;
uint8_t gnHistRef;

typedef struct _CmdInfo
{
	char* szCmd;
	CmdHandler pHandle;
} CmdInfo;

CmdInfo gaCmds[MAX_CMD_COUNT];
uint8_t gnCmds;

void cli_RunCmd(char* szCmdLine);

void cli_CmdHelp(uint8_t argc, char* argv[])
{
	printf("Version: %s %s\n", __DATE__, __TIME__);
	if(argc > 1)
	{
		int32_t nNum = atoi(argv[1]);
		printf("help with %lX\n", nNum);
		char* aCh = (char*)&nNum;
		printf("help with %02X %02X %02X %02X\n",
			   aCh[0], aCh[1], aCh[2], aCh[3]);
	}
	else
	{
		for(uint8_t nIdx = 0; nIdx < gnCmds; nIdx++)
		{
			printf("%d: %s\n", nIdx, gaCmds[nIdx].szCmd);
		}
	}
}

void cli_CmdHistory(uint8_t argc, char* argv[])
{
	if(argc > 1) // Run the indexed command.
	{
		uint8_t nSlot = atoi(argv[1]);
		if(nSlot < COUNT_LINE_BUF)
		{
			if(strlen(gaHistBuf[nSlot]) > 0)
			{
				cli_RunCmd(gaHistBuf[nSlot]);
			}
		}
	}
	else // 
	{
		uint8_t nIdx = gnPrvHist;
		do
		{
			nIdx = (nIdx + 1) % COUNT_LINE_BUF;
			if(strlen(gaHistBuf[nIdx]) > 0)
			{
				printf("%2d> %s\n", nIdx, gaHistBuf[nIdx]);
			}

		} while(nIdx != gnPrvHist);
	}
}

void lb_NewEntry(char* szCmdLine)
{
	if(0 != strcmp(gaHistBuf[gnPrvHist], szCmdLine))
	{
		gnPrvHist = (gnPrvHist + 1) % COUNT_LINE_BUF;
		strcpy(gaHistBuf[gnPrvHist], szCmdLine);
	}
	gnHistRef = (gnPrvHist + 1) % COUNT_LINE_BUF;
}

uint8_t lb_GetNextEntry(bool bInc, char* szCmdLine)
{
	uint8_t nStartRef = gnHistRef;
	szCmdLine[0] = 0;
	int nGab = (true == bInc) ? 1 : -1;
	do
	{
		gnHistRef = (gnHistRef + COUNT_LINE_BUF + nGab) % COUNT_LINE_BUF;
		if(strlen(gaHistBuf[gnHistRef]) > 0)
		{
			strcpy(szCmdLine, gaHistBuf[gnHistRef]);
			return strlen(szCmdLine);
		}
	} while(nStartRef != gnHistRef);
	return 0;
}

uint32_t cli_Token(char* aTok[], char* pCur)
{
	uint32_t nIdx = 0;
	if(0 == *pCur) return 0;
	while(1)
	{
		while(' ' == *pCur) // remove front space.
		{
			pCur++;
		}
		aTok[nIdx] = pCur;
		nIdx++;
		while((' ' != *pCur) && (0 != *pCur)) // remove back space
		{
			pCur++;
		}
		if(0 == *pCur) return nIdx;
		*pCur = 0;
		pCur++;
	}
}

void cli_RunCmd(char* szCmdLine)
{
	char* aTok[MAX_ARG_TOKEN];
	uint8_t nCnt = cli_Token(aTok, szCmdLine);
	bool bExecute = false;
	if(nCnt > 0)
	{
		for(uint8_t nIdx = 0; nIdx < gnCmds; nIdx++)
		{
			if(0 == strcmp(aTok[0], gaCmds[nIdx].szCmd))
			{
				gaCmds[nIdx].pHandle(nCnt, aTok);
				bExecute = true;
				break;
			}
		}
	}

	if(false == bExecute)
	{
		printf("Unknown command: %s\n", szCmdLine);
	}
}

void cli_PushRcv(char nCh)
{
	static char aLine[LEN_LINE];
	static uint8_t nLen = 0;

	if(' ' <= nCh && nCh <= '~')
	{
		PUTCHAR(nCh);
		aLine[nLen] = nCh;
		nLen++;
	}
	else if(('\n' == nCh) || ('\r' == nCh))
	{
		if(nLen > 0)
		{
			aLine[nLen] = 0;
			printf("\n");
			lb_NewEntry(aLine);
			cli_RunCmd(aLine);
			nLen = 0;
		}
		printf("\n$> ");
	}
	else if(0x08 == nCh) // backspace, DEL
	{
		if(nLen > 0)
		{
			PUTCHAR('\b');
			PUTCHAR('\b');
			nLen--;
		}
	}
#if 0 // Adv.
	else if(0x1B == nCh) // Escape sequence.
	{
		char nCh2, nCh3;
		while(0 == UART_RxByte(&nCh2, SEC(1)));
		if(0x5B == nCh2) // direction.
		{
			while(0 == UART_RxByte(&nCh3, SEC(1)));
			if(0x41 == nCh3) // up.
			{
				nLen = lb_GetNextEntry(false, aLine);
				printf("\r                          \r->");
				if(nLen > 0) printf(aLine);
			}
			else if(0x42 == nCh3) // down.
			{
				nLen = lb_GetNextEntry(true, aLine);
				printf("\r                          \r+>");
				if(nLen > 0) printf(aLine);
			}
		}
	}
#endif
	else
	{
		printf("~ %X\n", nCh);
	}
}


/////////////////////
uint32_t CLI_GetInt(char* szStr)
{
	uint32_t nNum;
	char* pEnd;
	uint8_t nLen = strlen(szStr);
	if((szStr[0] == '0') && ((szStr[1] == 'b') || (szStr[1] == 'B'))) // Binary.
	{
		nNum = strtoul(szStr + 2, &pEnd, 2);
	}
	else
	{
		nNum = (uint32_t)strtoul(szStr, &pEnd, 0);
	}

	if((pEnd - szStr) != nLen)
	{
		nNum = NOT_NUMBER;
	}
	return nNum;
}

void CLI_Register(char* szCmd, CmdHandler pHandle)
{
	if(gnCmds >= MAX_CMD_COUNT)
	{
		printf("CLI_Register: Full\n");
		return;
	}

	gaCmds[gnCmds].szCmd = szCmd;
	gaCmds[gnCmds].pHandle = pHandle;
	gnCmds++;
}


///////////////////////
#define STK_SIZE	(1024 * 4)	// DW.

CliHandler CLI_Init(void)
{
//	setvbuf(stdout, NULL, _IONBF, 0);
	CLI_Register("help", cli_CmdHelp);
	CLI_Register("hist", cli_CmdHistory);
	return cli_PushRcv;
}

