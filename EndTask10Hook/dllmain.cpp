#include <windows.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <UIAutomation.h>
#include <OleAcc.h>
#include <winsvc.h>
#include "logging.h"

#pragma comment(lib, "oleacc.lib")

static HHOOK g_hGetMsg = nullptr;
static HHOOK g_hKbd = nullptr;
static HWINEVENTHOOK g_hEvent = nullptr;
static HANDLE g_hEvtThread = nullptr;
static HANDLE g_hUnloadThread = nullptr;
static HANDLE g_hUnloadEvt = nullptr;
static volatile bool g_bRunning = false;
static volatile bool g_bPolling = false;
static volatile bool g_bPrevHotkeyDown = false;
static HMODULE g_hMod = nullptr;
static IUIAutomation* g_pUIA = nullptr;

static struct { DWORD pid; HWND hwnd; wchar_t name[256]; DWORD tick; } g_Target = {};
#define TARGET_TIMEOUT 8000

static bool IsExplorer()
{
    wchar_t path[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* n = wcsrchr(path, L'\\');
    return _wcsicmp(n ? n + 1 : path, L"explorer.exe") == 0;
}

static void ClearTarget()
{
    g_Target.pid = 0; g_Target.hwnd = nullptr;
    g_Target.name[0] = L'\0'; g_Target.tick = 0;
}

static bool IsTargetValid()
{
    return g_Target.pid && (GetTickCount() - g_Target.tick) <= TARGET_TIMEOUT;
}

struct FindData { const wchar_t* name; const wchar_t* firstWord; DWORD explorerPid; HWND hwnd; DWORD pid; };

static BOOL CALLBACK EnumMatchByName(HWND hwnd, LPARAM lp)
{
    FindData* fd = (FindData*)lp;
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t title[512] = L"";
    GetWindowTextW(hwnd, title, 512);
    if (!title[0]) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == fd->explorerPid) return TRUE;

    // Match: title contains accessible name, or name contains title
    if (StrStrIW(title, fd->name) || StrStrIW(fd->name, title)) {
        fd->hwnd = hwnd; fd->pid = pid; return FALSE;
    }
    // Fallback: title contains first word of accessible name (e.g. "Spotify")
    if (fd->firstWord && StrStrIW(title, fd->firstWord)) {
        fd->hwnd = hwnd; fd->pid = pid; return FALSE;
    }
    return TRUE;
}

static void GetFirstWord(const wchar_t* src, wchar_t* dst, int max)
{
    int i = 0;
    while (src[i] && src[i] != L' ' && src[i] != L'-' && i < max - 1) {
        dst[i] = src[i]; i++;
    }
    dst[i] = L'\0';
}

