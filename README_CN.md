# AppMute

Windows 单应用静音工具。通过全局快捷键切换指定应用的静音状态，不影响系统总音量。

## 功能

- 单独静音/取消静音指定应用
- 全局快捷键，任意窗口下生效
- 最小化到系统托盘，后台运行
- 配置文件热重载，无需重启
- 进程名匹配，大小写不敏感

## 快速开始

1. 从 [Releases](https://github.com/YiqianFan/AppMute/releases) 下载
2. 把 `app-mute.exe` 和 `config.toml` 放到同一目录
3. 编辑 `config.toml`：

```toml
[hotkeys]
"f1" = "chrome"
"ctrl+alt+1" = "spotify"
"ctrl+alt+2" = "discord"
```

4. 运行 `app-mute.exe`，最小化到系统托盘
5. 右键托盘图标退出

## 技术方案

### 音频控制

通过 Windows Core Audio API（`IAudioSessionManager2` / `ISimpleAudioVolume`）按会话静音，不是设备级别——只影响目标应用。

关键接口：
- `IMMDeviceEnumerator` — 获取默认音频端点
- `IAudioSessionManager2` — 获取 session 枚举器
- `IAudioSessionEnumerator` — 遍历活跃 session
- `ISimpleAudioVolume` — 设置/读取静音状态

### 快捷键系统

使用 Win32 `RegisterHotKey` + 隐藏 message-only window。`MOD_NOREPEAT` 防止按住重复触发。支持修饰键（`ctrl`、`alt`、`shift`、`win`）+ 功能键/字母/数字/方向键等。

## 版本历史

| # | 目录 | 语言 | 状态 | 说明 |
|---|------|------|------|------|
| 03 | `03-app-mute/` | Python + Rust | 归档 | Python 能用但依赖重；Rust 有致命 bug |
| 04 | `04-app-mute-cs/` | C# | 归档 | COM GUID 全错，静音不工作 |
| 05 | `05-app-mute-c/` | C | 归档 | IID 复制粘贴错误，静音不工作 |
| 06 | `06-app-mute/` | C# .NET 8 | **可用** | 单文件发布，COM 问题全部修复 |
| 07 | `07-app-mute-cpp/` | C++ | 有缺陷 | 用错接口（静音整个系统），只支持单热键 |
| 08 | `08-app-mute-rs/` | Rust | **可用** | 525KB 单文件，修复全部问题 |

### 推荐：v08（Rust）

525KB 单文件，零运行时依赖。

相比旧版修复：
- `static mut` 未定义行为 → `SetWindowLongPtr` 安全状态存储
- `GetMessageW(hwnd)` 过滤掉 WM_HOTKEY → 传 `None` 正确接收
- COM 双重初始化冲突 → 主线程单次 `CoInitializeEx(STA)`
- 内存维护 mute 状态 → 每次查询 Windows 实际状态
- 只支持 F1-F12 → 完整按键支持（a-z、0-9、方向键等）
- `IAudioEndpointVolume`（设备级别）→ `ISimpleAudioVolume`（会话级别）
- 手动 COM GUID → `windows` crate 自动生成

### 同样可用：v06（C#）

C# .NET 8，单文件发布，无外部依赖，COM 接口问题全部修复。

### 已知问题

<details>
<summary>v03 — Python + Rust</summary>

Python：内存维护 `muted_processes` 字典，与系统混音器不同步。`keyboard` 库需要管理员权限。`gui.py` 有死代码。

Rust：`static mut` 全局状态是未定义行为。`parse_vkey` 只支持 F1-F12。COM 线程模型冲突。
</details>

<details>
<summary>v04 — C# 旧版</summary>

COM 接口 GUID 全部错误，`QueryInterface` 永远失败。vtable 声明截断。`catch { }` 吞掉所有异常。
</details>

<details>
<summary>v05 — C</summary>

`IID_ISimpleAudioVolume` 是 `IID_IAudioSessionManager2` 的复制品。TOML 解析只处理带引号的值。无托盘图标，无热重载。
</details>

<details>
<summary>v07 — C++</summary>

使用 `IAudioEndpointVolume`（设备级别）而非 `ISimpleAudioVolume`（会话级别），会静音整个系统。只支持单热键。缺少 `MOD_NOREPEAT`。
</details>

## 项目结构

```
├── 03-app-mute/          # Python + Rust（归档）
├── 04-app-mute-cs/       # C# 旧版（归档）
├── 05-app-mute-c/        # C（归档）
├── 06-app-mute/          # C# .NET 8（可用）
├── 07-app-mute-cpp/      # C++（有缺陷）
└── 08-app-mute-rs/       # Rust（推荐）
    ├── src/              # 源码
    └── dist/             # 配置模板
```

## 从源码构建

**v08（Rust）：**
```bash
cd 08-app-mute-rs
cargo build --release
# 输出：target/release/app-mute.exe
```

**v06（C#）：**
```bash
cd 06-app-mute
dotnet publish -c Release -r win-x64 --self-contained -p:PublishSingleFile=true
# 输出：bin/Release/net8.0/win-x64/publish/AppMute.exe
```

---

> 本项目使用 [vibe coding](https://en.wikipedia.org/wiki/Vibe_coding) 方式开发——一种以意图驱动、快速原型为导向的 AI 辅助迭代开发方式。
