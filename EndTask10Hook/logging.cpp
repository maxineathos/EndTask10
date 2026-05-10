#include "logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static HANDLE g_hLogFile = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_LogCS;

void LogInit()
{
    InitializeCriticalSection(&g_LogCS);

    wchar_t szLogPath[MAX_PATH];
    if (!GetEnvironmentVariableW(L"TEMP", szLogPath, _countof(szLogPath)))
    {
        wcscpy_s(szLogPath, _countof(szLogPath), L"C:\\Temp");
    }
    wcscat_s(szLogPath, _countof(szLogPath), L"\\EndTask10Debug.log");

    g_hLogFile = CreateFileW(
        szLogPath, FILE_APPEND_DATA, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
    );
}

void LogCleanup()
{
    if (g_hLogFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hLogFile);
        g_hLogFile = INVALID_HANDLE_VALUE;
    }
    DeleteCriticalSection(&g_LogCS);
}

void LogMessage(const wchar_t* format, ...)
{
    if (g_hLogFile == INVALID_HANDLE_VALUE)
        return;

    EnterCriticalSection(&g_LogCS);

    va_list args;
    va_start(args, format);

    wchar_t buffer[1024];
    vswprintf_s(buffer, _countof(buffer), format, args);
    va_end(args);

    SYSTEMTIME st;
    GetSystemTime(&st);

    wchar_t szLine[2048];
    int cch = swprintf_s(szLine, _countof(szLine),
        L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] %s\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        buffer);

    if (cch > 0)
    {
        DWORD dwWritten;
        WriteFile(g_hLogFile, szLine, (DWORD)(cch * sizeof(wchar_t)), &dwWritten, nullptr);
    }

    LeaveCriticalSection(&g_LogCS);
}