static void IdentifyTarget(POINT pt)
{
    ClearTarget();
    wchar_t accName[256] = L"";

    if (g_pUIA) {
        IUIAutomationElement* el = nullptr;
        if (g_pUIA->ElementFromPoint(pt, &el) == S_OK && el) {
            BSTR name = nullptr;
            if (el->get_CurrentName(&name) == S_OK && name && SysStringLen(name) > 0) {
                wcsncpy_s(accName, name, _TRUNCATE);
                SysFreeString(name);
            } else {
                if (name) SysFreeString(name);
                // Walk up the UIA tree (e.g. from thumbnail preview to taskbar button)
                IUIAutomationTreeWalker* walker = nullptr;
                if (g_pUIA->get_ControlViewWalker(&walker) == S_OK && walker) {
                    IUIAutomationElement* cur = el;
                    for (int d = 0; d < 10; d++) {
                        IUIAutomationElement* parent = nullptr;
                        if (walker->GetParentElement(cur, &parent) != S_OK || !parent)
                            break;
                        if (cur != el) cur->Release();
                        cur = parent;
                        BSTR pname = nullptr;
                        if (cur->get_CurrentName(&pname) == S_OK && pname && SysStringLen(pname) > 0) {
                            wcsncpy_s(accName, pname, _TRUNCATE);
                            SysFreeString(pname);
                            break;
                        }
                        if (pname) SysFreeString(pname);
                    }
                    if (cur != el) cur->Release();
                    walker->Release();
                }
            }
            el->Release();
        }
    }

    if (!accName[0]) {
        IAccessible* acc = nullptr;
        VARIANT var = { VT_I4 };
        if (AccessibleObjectFromPoint(pt, &acc, &var) == S_OK && acc) {
            BSTR name = nullptr;
            if (acc->get_accName(var, &name) == S_OK && name) {
                wcsncpy_s(accName, name, _TRUNCATE);
                SysFreeString(name);
            }
            acc->Release();
        }
    }

    if (accName[0]) {
        wcsncpy_s(g_Target.name, accName, _TRUNCATE);
        wchar_t firstWord[128] = L"";
        GetFirstWord(accName, firstWord, 128);
        DWORD explorerPid = GetCurrentProcessId();
        FindData fd = { g_Target.name, firstWord[0] && wcslen(firstWord) > 2 ? firstWord : nullptr, explorerPid, nullptr, 0 };
        EnumWindows(EnumMatchByName, (LPARAM)&fd);
        if (fd.hwnd && fd.pid) {
            g_Target.hwnd = fd.hwnd;
            g_Target.pid = fd.pid;
            GetWindowTextW(g_Target.hwnd, g_Target.name, 256);
        }
    }

    // Last resort: try process name matching
    if (!g_Target.hwnd && accName[0]) {
        wchar_t firstWord[128] = L"";
        GetFirstWord(accName, firstWord, 128);
        if (firstWord[0]) {
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32W pe = { sizeof(pe) };
                if (Process32FirstW(snap, &pe)) do {
                    if (pe.th32ProcessID == GetCurrentProcessId()) continue;
                    wchar_t exe[128] = L"";
                    wcscpy_s(exe, _countof(exe), pe.szExeFile);
                    wchar_t* dot = wcsrchr(exe, L'.');
                    if (dot) *dot = L'\0';
                    if (StrStrIW(exe, firstWord) || StrStrIW(firstWord, exe)) {
                        // Set PID immediately — TerminateProcess doesn't need an HWND
                        g_Target.pid = pe.th32ProcessID;
                        g_Target.hwnd = nullptr;
                        wcsncpy_s(g_Target.name, pe.szExeFile, _TRUNCATE);
                        // Try to find a visible window for a better name
                        HWND hw = FindWindowW(nullptr, nullptr);
                        while (hw) {
                            DWORD pid = 0;
                            GetWindowThreadProcessId(hw, &pid);
                            if (pid == pe.th32ProcessID && IsWindowVisible(hw)) {
                                GetWindowTextW(hw, g_Target.name, 256);
                                g_Target.hwnd = hw;
                                break;
                            }
                            hw = GetNextWindow(hw, GW_HWNDNEXT);
                        }
                        break;
                    }
                } while (Process32NextW(snap, &pe));
                CloseHandle(snap);
            }
        }
    }

    g_Target.tick = GetTickCount();
    LogMessage(L"Target: '%s' hwnd=%p pid=%lu", g_Target.name, g_Target.hwnd, g_Target.pid);
}

static void KillProcessFamily(DWORD mainPid)
{
    wchar_t mainExe[128] = L"";
    wchar_t mainExeNoExt[128] = L"";
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(snap, &pe)) do {
                if (pe.th32ProcessID == mainPid) {
                    wcscpy_s(mainExe, _countof(mainExe), pe.szExeFile);
                    wcscpy_s(mainExeNoExt, _countof(mainExeNoExt), pe.szExeFile);
                    wchar_t* dot = wcsrchr(mainExeNoExt, L'.');
                    if (dot) *dot = L'\0';
                    break;
                }
            } while (Process32NextW(snap, &pe));
            CloseHandle(snap);
        }
    }

    if (!mainExe[0]) return;

    // Collect PIDs to kill: all children of mainPid + all same-name processes
    DWORD pids[512];
    int count = 0;
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(snap, &pe)) do {
                if (pe.th32ProcessID == mainPid || pe.th32ProcessID == GetCurrentProcessId())
                    continue;
                if (pe.th32ParentProcessID == mainPid || _wcsicmp(pe.szExeFile, mainExe) == 0) {
                    if (count < 512) pids[count++] = pe.th32ProcessID;
                }
            } while (Process32NextW(snap, &pe));
            CloseHandle(snap);
        }
    }

    for (int i = 0; i < count; i++) {
        HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, pids[i]);
        if (hp) {
            TerminateProcess(hp, 1);
            CloseHandle(hp);
            LogMessage(L"Killed family: pid=%lu", pids[i]);
        }
    }

    // Try to stop related services (prevents auto-restart, e.g. Steam Client Service)
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
    if (scm) {
        DWORD bufSize = 0, needed = 0, count2 = 0;
        EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_ACTIVE,
            nullptr, 0, &needed, &count2, nullptr, nullptr);
        if (GetLastError() == ERROR_MORE_DATA) {
            bufSize = needed;
            ENUM_SERVICE_STATUS_PROCESSW* buf = (ENUM_SERVICE_STATUS_PROCESSW*)HeapAlloc(GetProcessHeap(), 0, bufSize);
            if (buf) {
                if (EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_ACTIVE,
                    (LPBYTE)buf, bufSize, &needed, &count2, nullptr, nullptr)) {
                    for (DWORD i = 0; i < count2; i++) {
                        // Match by display name containing exe name (e.g. "Steam Client Service" -> "Steam")
                        if (StrStrIW(buf[i].lpDisplayName, mainExeNoExt)) {
                            SC_HANDLE svc = OpenServiceW(scm, buf[i].lpServiceName,
                                SERVICE_STOP | SERVICE_QUERY_STATUS);
                            if (svc) {
                                SERVICE_STATUS ss;
                                if (ControlService(svc, SERVICE_CONTROL_STOP, &ss)) {
                                    LogMessage(L"Stopped service: %s", buf[i].lpDisplayName);
                                }
                                CloseServiceHandle(svc);
                            }
                        }
                    }
                }
                HeapFree(GetProcessHeap(), 0, buf);
            }
        }
        CloseServiceHandle(scm);
    }
}

