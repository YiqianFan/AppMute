# AppMute

Windows per-application audio mute tool. Toggle any app's mute state via global hotkeys without touching system volume.

Windows 单应用静音工具。通过全局快捷键切换指定应用的静音状态，不影响系统总音量。

## Features / 功能

- Mute/unmute specific applications independently — 单独静音/取消静音指定应用
- Global hotkeys work regardless of which window is focused — 全局快捷键，任意窗口下生效
- System tray background operation — 最小化到系统托盘，后台运行
- Config hot-reload, no restart needed — 配置文件热重载，无需重启
- Process name matching, case-insensitive — 进程名匹配，大小写不敏感

## Quick Start / 快速开始

1. Download from [Releases](https://github.com/YiqianFan/AppMute/releases) — 从 Releases 下载
2. Place `app-mute.exe` and `config.toml` in the same directory — 放到同一目录
3. Edit `config.toml` — 编辑配置文件：

```toml
[hotkeys]
"f1" = "chrome"
"ctrl+alt+1" = "spotify"
"ctrl+alt+2" = "discord"
```

4. Run `app-mute.exe` — minimized to system tray — 运行后最小化到系统托盘
5. Right-click tray icon to exit — 右键托盘图标退出

## Technical Overview / 技术方案

### Audio Control / 音频控制

Windows Core Audio API (`IAudioSessionManager2` / `ISimpleAudioVolume`). Per-session mute, not device-level — only the target app is affected.

通过 Windows Core Audio API 按会话静音，不是设备级别——只影响目标应用。

Key interfaces / 关键接口：
- `IMMDeviceEnumerator` — default audio endpoint / 获取默认音频端点
- `IAudioSessionManager2` — session enumerator / 获取 session 枚举器
- `IAudioSessionEnumerator` — iterate active sessions / 遍历活跃 session
- `ISimpleAudioVolume` — get/set mute state / 设置/读取静音状态

### Hotkey System / 快捷键系统

Win32 `RegisterHotKey` with a hidden message-only window. `MOD_NOREPEAT` prevents hold-to-repeat. Supports modifiers (`ctrl`, `alt`, `shift`, `win`) + function keys, letters, digits, arrows, and special keys.

使用 Win32 `RegisterHotKey` + 隐藏 message-only window。支持修饰键 + 功能键/字母/数字/方向键等。

## Version History / 版本历史

| # | Directory | Language | Status | Notes / 说明 |
|---|-----------|----------|--------|--------------|
| 03 | `03-app-mute/` | Python + Rust | Archived / 归档 | Python works but heavy deps; Rust has critical bugs / Python 能用但依赖重；Rust 有致命 bug |
| 04 | `04-app-mute-cs/` | C# | Archived / 归档 | COM GUIDs all wrong / COM GUID 全错 |
| 05 | `05-app-mute-c/` | C | Archived / 归档 | IID copy-paste error / IID 复制粘贴错误 |
| 06 | `06-app-mute/` | C# .NET 8 | **Working / 可用** | Single-file publish, all COM issues fixed / 单文件发布，COM 问题全部修复 |
| 07 | `07-app-mute-cpp/` | C++ | Defective / 有缺陷 | Uses wrong API (mutes system), single hotkey / 用错接口（静音整个系统），只支持单热键 |
| 08 | `08-app-mute-rs/` | Rust | **Working / 可用** | 525KB single file, all bugs fixed / 525KB 单文件，修复全部问题 |

### Recommended: v08 (Rust) / 推荐：v08

525KB single binary, zero runtime dependencies.

525KB 单文件，零运行时依赖。

Fixes over previous versions / 相比旧版修复：
- `static mut` UB → `SetWindowLongPtr` for safe state storage / 安全状态存储
- `GetMessageW(hwnd)` filtering WM_HOTKEY → pass `None` / 正确接收热键消息
- COM double-init conflict → single `CoInitializeEx(STA)` / 单次 COM 初始化
- In-memory mute tracking → query `GetMute` each time / 每次查询实际状态
- F1-F12 only → full key support / 完整按键支持
- `IAudioEndpointVolume` (system-level) → `ISimpleAudioVolume` (session-level) / 会话级别静音
- Manual COM GUIDs → `windows` crate auto-generated / crate 自动生成

### Also Working: v06 (C#) / 同样可用

C# .NET 8, single-file publish, no external dependencies. All COM interface issues resolved.

C# .NET 8，单文件发布，无外部依赖，COM 接口问题全部修复。

### Known Issues / 已知问题

<details>
<summary>v03 — Python + Rust</summary>

Python: maintains in-memory `muted_processes` dict, desyncs with Windows mixer state. `keyboard` library requires admin. Dead code in `gui.py`.

Python：内存维护 mute 状态，与系统混音器不同步。`keyboard` 库需要管理员权限。

Rust: `static mut` global state is UB. `parse_vkey` only handles F1-F12. COM apartment/threading mismatch.

Rust：`static mut` 全局状态是未定义行为。`parse_vkey` 只支持 F1-F12。COM 线程模型冲突。
</details>

<details>
<summary>v04 — C# (old)</summary>

All COM interface GUIDs incorrect — `QueryInterface` always fails. vtable declarations truncated. `catch { }` swallows all exceptions.

COM 接口 GUID 全部错误，`QueryInterface` 永远失败。vtable 声明截断。
</details>

<details>
<summary>v05 — C</summary>

`IID_ISimpleAudioVolume` is a copy of `IID_IAudioSessionManager2`. TOML parser only handles quoted values. No tray icon, no hot-reload.

`IID_ISimpleAudioVolume` 是 `IID_IAudioSessionManager2` 的复制品。TOML 解析只处理带引号的值。
</details>

<details>
<summary>v07 — C++</summary>

Uses `IAudioEndpointVolume` (device-level) instead of `ISimpleAudioVolume` (session-level) — mutes entire system. Single hotkey only. Missing `MOD_NOREPEAT`.

使用 `IAudioEndpointVolume`（设备级别）而非 `ISimpleAudioVolume`（会话级别），会静音整个系统。
</details>

## Project Structure / 项目结构

```
├── 03-app-mute/          # Python + Rust (archived)
├── 04-app-mute-cs/       # C# old (archived)
├── 05-app-mute-c/        # C (archived)
├── 06-app-mute/          # C# .NET 8 (working)
├── 07-app-mute-cpp/      # C++ (defective)
└── 08-app-mute-rs/       # Rust (recommended)
    ├── src/              # Source code / 源码
    └── dist/             # Config template + README / 配置模板
```

## Build from Source / 从源码构建

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
>
> 本项目使用 [vibe coding](https://en.wikipedia.org/wiki/Vibe_coding) 方式开发——一种以意图驱动、快速原型为导向的 AI 辅助迭代开发方式。
