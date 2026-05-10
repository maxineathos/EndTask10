<p align="center">
  <img src="https://i.imgur.com/fnhroTI.png" alt="EndTask10 Logo" width="100%"/>
</p>

<p align="center">
  <i>Instantly end frozen apps. Just hover and press <b>CTRL + SHIFT + END</b></i>
</p>

<p align="center">
  <a href="https://github.com/maxineathos/EndTask10/releases"><img src="https://img.shields.io/github/v/release/maxineathos/EndTask10?color=brightgreen&label=Download&logo=github" alt="Download"/></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-Custom%20(Non--Commercial)-orange.svg" alt="License"/></a>
  <img src="https://img.shields.io/badge/Platform-Windows%2010%20|%2011%20x64-blue" alt="Platform"/>
  <img src="https://img.shields.io/github/downloads/maxineathos/EndTask10/total?color=brightgreen&label=Downloads" alt="Downloads"/>
</p>

---

## The idea

**Windows 11** has a built-in "End Task" option in the taskbar context menu — finally, a quick way to kill a frozen app without opening Task Manager.

<p align="center">
  <img src="https://cdn.neowin.com/news/images/uploaded/2023/05/1685119677_end_task.jpg" alt="Windows 11 End Task feature" width="100%"/>
  <br/>
  <i>Windows 11's built-in End Task — a step forward, but still two clicks away</i>
</p>

**Windows 10** doesn't have it. And even on Windows 11, you still need to right-click and navigate a menu — two extra steps when your app is frozen and every second counts.

EndTask10 makes it a **single keystroke**: hover your mouse over any taskbar icon and press **Ctrl+Shift+End**. That's it.

<p align="center">
  <a href="https://github.com/user-attachments/assets/0f614778-676c-4ed6-80d5-fdd3fc3f9d50">
    <img src="https://i.imgur.com/NMNbQNt.png" alt="EndTask10 Demo" width="100%"/>
  </a>
  <br/>
  <i>Click to watch the demo — Hover + Ctrl+Shift+End = done. No menus, no clicking, no waiting.</i>
</p>

No tray icons, no background processes, no popups. A tiny DLL injected into `explorer.exe` listens for the hotkey, identifies the app under your cursor, and kills it. The launcher exits immediately — nothing stays running except a lightweight hook inside a process already in memory.

---

## Quick start

### 1. Download

Get the latest build from [Releases](https://github.com/maxineathos/EndTask10/releases).

### 2. Run setup (as admin)

```cmd
setup.bat
```

Auto-elevates via UAC. Copies files to `%LOCALAPPDATA%\EndTask10\`, registers auto-start, and injects into `explorer.exe` immediately.

### 3. Use it

| Mode | How |
|------|-----|
| **Hover** 🖱️ | Mouse over any taskbar icon, press **Ctrl+Shift+End** |
| **Right-click** 🖱️ | Right-click a taskbar app, press **Ctrl+Shift+End** within 8s |

Press **Esc** to cancel a pending target.

### Uninstall

```cmd
uninstall.bat
```

---

## FAQ

### Does it work on Windows 10? ✅
Yes — that's the whole point. EndTask10 brings the "End Task" capability to Windows 10.

### Does it work on Windows 11? ✅
Yes. Works alongside the built-in "End Task" menu option as a faster keyboard shortcut.

### Why does my antivirus flag it? 🛡️
The tool injects a DLL via `CreateRemoteThread` — a technique also used by malware. It's a false positive. The source is fully open for review. Add an exception for `%LOCALAPPDATA%\EndTask10\`.

### Why does Steam restart after I kill it? 🔄
Steam has an internal watchdog. Even Windows 11's built-in "End Task" can't permanently stop it. EndTask10 still kills frozen Steam — it just restarts afterward.

### Why admin for setup? 🔒
To stop services that restart killed apps (Steam Client Service, etc.). The tool itself runs at user level.

---

## License

Custom non-commercial — see [LICENSE](LICENSE) for details. Commercial use requires permission.

---

<p align="center">
  <a href="docs/technical.md">Technical documentation →</a>
</p>
