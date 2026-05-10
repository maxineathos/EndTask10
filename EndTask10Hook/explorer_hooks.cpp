#include "explorer_hooks.h"
#include "logging.h"
#include <MinHook.h>

#define ENDTASK_MENU_ITEM_ID 0x1000

typedef int (WINAPI* TPMEx_t)(HMENU, UINT, int, int, HWND, LPTPMPARAMS);
typedef int (WINAPI* TPM_t)(HMENU, UINT, int, int, int, HWND, CONST RECT*);
typedef BOOL(WINAPI* GMIIW_t)(HMENU, UINT, BOOL, LPMENUITEMINFOW);
typedef int (WINAPI* GMIC_t)(HMENU);
typedef HWND(WINAPI* CWEx_t)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);

static TPMEx_t  g_AddrTPMEx = nullptr;
static TPM_t    g_AddrTPM   = nullptr;
static CWEx_t   g_AddrCWEx  = nullptr;
static TPMEx_t  g_TrampTPMEx = nullptr;
static TPM_t    g_TrampTPM   = nullptr;
static CWEx_t   g_TrampCWEx  = nullptr;
static GMIIW_t  g_GetMIIW = nullptr;
static GMIC_t   g_GetMIC  = nullptr;

static WNDPROC g_OrigOwnerProc = nullptr;
static HWND g_MenuOwner = nullptr;

static LRESULT CALLBACK OwnerProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_COMMAND && LOWORD(w) == ENDTASK_MENU_ITEM_ID)
    {
        LogMessage(L"End Task CLICKED!");
        MessageBoxW(h, L"End Task clicked!", L"EndTask10", MB_ICONINFORMATION);
        return 0;
    }
    return CallWindowProcW(g_OrigOwnerProc, h, m, w, l);
}

static BOOL IsTaskbarMenu(HMENU h)
{
    if (!h) return FALSE;
    int n = g_GetMIC(h);
    for (int i = 0; i < n; i++)
    {
        MENUITEMINFOW mii = { sizeof(mii), MIIM_STRING };
        if (!g_GetMIIW(h, i, TRUE, &mii)) continue;
        wchar_t b[256] = L"";
        if (mii.cch > 0 && mii.cch < 256) { mii.dwTypeData = b; mii.cch = 255; g_GetMIIW(h, i, TRUE, &mii); }
        if (wcsstr(b, L"Close window") || wcsstr(b, L"close window")) return TRUE;
    }
    return FALSE;
}

static void AddEndTask(HMENU h, HWND w)
{
    if (g_OrigOwnerProc) return;
    int n = g_GetMIC(h);
    for (int i = 0; i < n; i++)
    {
        MENUITEMINFOW mii = { sizeof(mii), MIIM_STRING };
        if (!g_GetMIIW(h, i, TRUE, &mii)) continue;
        wchar_t b[256] = L"";
        if (mii.cch > 0 && mii.cch < 256) { mii.dwTypeData = b; mii.cch = 255; g_GetMIIW(h, i, TRUE, &mii); }
        if (wcsstr(b, L"Close window") || wcsstr(b, L"close window"))
        {
            InsertMenuW(h, i, MF_BYPOSITION | MF_STRING, ENDTASK_MENU_ITEM_ID, L"End Task");
            LogMessage(L"Inserted 'End Task' before 'Close window'");
            g_MenuOwner = w;
            g_OrigOwnerProc = (WNDPROC)SetWindowLongPtrW(w, GWLP_WNDPROC, (LONG_PTR)OwnerProc);
            break;
        }
    }
}

static int WINAPI HookTPMEx(HMENU h, UINT f, int x, int y, HWND w, LPTPMPARAMS p)
{
    if (IsTaskbarMenu(h)) { LogMessage(L"TPMEx TASKBAR!"); AddEndTask(h, w); }
    return g_TrampTPMEx(h, f, x, y, w, p);
}

