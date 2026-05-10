#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <UIAutomation.h>
#include <OleAcc.h>
#include "explorer_hooks.h"
#include "logging.h"

#pragma comment(lib, "oleacc.lib")

static HWINEVENTHOOK g_hEventHook = nullptr;
static HHOOK g_hGetMsgHook = nullptr;
static HHOOK g_hKeyboardHook = nullptr;
static HANDLE g_hEventThread = nullptr;
static volatile bool g_bRunning = false;

static IUIAutomation* g_pUIA = nullptr;
static IUIAutomationFocusChangedEventHandler* g_pFocusHandler = nullptr;

// Target app for End Task (set on right-click, consumed on hotkey)
static struct { DWORD pid; HWND hwnd; wchar_t name[256]; } g_Target = {};

class UiaFocusHandler : public IUIAutomationFocusChangedEventHandler
{
    LONG m_refCount = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == __uuidof(IUIAutomationFocusChangedEventHandler))
        {
            *ppv = static_cast<IUIAutomationFocusChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_refCount); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG c = InterlockedDecrement(&m_refCount);
        if (c == 0) delete this;
        return (ULONG)c;
    }
    HRESULT STDMETHODCALLTYPE HandleFocusChangedEvent(IUIAutomationElement* sender) override
    {
        return S_OK;
    }
};

