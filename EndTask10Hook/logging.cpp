#include "logging.h"
#include <stdio.h>
#include <stdarg.h>

static HANDLE g_hLog = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_cs;

void LogInit()
{
    InitializeCriticalSection(&g_cs);
    wchar_t path[MAX_PATH];
    if (!GetEnvironmentVariableW(L"TEMP", path, MAX_PATH))
        wcscpy_s(path, L"C:\\Temp");
    wcscat_s(path, L"\\EndTask10.log");
    g_hLog = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void LogCleanup()
{
    if (g_hLog != INVALID_HANDLE_VALUE) { CloseHandle(g_hLog); g_hLog = INVALID_HANDLE_VALUE; }
    DeleteCriticalSection(&g_cs);
}

void LogMessage(const wchar_t* fmt, ...)
{
    if (g_hLog == INVALID_HANDLE_VALUE) return;
    EnterCriticalSection(&g_cs);
    va_list ap;
    va_start(ap, fmt);
    wchar_t buf[1024];
    vswprintf_s(buf, _countof(buf), fmt, ap);
    va_end(ap);
    SYSTEMTIME st;
    GetSystemTime(&st);
    wchar_t line[2048];
    int n = swprintf_s(line, _countof(line), L"[%02d:%02d:%02d.%03d] %s\r\n",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
    if (n > 0) { DWORD w; WriteFile(g_hLog, line, n * 2, &w, nullptr); }
    LeaveCriticalSection(&g_cs);
}
