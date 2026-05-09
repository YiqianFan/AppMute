# AppMute — 全局快捷键应用静音

按组合键单独控制某个 Windows 应用的静音状态，不影响其他应用和系统总音量。

## 使用场景

- 游戏中快速屏蔽语音聊天
- 后台音乐临时静音但不停止播放
- 浏览器视频自动播放太吵，只静浏览器

## 使用方法

1. 双击 `AppMute.exe` 运行，程序最小化到系统托盘
2. 首次运行自动在同目录创建 `config.json`
3. 按配置的快捷键切换对应应用的静音
4. 右键托盘图标：编辑配置 / 退出

## 配置格式（config.json）

```json
{
  "Hotkeys": {
    "F1": "chrome",
    "ctrl+alt+1": "spotify",
    "ctrl+alt+2": "discord"
  }
}
```

- 进程名不含 `.exe`，大小写不敏感
- 修饰键：`ctrl`、`alt`、`shift`、`win`
- 按键：`F1`-`F12`、字母 `a`-`z`、数字 `0`-`9`、`space`、`enter`、`esc`、`tab`
- 修改配置后自动生效，无需重启

## 构建

需要 .NET 8 SDK：

```bash
dotnet publish -c Release
```

生成的单文件 exe 在 `bin/Release/net8.0/win-x64/publish/` 下，约 12MB，无需安装 .NET。
