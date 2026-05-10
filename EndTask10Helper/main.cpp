/*
 * EndTask10Helper - Process Termination Helper
 * 
 * For Milestone 1: Just receives PID and shows a MessageBox
 * No actual process termination yet
 * 
 * Usage: EndTask10Helper.exe <PID>
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2)
    {
        MessageBoxW(nullptr, L"Usage: EndTask10Helper.exe <PID>", L"EndTask10Helper", MB_ICONINFORMATION);
        return 1;
    }

    DWORD dwPID = (DWORD)_wtoi(argv[1]);

    wchar_t szMessage[256];
    swprintf_s(szMessage, _countof(szMessage), L"EndTask10Helper called with PID: %lu\n\nMilestone 1: Just showing this message.\nNext: Implement graceful close.", dwPID);

    MessageBoxW(nullptr, szMessage, L"EndTask10Helper - Milestone 1", MB_ICONINFORMATION);

    return 0;
}
