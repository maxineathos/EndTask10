# EndTask10

> Kill taskbar apps instantly with **Ctrl+Shift+E** — just hover over the icon or right-click — no tray icon, no UI.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Platform](https://img.shields.io/badge/Platform-Windows%2010%20|%2011%20x64-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B17-purple)

---

## What is this?

Windows 11 ships an **"End Task"** option in the taskbar right-click menu that forcefully terminates a hung application. Windows 10 doesn't have it.

**EndTask10** brings the same functionality to Windows 10 (and works as an alternative keyboard shortcut on Windows 11 too). **Just hover** your mouse over any running app on the taskbar (or its thumbnail preview) and press **Ctrl+Shift+E** — the app is killed instantly. No right-click needed.

## How it works

### Method 1: Hover (primary)

| Step | What happens |
|------|-------------|
| 1 | You **hover** your mouse over a running app on the taskbar (icon or thumbnail preview) |
| 2 | Press **Ctrl+Shift+E** |
| 3 | A lightweight DLL injected into `explorer.exe` identifies the app via **UI Automation** at the cursor position |
| 4 | The DLL calls `TerminateProcess` on the target PID — the app is killed instantly |

### Method 2: Right-click

| Step | What happens |
|------|-------------|
| 1 | You **right-click** a running app on the taskbar |
| 2 | The DLL detects the click via a `WH_GETMESSAGE` hook and pre-identifies the target |
| 3 | You press **Ctrl+Shift+E** (now or up to 8 seconds later) |
| 4 | The DLL calls `TerminateProcess` on the target PID — the app is killed instantly |

Both methods work seamlessly. No tray icons, no overlays, no popups, no background processes.

## Requirements

- **Windows 10** x64 (build 19041+) **or Windows 11** x64
- **No admin rights needed** — runs at user integrity level
- Compatible with standard taskbar menus (pin, close, jump lists, etc.)

## Quick install

> **Antivirus note:** EndTask10 injects a DLL into `explorer.exe` using `CreateRemoteThread` — a technique also used by malware, so antivirus engines may flag it as a **false positive**. This is normal. Add an exception for `%LOCALAPPDATA%\EndTask10\` or the extracted folder if needed.
>
> **Note on auto-restart:** Some apps (e.g. Steam) have built-in watchdog mechanisms that restart them no matter how you kill them. Even so, the tool is still useful as an **anti-crash** solution: if an app freezes and you can't close it normally, hover over its taskbar icon and press **Ctrl+Shift+E** to terminate it and get unstuck — even if it restarts, at least the frozen state is gone.

### 1. Download

Get the latest build from [Releases](https://github.com/maxineathos/EndTask10/releases).

### 2. Run setup (as administrator)

```cmd
setup.bat
```

The script will auto-elevate via UAC — accept the prompt. This is needed to stop services that may otherwise restart killed apps.

This will:
- Copy the files to `%LOCALAPPDATA%\EndTask10\`
- Register in **HKCU\Run** (auto-starts on login)
- Inject into `explorer.exe` immediately

### 3. Use it

**Hover mode (recommended):**
1. **Hover** your mouse over any running app on the taskbar (icon or thumbnail preview)
2. Press **Ctrl+Shift+E** — the app dies immediately

**Right-click mode (alternative):**
1. **Right-click** any running app on the taskbar (opens the jump list)
2. Press **Ctrl+Shift+E** within 8 seconds — the app dies

> **Tip:** Press **Esc** at any time to clear the current target (if you accidentally identified the wrong app).

### Uninstall

```cmd
uninstall.bat
```

Removes all files, unloads the DLL, and deletes the startup entry.

---

## Building from source

### Prerequisites

- **Visual Studio 2022 / 2026** Build Tools (or full VS) with:
  - MSVC v143+ x64 toolchain
  - Windows 10 SDK (10.0.19041+)
- **CMake** 3.20+

### Build

```cmd
build.bat
```

Or manually:

```cmd
cmake -G "Visual Studio 18 2026" -A x64 -B build
cmake --build build --config Release
```

Output goes to `build\bin\Release\`.

### Project structure

```
EndTask10/
├── EndTask10Launcher/       # DLL injector (CreateRemoteThread + LoadLibraryW)
├── EndTask10Hook/           # Injected DLL — all hook logic
├── build.bat                # One-click build
├── setup.bat                # One-click install
├── uninstall.bat            # Clean removal
└── CMakeLists.txt           # Root CMake project
```

---

## Technical overview

### Injection

`EndTask10Launcher` finds `explorer.exe` via `CreateToolhelp32Snapshot`, opens it with `PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD`, writes the DLL path with `VirtualAllocEx` / `WriteProcessMemory`, and spawns a remote thread calling `LoadLibraryW`. The launcher exits immediately — no persistent background process.

### Ctrl+Shift+E detection

The DLL runs a dedicated polling loop (every 100ms) that checks the physical key state via `GetAsyncKeyState`. This works **regardless** of which window has focus, what modal menu is open, or any UI quirks — it reads the raw keyboard state from the input atom table. Rising-edge detection ensures the hotkey fires only once per press, not repeatedly while held.

- **Hover mode:** if no right-click target is set, `IdentifyTarget` is called from the current cursor position on-the-fly
- **Right-click mode:** target is pre-identified by the `WH_GETMESSAGE` hook on `WM_RBUTTONDOWN`, then used when the hotkey fires

A separate `WH_KEYBOARD_LL` hook handles only the **Esc** key (clears the current target).

### Target identification

When you hover over a taskbar button or right-click it, the DLL uses `IUIAutomation::ElementFromPoint` (with `IAccessible` fallback) to get the accessible name of the element under the cursor. If the element has no name (e.g. a thumbnail preview child element), the DLL walks up the UIA control tree (up to 10 levels) to find a parent with a name, typically the taskbar button itself.

The name is then matched to a visible window via `EnumWindows`, falling back to process name matching as a last resort. The target PID is stored and used when **Ctrl+Shift+E** is pressed.

If the target is stale or missing at hotkey time, the tool re-identifies from the current cursor position automatically.

### Unloading

A named event (`Global\EndTask10_Unload`) is monitored by a dedicated wait thread inside the DLL. When signaled (via `EndTask10Launcher.exe /unload`), the thread posts `WM_QUIT` to the event thread, unhooks everything, calls `FreeLibraryAndExitThread`, and the DLL unloads cleanly — no need to restart `explorer.exe`.

### Why not visual injection?

Windows 10 taskbar running-app context menus are XAML-based (`MenuFlyout`), rendered entirely by the Windows.UI.Xaml framework with no HWND, no WinEvents, and no accessible WinRT hooks. XAML `MenuFlyout` cannot be extended from external code — which is why we use the keyboard shortcut approach instead.

---

## Compatibility

- **Windows 10** x64 (build 19041+): Fully supported — this is the primary target
- **Windows 11** x64: Fully compatible — works identically, providing a keyboard shortcut alternative to the built-in "End Task" menu option

Both versions use the same explorer.exe process model and taskbar infrastructure.

## FAQ

### How do I use it? Hover or right-click?

Both work. **Hover** is the primary mode: just move your mouse over the taskbar icon (or its thumbnail preview) and press **Ctrl+Shift+E** — no clicking needed. The right-click mode is useful if you want to pre-select a target and take your time (up to 8 seconds) before pressing the hotkey.

### Why does Steam restart after I kill it?

Steam has an internal watchdog mechanism that detects when its process is terminated, regardless of how it was killed (Task Manager, EndTask10, or even Windows 11's built-in "End Task"). It's a deliberate design choice by Valve. The tool still works as an **anti-crash** — if Steam freezes, you can kill it to get unstuck, even if it restarts afterward.

### Why does my antivirus flag EndTask10?

EndTask10 injects a DLL into `explorer.exe` using `CreateRemoteThread` — a technique also used by malware. This triggers false positives. Add an exception for `%LOCALAPPDATA%\EndTask10\` or the extracted folder. The source code is fully open for review.

### Does EndTask10 work on Windows 11?

Yes. It works on both Windows 10 and Windows 11. On Windows 11 it acts as an alternative keyboard shortcut alongside the built-in "End Task" menu option.

### Why does it need administrator privileges?

The setup requests admin to stop background services that restart killed apps (e.g. Steam Client Service, Epic Online Services). The tool itself works without admin — only service stopping requires elevation.

### Does EndTask10 run in the background?

No. The launcher injects the DLL and exits immediately. A small hook DLL stays loaded inside `explorer.exe` listening for right-clicks, hover identification, and the keyboard shortcut. No tray icon, no visible process.

## Credits

Inspired by the Windows 11 "End Task" taskbar feature.

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for details.
