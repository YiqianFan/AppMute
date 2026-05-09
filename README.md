# AppMute

Windows per-application audio mute tool. Toggle any app's mute state via global hotkeys without touching system volume.

## Features

- Mute/unmute specific applications independently
- Global hotkeys work regardless of which window is focused
- System tray background operation
- Config hot-reload, no restart needed
- Process name matching, case-insensitive

## Quick Start

1. Download from [Releases](https://github.com/YiqianFan/AppMute/releases)
2. Place `app-mute.exe` and `config.toml` in the same directory
3. Edit `config.toml`:

```toml
[hotkeys]
"f1" = "chrome"
"ctrl+alt+1" = "spotify"
"ctrl+alt+2" = "discord"
```

4. Run `app-mute.exe` — minimized to system tray
5. Right-click tray icon to exit

## Technical Overview

### Audio Control

Windows Core Audio API (`IAudioSessionManager2` / `ISimpleAudioVolume`). Per-session mute, not device-level — only the target app is affected.

Key interfaces:
- `IMMDeviceEnumerator` — default audio endpoint
- `IAudioSessionManager2` — session enumerator
- `IAudioSessionEnumerator` — iterate active sessions
- `ISimpleAudioVolume` — get/set mute state

### Hotkey System

Win32 `RegisterHotKey` with a hidden message-only window. `MOD_NOREPEAT` prevents hold-to-repeat. Supports modifiers (`ctrl`, `alt`, `shift`, `win`) + function keys, letters, digits, arrows, and special keys.

## Version History

| # | Directory | Language | Status | Notes |
|---|-----------|----------|--------|-------|
| 03 | `03-app-mute/` | Python + Rust | Archived | Python works but heavy deps; Rust has critical bugs |
| 04 | `04-app-mute-cs/` | C# | Archived | COM GUIDs all wrong, mute never works |
| 05 | `05-app-mute-c/` | C | Archived | IID copy-paste error, mute never works |
| 06 | `06-app-mute/` | C# .NET 8 | **Working** | Single-file publish, all COM issues fixed |
| 07 | `07-app-mute-cpp/` | C++ | Defective | Uses wrong API (mutes entire system), single hotkey only |
| 08 | `08-app-mute-rs/` | Rust | **Working** | 525KB single file, all previous bugs fixed |

### Recommended: v08 (Rust)

525KB single binary, zero runtime dependencies.

Fixes over previous versions:
- `static mut` UB → `SetWindowLongPtr` for safe state storage
- `GetMessageW(hwnd)` filtering WM_HOTKEY → pass `None`
- COM double-init conflict → single `CoInitializeEx(STA)` on main thread
- In-memory mute tracking → query `GetMute` from Windows each time
- F1-F12 only → full key support (a-z, 0-9, arrows, etc.)
- `IAudioEndpointVolume` (system-level) → `ISimpleAudioVolume` (session-level)
- Manual COM GUIDs → `windows` crate auto-generated GUIDs

### Also Working: v06 (C#)

C# .NET 8, single-file publish, no external dependencies. All COM interface issues resolved.

### Known Issues

<details>
<summary>v03 — Python + Rust</summary>

Python: maintains in-memory `muted_processes` dict, desyncs with Windows mixer state. `keyboard` library requires admin. Dead code in `gui.py`.

Rust: `static mut` global state is UB. `parse_vkey` only handles F1-F12. COM apartment/threading mismatch.
</details>

<details>
<summary>v04 — C# (old)</summary>

All COM interface GUIDs incorrect — `QueryInterface` always fails. vtable declarations truncated. `catch { }` swallows all exceptions.
</details>

<details>
<summary>v05 — C</summary>

`IID_ISimpleAudioVolume` is a copy of `IID_IAudioSessionManager2`. TOML parser only handles quoted values. No tray icon, no hot-reload.
</details>

<details>
<summary>v07 — C++</summary>

Uses `IAudioEndpointVolume` (device-level) instead of `ISimpleAudioVolume` (session-level) — mutes entire system. Single hotkey only. Missing `MOD_NOREPEAT`.
</details>

## Project Structure

```
├── 03-app-mute/          # Python + Rust (archived)
├── 04-app-mute-cs/       # C# old (archived)
├── 05-app-mute-c/        # C (archived)
├── 06-app-mute/          # C# .NET 8 (working)
├── 07-app-mute-cpp/      # C++ (defective)
└── 08-app-mute-rs/       # Rust (recommended)
    ├── src/              # Source code
    └── dist/             # Config template + README
```

## Build from Source

**v08 (Rust):**
```bash
cd 08-app-mute-rs
cargo build --release
# Output: target/release/app-mute.exe
```

**v06 (C#):**
```bash
cd 06-app-mute
dotnet publish -c Release -r win-x64 --self-contained -p:PublishSingleFile=true
# Output: bin/Release/net8.0/win-x64/publish/AppMute.exe
```

---

> This project was developed with [vibe coding](https://en.wikipedia.org/wiki/Vibe_coding) — an iterative, AI-assisted approach where implementation is driven by intent and rapid prototyping rather than upfront specification.
