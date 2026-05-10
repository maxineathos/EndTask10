#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

#define DLL_NAME L"EndTask10Hook.dll"
#define EVENT_NAME L"Global\\EndTask10_Unload"

static BOOL GetDLLPath(wchar_t* buf, size_t cch)
{
    if (!GetModuleFileNameW(nullptr, buf, (DWORD)cch)) return FALSE;
    wchar_t* p = wcsrchr(buf, L'\\');
    if (!p) return FALSE;
    wcscpy_s(p + 1, cch - (p + 1 - buf), DLL_NAME);
    return TRUE;
}

static DWORD FindExplorerPID()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) do {
        if (_wcsicmp(pe.szExeFile, L"explorer.exe") == 0) { pid = pe.th32ProcessID; break; }
    } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return pid;
}

static BOOL IsDLLLoaded(DWORD pid)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snap == INVALID_HANDLE_VALUE) return FALSE;
    MODULEENTRY32W me = { sizeof(me) };
    BOOL found = FALSE;
    if (Module32FirstW(snap, &me)) do {
        if (_wcsicmp(me.szModule, DLL_NAME) == 0) { found = TRUE; break; }
    } while (Module32NextW(snap, &me));
    CloseHandle(snap);
    return found;
}

BOOL UnloadDLL()
{
    HANDLE hEvt = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
    if (!hEvt) { wprintf(L"DLL not loaded (no unload event)\n"); return TRUE; }
    wprintf(L"Signaling unload...\n");
    SetEvent(hEvt);
    CloseHandle(hEvt);
    DWORD pid = FindExplorerPID();
    for (int i = 0; i < 25; i++) {
        if (!IsDLLLoaded(pid)) { wprintf(L"DLL unloaded\n"); return TRUE; }
        Sleep(200);
    }
    wprintf(L"Warning: DLL may still be loaded\n");
    return TRUE;
}

BOOL InjectDLL(DWORD pid, const wchar_t* dllPath)
{
    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) { wprintf(L"OpenProcess failed: %lu\n", GetLastError()); return FALSE; }

    size_t cb = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    void* rem = VirtualAllocEx(hProc, nullptr, cb, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!rem) { wprintf(L"VirtualAllocEx failed: %lu\n", GetLastError()); CloseHandle(hProc); return FALSE; }

    if (!WriteProcessMemory(hProc, rem, (void*)dllPath, cb, nullptr)) {
        wprintf(L"WriteProcessMemory failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, rem, 0, MEM_RELEASE); CloseHandle(hProc); return FALSE;
    }

    LPTHREAD_START_ROUTINE loadLib = (LPTHREAD_START_ROUTINE)
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, loadLib, rem, 0, nullptr);
    if (!hThread) {
        wprintf(L"CreateRemoteThread failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, rem, 0, MEM_RELEASE); CloseHandle(hProc); return FALSE;
    }

    WaitForSingleObject(hThread, 10000);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, rem, 0, MEM_RELEASE);
    CloseHandle(hProc);

    if (IsDLLLoaded(pid)) { wprintf(L"SUCCESS: DLL injected\n"); return TRUE; }
    wprintf(L"ERROR: DLL not loaded after injection\n");
    return FALSE;
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc > 1 && _wcsicmp(argv[1], L"/unload") == 0) {
        wprintf(L"EndTask10 - Unload\n\n");
        return UnloadDLL() ? 0 : 1;
    }

    wprintf(L"EndTask10 - Injector\n\n");

    // Try to stop services that auto-restart killed apps (only works if elevated)
    BOOL isElevated = FALSE;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION te = {};
        DWORD sz = 0;
        if (GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &sz))
            isElevated = te.TokenIsElevated;
        CloseHandle(hToken);
    }
    if (isElevated) {
        wprintf(L"Running elevated — stopping restart services...\n");
        const wchar_t* services[] = {
            L"Steam Client Service",
            L"Steam Client Service64",
            L"Epic Online Services",
            L"EpicGamesLauncher",
        };
        for (int i = 0; i < _countof(services); i++) {
            SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
            if (scm) {
                SC_HANDLE svc = OpenServiceW(scm, services[i], SERVICE_STOP | SERVICE_QUERY_STATUS);
                if (svc) {
                    SERVICE_STATUS ss;
                    if (ControlService(svc, SERVICE_CONTROL_STOP, &ss))
                        wprintf(L"  Stopped: %s\n", services[i]);
                    CloseServiceHandle(svc);
                }
                CloseServiceHandle(scm);
            }
        }
    }

    DWORD pid = FindExplorerPID();
    if (!pid) { wprintf(L"explorer.exe not found\n"); return 1; }
    wprintf(L"explorer.exe PID: %lu\n", pid);

    if (IsDLLLoaded(pid)) {
        wprintf(L"DLL already loaded, unloading...\n");
        UnloadDLL();
        pid = FindExplorerPID();
    }

    wchar_t path[MAX_PATH];
    if (!GetDLLPath(path, MAX_PATH)) { wprintf(L"Failed to get DLL path\n"); return 1; }

    if (!InjectDLL(pid, path)) return 1;
    wprintf(L"\nUse '%s /unload' to unload\n", argv[0]);
    return 0;
}