static bool IsExplorerProcess()
{
    wchar_t path[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* name = wcsrchr(path, L'\\');
    name = name ? name + 1 : path;
    return _wcsicmp(name, L"explorer.exe") == 0;
}

struct FindData { const wchar_t* name; HWND hwnd; DWORD pid; };

static BOOL CALLBACK EnumWindowMatch(HWND hwnd, LPARAM lParam)
{
    FindData* fd = (FindData*)lParam;
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t title[512] = L"";
    if (!GetWindowTextW(hwnd, title, 512)) return TRUE;
    if (!title[0]) return TRUE;

    // Case-insensitive matching
    if (StrStrIW(title, fd->name) || StrStrIW(fd->name, title))
    {
        fd->hwnd = hwnd;
        GetWindowThreadProcessId(hwnd, &fd->pid);
        return FALSE;
    }

    // Try matching first word of accessible name against window title
    wchar_t firstWord[128] = L"";
    int pos = 0;
    while (fd->name[pos] && fd->name[pos] != L' ' && fd->name[pos] != L'-' && pos < 127)
    {
        firstWord[pos] = fd->name[pos];
        pos++;
    }
    firstWord[pos] = L'\0';
    if (firstWord[0] && wcslen(firstWord) > 2 && StrStrIW(title, firstWord))
    {
        fd->hwnd = hwnd;
        GetWindowThreadProcessId(hwnd, &fd->pid);
        return FALSE;
    }

    return TRUE;
}

static void IdentifyTargetFromPoint(POINT pt)
{
    g_Target.pid = 0;
    g_Target.hwnd = nullptr;
    g_Target.name[0] = L'\0';

    wchar_t accName[256] = L"";

    if (g_pUIA)
    {
        IUIAutomationElement* el = nullptr;
        if (g_pUIA->ElementFromPoint(pt, &el) == S_OK && el)
        {
            BSTR bName = nullptr;
            if (el->get_CurrentName(&bName) == S_OK && bName)
            {
                wcsncpy_s(accName, bName, _TRUNCATE);
                SysFreeString(bName);
            }

            // Try getting native HWND directly from UIA element
            UIA_HWND nativeHwnd = 0;
            if (el->get_CurrentNativeWindowHandle(&nativeHwnd) == S_OK && nativeHwnd)
            {
                g_Target.hwnd = (HWND)nativeHwnd;
                GetWindowThreadProcessId(g_Target.hwnd, &g_Target.pid);
                LogMessage(L"UIA native HWND: %p pid=%lu", g_Target.hwnd, g_Target.pid);
            }

            // Try getting PID directly from UIA element
            int rawPid = 0;
            if (!g_Target.pid && el->get_CurrentProcessId(&rawPid) == S_OK && rawPid > 0)
            {
                g_Target.pid = (DWORD)rawPid;
                LogMessage(L"UIA raw PID: %lu", g_Target.pid);
            }

            el->Release();
        }
    }

    // Fallback: classic accessibility
    if (accName[0] == L'\0')
    {
        IAccessible* pAcc = nullptr;
        VARIANT var = { VT_I4 };
        var.lVal = CHILDID_SELF;
        if (AccessibleObjectFromPoint(pt, &pAcc, &var) == S_OK && pAcc)
        {
            BSTR bName = nullptr;
            if (pAcc->get_accName(var, &bName) == S_OK && bName)
            {
                wcsncpy_s(accName, bName, _TRUNCATE);
                SysFreeString(bName);
            }
            pAcc->Release();
        }
    }

    // Find matching visible window by title (fallback if no native HWND from UIA)
    if (!g_Target.hwnd && accName[0])
    {
        wcsncpy_s(g_Target.name, accName, _TRUNCATE);
        FindData fd = { g_Target.name, nullptr, 0 };
        EnumWindows(EnumWindowMatch, (LPARAM)&fd);
        g_Target.hwnd = fd.hwnd;
        g_Target.pid = fd.pid;
    }

    LogMessage(L"Target: '%s' hwnd=%p pid=%lu", g_Target.name, g_Target.hwnd, g_Target.pid);
}

static LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
    {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
        // Ctrl+Shift+End to End Task
        if (kb->vkCode == 0x45 &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
            (GetAsyncKeyState(VK_SHIFT) & 0x8000))
        {
            LogMessage(L"Ctrl+Shift+E pressed!");
            if (g_Target.pid != 0)
            {
                LogMessage(L"Ending task: '%s' pid=%lu", g_Target.name, g_Target.pid);
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, g_Target.pid);
                if (hProc)
                {
                    TerminateProcess(hProc, 1);
                    CloseHandle(hProc);
                    LogMessage(L"Process terminated");
                }
                else
                {
                    LogMessage(L"OpenProcess failed: %lu", GetLastError());
                }
                g_Target.pid = 0;
                g_Target.hwnd = nullptr;
                g_Target.name[0] = L'\0';
            }
            else
            {
                LogMessage(L"No target set");
            }
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
    LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (event == EVENT_SYSTEM_MENUPOPUPSTART || event == EVENT_SYSTEM_MENUSTART)
    {
        wchar_t cls[64] = L"", txt[256] = L"";
        GetClassNameW(hwnd, cls, 64);
        GetWindowTextW(hwnd, txt, 256);
        LogMessage(L"MENU_EVENT(%d): cls='%s' txt='%s'", event, cls, txt);
    }
}

static LRESULT CALLBACK GetMsgHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && wParam == PM_REMOVE && IsExplorerProcess())
    {
        MSG* msg = (MSG*)lParam;
        if (msg->message == WM_RBUTTONDOWN)
        {
            wchar_t cls[64] = L"";
            GetClassNameW(msg->hwnd, cls, 64);
            if (wcsstr(cls, L"MSTask") || wcsstr(cls, L"Shell_Tray") ||
                wcsstr(cls, L"ReBar") || wcsstr(cls, L"WorkerW"))
            {
                LogMessage(L"Right-click on %s at %d,%d", cls, msg->pt.x, msg->pt.y);
                IdentifyTargetFromPoint(msg->pt);
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

static void SetupUIA()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    LogMessage(L"CoInitializeEx: hr=x%08x", hr);

    hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
        IID_IUIAutomation, (void**)&g_pUIA);
    LogMessage(L"CoCreateInstance UIA: hr=x%08x p=%p", hr, g_pUIA);

    if (g_pUIA)
    {
        g_pFocusHandler = new UiaFocusHandler();
        hr = g_pUIA->AddFocusChangedEventHandler(nullptr, g_pFocusHandler);
        LogMessage(L"AddFocusChangedEventHandler: hr=x%08x", hr);
    }
}

static void CleanupUIA()
{
    if (g_pUIA && g_pFocusHandler)
        g_pUIA->RemoveFocusChangedEventHandler(g_pFocusHandler);
    if (g_pFocusHandler) { g_pFocusHandler->Release(); g_pFocusHandler = nullptr; }
    if (g_pUIA) { g_pUIA->Release(); g_pUIA = nullptr; }
    CoUninitialize();
}

static void InstallHooks(HMODULE hMod)
{
    // WH_GETMESSAGE: detect right-clicks on taskbar (local hook, no DLL injection)
    DWORD mainTid = GetWindowThreadProcessId(FindWindowW(L"Shell_TrayWnd", nullptr), nullptr);
    if (mainTid)
    {
        g_hGetMsgHook = SetWindowsHookExW(WH_GETMESSAGE, GetMsgHookProc, nullptr, mainTid);
        LogMessage(L"WH_GETMESSAGE hook (tid=%lu): %p", mainTid, g_hGetMsgHook);
    }

    // WH_KEYBOARD_LL: detect Ctrl+Shift+End globally (runs on our thread, hMod=NULL = no DLL injection)
    g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
    LogMessage(L"WH_KEYBOARD_LL hook: %p", g_hKeyboardHook);
}

static DWORD WINAPI EventThreadProc(LPVOID lpParam)
{
    HMODULE hMod = (HMODULE)lpParam;
    LogMessage(L"EventThread started (tid=%lu)", GetCurrentThreadId());

    SetupUIA();
    InstallHooks(hMod);

    g_hEventHook = SetWinEventHook(
        EVENT_SYSTEM_MENUSTART, EVENT_SYSTEM_MENUPOPUPEND,
        hMod, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    LogMessage(L"Menu event hook: %p", g_hEventHook);

    MSG msg;
    while (g_bRunning && GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    LogMessage(L"EventThread exiting");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);

        if (!IsExplorerProcess())
            break;

        LogInit();
        LogMessage(L"======================================");
        LogMessage(L"EndTask10Hook.dll loaded (hModule=%p)", hModule);
        LogMessage(L"======================================");

        g_bRunning = true;
        g_hEventThread = CreateThread(nullptr, 0, EventThreadProc, hModule, 0, nullptr);
        LogMessage(L"EventThread: %p", g_hEventThread);

        InitializeHooks();
        LogMessage(L"DLL_PROCESS_ATTACH complete");
        break;
    }

    case DLL_PROCESS_DETACH:
    {
        if (!IsExplorerProcess())
            break;

        LogMessage(L"Unloading...");
        g_bRunning = false;
        if (g_hEventThread)
        {
            PostThreadMessageW(GetThreadId(g_hEventThread), WM_QUIT, 0, 0);
            WaitForSingleObject(g_hEventThread, 1000);
            CloseHandle(g_hEventThread);
        }
        if (g_hGetMsgHook) UnhookWindowsHookEx(g_hGetMsgHook);
        if (g_hKeyboardHook) UnhookWindowsHookEx(g_hKeyboardHook);
        if (g_hEventHook) UnhookWinEvent(g_hEventHook);
        CleanupUIA();
        UninitializeHooks();
        LogCleanup();
        break;
    }

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}

extern "C" __declspec(dllexport) void TestExport() {}
