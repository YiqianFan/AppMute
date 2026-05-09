# AppMute-CS — C# 全局快捷键应用静音

技术栈：C# + Win32 API

## 构建

需要 .NET SDK。Windows 10/11 自带 .NET，不需要单独安装。

```bash
dotnet build -c Release
```

输出：`bin/Release/net8.0/AppMute.exe`（单文件，约 5MB）

## 运行

```bash
dotnet run -c Release
# 或直接运行 exe
./bin/Release/net8.0/AppMute.exe
```

## 依赖

无外部依赖，所有 Win32 API 直接 P/Invoke。

## 配置

编辑 `config.json`：
```json
{
  "hotkeys": {
    "F1": "chrome",
    "F2": "spotify",
    "ctrl+alt+m": "discord"
  }
}
```

## 架构

```
AppMute/
├── Program.cs          # 入口，托盘，消息循环
├── HotkeyManager.cs    # RegisterHotKey 全局快捷键
├── AudioManager.cs     # Core Audio API 静音控制
├── Config.cs          # JSON 配置加载
├── TrayIcon.cs        # 系统托盘图标
└── config.json         # 用户配置
```