static void ExecuteKill()
{
    if (!IsTargetValid()) {
        LogMessage(L"Target stale (pid=%lu age=%lums), re-identifying...", g_Target.pid, g_Target.tick ? (GetTickCount() - g_Target.tick) : 0);
        POINT curPt;
        GetCursorPos(&curPt);
        IdentifyTarget(curPt);
    }
    if (IsTargetValid()) {
        LogMessage(L"Killing '%s' pid=%lu", g_Target.name, g_Target.pid);
        HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, g_Target.pid);
        if (hp) { TerminateProcess(hp, 1); CloseHandle(hp); LogMessage(L"Killed main"); }
        else LogMessage(L"OpenProcess failed: %lu", GetLastError());
        KillProcessFamily(g_Target.pid);
        ClearTarget();
    } else {
        LogMessage(L"No valid target after re-identification");
    }
}

static LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
        if (kb->vkCode == VK_ESCAPE && g_Target.pid) { ClearTarget(); g_bPolling = FALSE; }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

static LRESULT CALLBACK GetMsgHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && wParam == PM_REMOVE) {
        MSG* msg = (MSG*)lParam;
        if (msg->message == WM_RBUTTONDOWN) {
            wchar_t cls[64] = L"";
            GetClassNameW(msg->hwnd, cls, 64);
            if (wcsstr(cls, L"MSTask") || wcsstr(cls, L"Shell_Tray") ||
                wcsstr(cls, L"ReBar") || wcsstr(cls, L"WorkerW")) {
                LogMessage(L"Right-click on %s (%d,%d)", cls, msg->pt.x, msg->pt.y);
                IdentifyTarget(msg->pt);
                if (g_Target.pid) g_bPolling = TRUE;
            }
        }
        else if (msg->message == WM_LBUTTONDOWN && g_Target.pid) {
            wchar_t cls[64] = L"";
            GetClassNameW(msg->hwnd, cls, 64);
            if (wcsstr(cls, L"MSTask") || wcsstr(cls, L"Shell_Tray") ||
                wcsstr(cls, L"ReBar") || wcsstr(cls, L"WorkerW") || wcsstr(cls, L"Desktop")) {
                DWORD age = g_Target.tick ? (GetTickCount() - g_Target.tick) : 0;
                LogMessage(L"Left-click on %s age=%lums (ignored)", cls, age);
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

static void SetupUIA()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
        IID_IUIAutomation, (void**)&g_pUIA);
}

static void CleanupUIA()
{
    if (g_pUIA) { g_pUIA->Release(); g_pUIA = nullptr; }
    CoUninitialize();
}

static void InstallHooks()
{
    DWORD tid = GetWindowThreadProcessId(FindWindowW(L"Shell_TrayWnd", nullptr), nullptr);
    if (tid) {
        g_hGetMsg = SetWindowsHookExW(WH_GETMESSAGE, GetMsgHookProc, nullptr, tid);
        LogMessage(L"WH_GETMESSAGE (tid=%lu): %p", tid, g_hGetMsg);
    }
    g_hKbd = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
    LogMessage(L"WH_KEYBOARD_LL: %p", g_hKbd);
}

static DWORD WINAPI EventThreadProc(LPVOID)
{
    LogMessage(L"EventThread started (tid=%lu)", GetCurrentThreadId());
    SetupUIA();
    InstallHooks();
    g_hEvent = SetWinEventHook(EVENT_SYSTEM_MENUSTART, EVENT_SYSTEM_MENUPOPUPEND,
        nullptr, [](HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {},
        0, 0, WINEVENT_OUTOFCONTEXT);

    HANDLE hReady = CreateEventW(nullptr, TRUE, FALSE, L"Global\\EndTask10_Ready");
    SetEvent(hReady);
    CloseHandle(hReady);

    while (g_bRunning)
    {
        DWORD wait = MsgWaitForMultipleObjects(1, &g_hUnloadEvt, FALSE, 100, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0) break;

        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_bRunning = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (g_bPolling && IsTargetValid()) {
            if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
                (GetAsyncKeyState('E') & 0x8000)) {
                LogMessage(L"Ctrl+Shift+E detected via poll");
                ExecuteKill();
                g_bPolling = FALSE;
            }
        }

        if (g_bPolling && !IsTargetValid())
            g_bPolling = FALSE;

        bool hotkeyDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                          (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
                          (GetAsyncKeyState('E') & 0x8000);

        if (hotkeyDown && !g_bPrevHotkeyDown) {
            LogMessage(L"Ctrl+Shift+E rising edge; polling=%d valid=%d", g_bPolling, IsTargetValid());
            if (g_bPolling && IsTargetValid()) {
                ExecuteKill();
                g_bPolling = FALSE;
            } else {
                POINT pt;
                GetCursorPos(&pt);
                IdentifyTarget(pt);
                if (IsTargetValid()) {
                    LogMessage(L"Kill from hover");
                    ExecuteKill();
                }
            }
        }
        g_bPrevHotkeyDown = hotkeyDown;
    }

    LogMessage(L"EventThread exiting");
    CleanupUIA();
    return 0;
}

static DWORD WINAPI UnloadThreadProc(LPVOID)
{
    LogMessage(L"UnloadThread waiting...");
    WaitForSingleObject(g_hUnloadEvt, INFINITE);
    LogMessage(L"Unload signaled, cleaning up");
    g_bRunning = false;
    if (g_hEvtThread) {
        PostThreadMessageW(GetThreadId(g_hEvtThread), WM_QUIT, 0, 0);
        WaitForSingleObject(g_hEvtThread, 1000);
        CloseHandle(g_hEvtThread); g_hEvtThread = nullptr;
    }
    if (g_hGetMsg) { UnhookWindowsHookEx(g_hGetMsg); g_hGetMsg = nullptr; }
    if (g_hKbd) { UnhookWindowsHookEx(g_hKbd); g_hKbd = nullptr; }
    if (g_hEvent) { UnhookWinEvent(g_hEvent); g_hEvent = nullptr; }
    CleanupUIA();
    LogMessage(L"Freeing library");
    FreeLibraryAndExitThread(g_hMod, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hMod);
        if (!IsExplorer()) break;
        g_hMod = hMod;
        LogInit();
        LogMessage(L"==============================================");
        LogMessage(L"EndTask10Hook loaded in explorer (hMod=%p)", hMod);
        g_hUnloadEvt = CreateEventW(nullptr, TRUE, FALSE, L"Global\\EndTask10_Unload");
        g_bRunning = true;
        g_hEvtThread = CreateThread(nullptr, 0, EventThreadProc, nullptr, 0, nullptr);
        g_hUnloadThread = CreateThread(nullptr, 0, UnloadThreadProc, nullptr, 0, nullptr);
        LogMessage(L"Load complete");
        break;
    case DLL_PROCESS_DETACH:
        if (!IsExplorer()) break;
        LogMessage(L"Detaching...");
        g_bRunning = false;
        if (g_hEvtThread) {
            PostThreadMessageW(GetThreadId(g_hEvtThread), WM_QUIT, 0, 0);
            WaitForSingleObject(g_hEvtThread, 1000);
            CloseHandle(g_hEvtThread);
        }
        if (g_hGetMsg) UnhookWindowsHookEx(g_hGetMsg);
        if (g_hKbd) UnhookWindowsHookEx(g_hKbd);
        if (g_hEvent) UnhookWinEvent(g_hEvent);
        CleanupUIA();
        if (g_hUnloadEvt) CloseHandle(g_hUnloadEvt);
        LogCleanup();
        break;
    }
    return TRUE;
}
