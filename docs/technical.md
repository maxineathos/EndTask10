# EndTask10 — Technical documentation

## Injection

`EndTask10Launcher` finds `explorer.exe` via `CreateToolhelp32Snapshot`, opens it with `PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD`, writes the DLL path with `VirtualAllocEx` / `WriteProcessMemory`, and spawns a remote thread calling `LoadLibraryW`. The launcher exits immediately — no persistent background process.

A named event (`Global\EndTask10_Ready`) is created/reset before injection. The DLL signals it after `SetupUIA` + `InstallHooks` complete, so the launcher waits up to 3 seconds — no race condition.

## Ctrl+Shift+E detection

The DLL runs a dedicated polling loop (every 100ms) that checks the physical key state via `GetAsyncKeyState`. This works **regardless** of which window has focus, what modal menu is open, or any UI quirks — it reads the raw keyboard state from the input atom table. Rising-edge detection ensures the hotkey fires only once per press, not repeatedly while held.

- **Hover mode:** if no right-click target is set, `IdentifyTarget` is called from the current cursor position on-the-fly
- **Right-click mode:** target is pre-identified by the `WH_GETMESSAGE` hook on `WM_RBUTTONDOWN`, then used when the hotkey fires

A separate `WH_KEYBOARD_LL` hook handles only the **Esc** key (clears the current target + stops polling).

## Target identification

When you hover over a taskbar button or right-click it, the DLL uses `IUIAutomation::ElementFromPoint` (with `IAccessible` fallback) to get the accessible name of the element under the cursor. If the element has no name (e.g. a thumbnail preview child element), the DLL walks up the UIA control tree (up to 10 levels) to find a parent with a name, typically the taskbar button itself.

The name is then matched to a visible window via `EnumWindows` with a 3-level fallback:

1. **Full window title** match
2. **First word** of window title
3. **Process name** (via `CreateToolhelp32Snapshot`)

`explorer.exe` PID is filtered out at every level. If the target is stale or missing at hotkey time, the tool re-identifies from the current cursor position automatically.

## Kill logic

`TerminateProcess` is called on the target PID. Afterward, `KillProcessFamily()` enumerates child processes and same-name processes and terminates them too, handling restart daemons.

## DLL unloading

A named event (`Global\EndTask10_Unload`) is monitored by a dedicated wait thread inside the DLL. When signaled (via `EndTask10Launcher.exe /unload`), the thread posts `WM_QUIT` to the event thread, unhooks everything, calls `FreeLibraryAndExitThread`, and the DLL unloads cleanly — no need to restart `explorer.exe`.

## Why not visual injection?

Windows 10 taskbar running-app context menus are XAML-based (`MenuFlyout`), rendered entirely by the Windows.UI.Xaml framework with no HWND, no WinEvents, and no accessible WinRT hooks. XAML `MenuFlyout` cannot be extended from external code — which is why we use the keyboard shortcut approach instead.

## Logging

All hook activity, identification attempts, and kill operations are logged to `%TEMP%\EndTask10.log` for debugging.

## Building from source

### Prerequisites

- **Visual Studio 2022 / 2026** Build Tools with:
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

## Project structure

```
EndTask10/
├── EndTask10Launcher/       # DLL injector (CreateRemoteThread + LoadLibraryW)
├── EndTask10Hook/           # Injected DLL — all hook logic
├── docs/                    # Documentation
├── build.bat                # One-click build
├── setup.bat                # One-click install
├── uninstall.bat            # Clean removal
├── CMakeLists.txt           # Root CMake project
├── LICENSE                  # Custom non-commercial license
└── README.md                # Project overview
```

## Compatibility

- **Windows 10** x64 (build 19041+): Fully supported
- **Windows 11** x64: Fully compatible

Both versions use the same explorer.exe process model and taskbar infrastructure.
