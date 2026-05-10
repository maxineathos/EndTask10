# EndTask10

> Kill taskbar apps instantly with **right-click + Ctrl+Shift+E** — no tray icon, no UI.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Platform](https://img.shields.io/badge/Platform-Windows%2010%20|%2011%20x64-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B17-purple)

---

## What is this?

Windows 11 ships an **"End Task"** option in the taskbar right-click menu that forcefully terminates a hung application. Windows 10 doesn't have it.

**EndTask10** brings the same functionality to Windows 10 (and works as an alternative keyboard shortcut on Windows 11 too). Right-click any running app on the taskbar, press **Ctrl+Shift+E**, and it's gone.

## How it works

| Step | What happens |
|------|-------------|
| 1 | You **right-click** a running app on the taskbar |
| 2 | A lightweight DLL injected into `explorer.exe` detects the click via a `WH_GETMESSAGE` hook and identifies the target app using **UI Automation** |
| 3 | You press **Ctrl+Shift+E** |
| 4 | The DLL calls `TerminateProcess` on the target PID — the app is killed instantly |

No tray icons, no overlays, no popups, no background processes.

## Requirements

- **Windows 10** x64 (build 19041+) **or Windows 11** x64
- **No admin rights needed** — runs at user integrity level
- Compatible with standard taskbar menus (pin, close, jump lists, etc.)

## Quick install

> **Antivirus note:** EndTask10 injects a DLL into `explorer.exe` using `CreateRemoteThread` — a technique also used by malware, so antivirus engines may flag it as a **false positive**. This is normal. Add an exception for `%LOCALAPPDATA%\EndTask10\` or the extracted folder if needed.
>
> **Note on auto-restart:** Some apps (e.g. Steam, Epic Games Launcher) use background services that detect when the main process is killed and restart it automatically. EndTask10 also tries to stop related services, but this may not work without administrator privileges. If an app keeps restarting, close it through its own interface instead.

### 1. Download

Get the latest build from [Releases](https://github.com/maxineathos/EndTask10/releases).

### 2. Run setup

```cmd
setup.bat
```

This will:
- Copy the files to `%LOCALAPPDATA%\EndTask10\`
- Register in **HKCU\Run** (auto-starts on login)
- Inject into `explorer.exe` immediately

### 3. Use it

1. **Right-click** any running app on the taskbar
2. Press **Ctrl+Shift+E**

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

### Hooks inside explorer.exe

| Hook | Type | Purpose |
|------|------|---------|
| `WH_GETMESSAGE` | Local (shell thread) | Detects `WM_RBUTTONDOWN` on `MSTaskListWClass` |
| `WH_KEYBOARD_LL` | Global low-level | Detects **Ctrl+Shift+E** from any process |

### Target identification

When you right-click a taskbar button, the DLL uses `IUIAutomation::ElementFromPoint` (with `IAccessible` fallback) to get the accessible name of the element under the cursor. It then matches that name to a visible window via `EnumWindows`, falling back to process name matching as a last resort. The target PID is stored and used when **Ctrl+Shift+E** is pressed.

If the target is stale or missing at hotkey time (e.g., cleared by a spurious event), the tool re-identifies from the current cursor position automatically.

### Unloading

A named event (`Global\EndTask10_Unload`) is monitored by a dedicated wait thread inside the DLL. When signaled (via `EndTask10Launcher.exe /unload`), the thread posts `WM_QUIT` to the event thread, unhooks everything, calls `FreeLibraryAndExitThread`, and the DLL unloads cleanly — no need to restart `explorer.exe`.

### Why not visual injection?

Windows 10 taskbar running-app context menus are XAML-based (`MenuFlyout`), rendered entirely by the Windows.UI.Xaml framework with no HWND, no WinEvents, and no accessible WinRT hooks. XAML `MenuFlyout` cannot be extended from external code — which is why we use the keyboard shortcut approach instead.

---

## Compatibility

- **Windows 10** x64 (build 19041+): Fully supported — this is the primary target
- **Windows 11** x64: Fully compatible — works identically, providing a keyboard shortcut alternative to the built-in "End Task" menu option

Both versions use the same explorer.exe process model and taskbar infrastructure.

## Credits

Inspired by the Windows 11 "End Task" taskbar feature.

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for details.
