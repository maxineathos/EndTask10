#pragma once
#include <windows.h>

void LogInit();
void LogCleanup();
void LogMessage(const wchar_t* format, ...);
