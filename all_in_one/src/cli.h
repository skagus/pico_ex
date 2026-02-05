#pragma once

#include <stdint.h>
#include <string.h>

#define NOT_NUMBER			(0x7FFFFFFF)

typedef void (*CmdHandler)(uint8_t argc, char* argv[]);
typedef void (*CliHandler)(char input);

#define SAME_STR(const_name, input_name) (0 == strncmp(const_name, input_name, strlen(const_name)))


CliHandler CLI_Init(void);
void CLI_Register(char* szCmd, CmdHandler pHandle);
uint32_t CLI_GetInt(char* szStr);
