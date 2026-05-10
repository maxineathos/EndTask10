/*
 * EndTask10Launcher - DLL Injector
 * 
 * Purpose:
 *   - Runs at Windows startup
 *   - Injects EndTask10Hook.dll into explorer.exe
 *   - Exits immediately
 *   - No persistent background process
 * 
 * Strategy:
 *   1. Find explorer.exe process
 *   2. Open handle with necessary permissions
 *   3. Allocate memory in explorer.exe address space
 *   4. Write DLL path into allocated memory
 *   5. Create remote thread executing LoadLibraryW
 *   6. Wait for thread completion
 *   7. Cleanup and exit
 */

#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Get absolute path of DLL relative to launcher
BOOL GetDLLPath(wchar_t* pDllPath, size_t cchPath)
{
    if (!GetModuleFileNameW(nullptr, pDllPath, (DWORD)cchPath))
    {
        wprintf(L"ERROR: GetModuleFileNameW failed\n");
        return FALSE;
    }

    // Remove executable name, keep directory
    wchar_t* pLastBackslash = wcsrchr(pDllPath, L'\\');
    if (!pLastBackslash)
    {
        wprintf(L"ERROR: Invalid module path\n");
        return FALSE;
    }

    // Build DLL path: same directory as launcher
    wcscpy_s(pLastBackslash + 1, cchPath - (pLastBackslash + 1 - pDllPath), L"EndTask10Hook.dll");
    return TRUE;
}

// Find explorer.exe process ID
DWORD FindExplorerPID()
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        wprintf(L"ERROR: CreateToolhelp32Snapshot failed\n");
        return 0;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnapshot, &pe32))
    {
        wprintf(L"ERROR: Process32FirstW failed\n");
        CloseHandle(hSnapshot);
        return 0;
    }

    DWORD dwExplorerPID = 0;

    do
    {
        if (_wcsicmp(pe32.szExeFile, L"explorer.exe") == 0)
        {
            dwExplorerPID = pe32.th32ProcessID;
            wprintf(L"Found explorer.exe (PID: %lu)\n", dwExplorerPID);
            break;
        }
    } while (Process32NextW(hSnapshot, &pe32));

    CloseHandle(hSnapshot);
    return dwExplorerPID;
}

// Inject DLL into target process
BOOL InjectDLL(DWORD dwPID, const wchar_t* pDllPath)
{
    wprintf(L"Injecting DLL into explorer.exe (PID: %lu)\n", dwPID);
    wprintf(L"DLL path: %s\n", pDllPath);

    // Open process with full access
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        dwPID
    );

    if (!hProcess)
    {
        wprintf(L"ERROR: OpenProcess failed (0x%lx)\n", GetLastError());
        return FALSE;
    }

    // Allocate memory in target process for DLL path
    size_t cbPath = (wcslen(pDllPath) + 1) * sizeof(wchar_t);
    void* pRemoteBuffer = VirtualAllocEx(hProcess, nullptr, cbPath, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!pRemoteBuffer)
    {
        wprintf(L"ERROR: VirtualAllocEx failed (0x%lx)\n", GetLastError());
        CloseHandle(hProcess);
        return FALSE;
    }

    wprintf(L"Allocated remote buffer at %p\n", pRemoteBuffer);

    // Write DLL path to remote buffer
    if (!WriteProcessMemory(hProcess, pRemoteBuffer, (void*)pDllPath, cbPath, nullptr))
    {
        wprintf(L"ERROR: WriteProcessMemory failed (0x%lx)\n", GetLastError());
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    wprintf(L"Wrote DLL path to remote buffer\n");

    // Get address of LoadLibraryW
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    LPTHREAD_START_ROUTINE pLoadLibraryW = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");

    if (!pLoadLibraryW)
    {
        wprintf(L"ERROR: Could not get LoadLibraryW address\n");
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    // Create remote thread to execute LoadLibraryW
    HANDLE hRemoteThread = CreateRemoteThread(
        hProcess,
        nullptr,
        0,
        pLoadLibraryW,
        pRemoteBuffer,
        0,
        nullptr
    );

    if (!hRemoteThread)
    {
        wprintf(L"ERROR: CreateRemoteThread failed (0x%lx)\n", GetLastError());
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    wprintf(L"Remote thread created, waiting for completion...\n");

    // Wait for remote thread to complete
    WaitForSingleObject(hRemoteThread, INFINITE);

    // Get thread exit code (should be non-zero if DLL loaded successfully)
    DWORD dwExitCode = 0;
    GetExitCodeThread(hRemoteThread, &dwExitCode);
    wprintf(L"Remote thread exit code: 0x%lx\n", dwExitCode);

    // Cleanup
    VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
    CloseHandle(hRemoteThread);
    CloseHandle(hProcess);

    if (!dwExitCode)
    {
        wprintf(L"ERROR: LoadLibraryW returned NULL, DLL load failed\n");
        return FALSE;
    }

    wprintf(L"SUCCESS: DLL injected successfully\n");
    return TRUE;
}

int wmain()
{
    wprintf(L"EndTask10Launcher - DLL Injector for explorer.exe\n");
    wprintf(L"============================================\n\n");

    // Get DLL path
    wchar_t szDllPath[MAX_PATH];
    if (!GetDLLPath(szDllPath, _countof(szDllPath)))
    {
        wprintf(L"ERROR: Failed to get DLL path\n");
        return 1;
    }

    // Find explorer.exe
    DWORD dwExplorerPID = FindExplorerPID();
    if (!dwExplorerPID)
    {
        wprintf(L"ERROR: Could not find explorer.exe\n");
        return 1;
    }

    // Inject DLL
    if (!InjectDLL(dwExplorerPID, szDllPath))
    {
        wprintf(L"ERROR: DLL injection failed\n");
        return 1;
    }

    wprintf(L"\nInjection complete. Exiting.\n");
    return 0;
}
