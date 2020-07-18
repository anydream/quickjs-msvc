#pragma once

#include <stdint.h>

//////////////////////////////////////////////////////////////////////////
void Win32ConsoleOutput(int isErr, const char *str, uint32_t sz);
int Win32ConsoleInput(char *str, uint32_t sz);
