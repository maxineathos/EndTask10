# EndTask10 — End Task like Windows 11, on Windows 10

> Bring the Windows 11 **End Task** taskbar feature to Windows 10 with a simple keyboard shortcut.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE.txt)
![Platform](https://img.shields.io/badge/Platform-Windows%2010%20x64-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B17-purple)

---

## What is this?

Windows 11 has an **"End Task"** option in the taskbar right-click menu that lets you forcefully terminate a hung application. This feature is not available on Windows 10.

**EndTask10** brings the same functionality to Windows 10 by injecting a lightweight DLL into `explorer.exe` that hooks the taskbar right-click and lets you press **Ctrl+Shift+E** to kill the selected app.

![Demo](https://via.placeholder.com/600x300/1a1a2e/ffffff?text=Right-click+→+Ctrl%2BShift%2BE+→+Done)

## How it works

| Step | What happens |
|------|-------------|
| 1 | You **right-click** a running app on the taskbar |
| 2 | A DLL inside `explorer.exe` detects the click via a `WH_GETMESSAGE` hook and identifies the target app's PID using **UI Automation** |
| 3 | You press **Ctrl+Shift+E** |
| 4 | The DLL calls `TerminateProcess` on the target PID — the app is killed instantly |

No tray icons, no overlays, no custom UIs. Just the shortcut.

## Requirements

- **Windows 10** x64 (build 19041+)
- **No admin rights needed** — runs at user integrity level
- Works alongside the original taskbar menu (Pin, Close window, jump lists, etc.)

## Quick install

### 1. Download
Grab the latest build from [Releases](https://github.com/maxineathos/EndTask10/releases).

### 2. Run setup
```cmd
setup.bat
```

This will:
- Copy the files to `%LOCALAPPDATA%\EndTask10\`
- Add itself to **HKCU Run** (auto-injects on login)
- Inject into `explorer.exe` immediately

### 3. Use it
1. **Right-click** any running app on the taskbar
2. Press **Ctrl+Shift+E**

That's it. The app is terminated.

### Uninstall
```cmd
uninstall.bat
```

Removes all files and startup entries.

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
├── EndTask10Hook/           # Injected DLL — contains all hook logic
├── EndTask10Helper/         # Helper process (optional)
├── third_party/MinHook/     # MinHook – minimal API hooking library
├── build.bat                # One-click build
├── setup.bat                # One-click install
├── uninstall.bat            # Clean removal
└── CMakeLists.txt           # Root CMake project
```

---

## Technical overview

### Injection
The launcher finds `explorer.exe`, opens it with `PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD`, allocates memory for the DLL path, writes it, and spawns a remote thread calling `LoadLibraryW`. No permanent background process.

### Hooks inside explorer.exe

| Hook | Type | Purpose |
|------|------|---------|
| `WH_GETMESSAGE` | Local (thread) | Detects `WM_RBUTTONDOWN` on `MSTaskListWClass` |
| `WH_KEYBOARD_LL` | Global | Detects **Ctrl+Shift+E** from any process |
| `TrackPopupMenu` / `TrackPopupMenuEx` | MinHook | Adds "End Task" to the **taskbar background** classic Win32 menu |

### Target identification
When you right-click a taskbar button, the DLL uses `IUIAutomation::ElementFromPoint` to identify which window is under the cursor, then calls `get_CurrentNativeWindowHandle` + `GetWindowThreadProcessId` to get the target PID.

### Why not visual injection?
Windows 10 taskbar running-app context menus are XAML-based (`MenuFlyout`), rendered entirely by the Windows.UI.Xaml framework with no HWND, no WinEvents, and no accessible WinRT hooks. **XAML MenuFlyout cannot be extended from external code** — which is why we use the keyboard shortcut approach instead.

---

## Credits

- **[MinHook](https://github.com/TsudaKageyu/minhook)** by Tsuda Kageyu — Minimalistic x86/x64 API hooking library
- Inspired by the Windows 11 "End Task" taskbar feature

## License

This project is licensed under the MIT License — see [LICENSE.txt](third_party/MinHook/LICENSE.txt) for details.