static int WINAPI HookTPM(HMENU h, UINT f, int x, int y, int r, HWND w, CONST RECT* p)
{
    if (IsTaskbarMenu(h)) { LogMessage(L"TPM TASKBAR!"); AddEndTask(h, w); }
    return g_TrampTPM(h, f, x, y, r, w, p);
}

// Hook CreateWindowExW to detect windows of interest
static int g_cwCount = 0;
static HWND WINAPI HookCWEx(DWORD es, LPCWSTR cls, LPCWSTR name, DWORD s,
    int x, int y, int w, int h, HWND p, HMENU m, HINSTANCE i, LPVOID lp)
{
    if (g_cwCount < 20 && cls && ((ULONG_PTR)cls > 0xFFFF))
    {
        if (wcsstr(cls, L"TaskList") || wcsstr(cls, L"Toolbar") ||
            wcsstr(cls, L"tooltip") || wcsstr(cls, L"XAML") ||
            wcsstr(cls, L"XCP") || wcsstr(cls, L"UserAdapter"))
        {
            LogMessage(L"CWEx[%d]: cls='%s'", ++g_cwCount, cls);
        }
    }
    return g_TrampCWEx(es, cls, name, s, x, y, w, h, p, m, i, lp);
}

void InitializeHooks()
{
    LogMessage(L"=== Initializing ===");

    HMODULE u = GetModuleHandleW(L"user32.dll");
    if (!u) { LogMessage(L"ERROR: no user32"); return; }

    g_AddrTPMEx = (TPMEx_t)GetProcAddress(u, "TrackPopupMenuEx");
    g_AddrTPM   = (TPM_t)GetProcAddress(u, "TrackPopupMenu");
    g_AddrCWEx  = (CWEx_t)GetProcAddress(u, "CreateWindowExW");
    g_GetMIIW   = (GMIIW_t)GetProcAddress(u, "GetMenuItemInfoW");
    g_GetMIC    = (GMIC_t)GetProcAddress(u, "GetMenuItemCount");

    LogMessage(L"TPMEx=%p TPM=%p CWEx=%p", g_AddrTPMEx, g_AddrTPM, g_AddrCWEx);

    MH_STATUS s = MH_Initialize();
    if (s != MH_OK) { LogMessage(L"MH_Init: %d", s); return; }

    if (g_AddrTPMEx)
    {
        s = MH_CreateHook(g_AddrTPMEx, HookTPMEx, (void**)&g_TrampTPMEx);
        if (s == MH_OK) MH_EnableHook(g_AddrTPMEx);
    }
    if (g_AddrTPM)
    {
        s = MH_CreateHook(g_AddrTPM, HookTPM, (void**)&g_TrampTPM);
        if (s == MH_OK) MH_EnableHook(g_AddrTPM);
    }
    if (g_AddrCWEx)
    {
        s = MH_CreateHook(g_AddrCWEx, HookCWEx, (void**)&g_TrampCWEx);
        if (s == MH_OK) MH_EnableHook(g_AddrCWEx);
    }

    LogMessage(L"=== Hooks initialized ===");
}

void UninitializeHooks()
{
    if (g_MenuOwner && g_OrigOwnerProc)
        SetWindowLongPtrW(g_MenuOwner, GWLP_WNDPROC, (LONG_PTR)g_OrigOwnerProc);

    if (g_AddrTPMEx) MH_DisableHook(g_AddrTPMEx);
    if (g_AddrTPM)   MH_DisableHook(g_AddrTPM);
    if (g_AddrCWEx)  MH_DisableHook(g_AddrCWEx);
    MH_Uninitialize();

    g_AddrTPMEx = nullptr;
    g_AddrTPM = nullptr;
    g_AddrCWEx = nullptr;
    g_TrampTPMEx = nullptr;
    g_TrampTPM = nullptr;
    g_TrampCWEx = nullptr;
    g_GetMIIW = nullptr;
    g_GetMIC = nullptr;
    g_OrigOwnerProc = nullptr;
    g_MenuOwner = nullptr;
}
