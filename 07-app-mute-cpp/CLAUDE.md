# AppMute C++ — 项目约定

## 技术栈
- 原生 Win32 C++（无框架依赖）
- CMake + MSVC
- 托盘程序 + 热键 + 原生设置对话框

## 构建命令
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

## 代码风格
- 命名空间：`dialog`、`audio`、`hotkey`、`config`、`tray`
- 全局变量前缀：`g_`
- 头文件内联保护：`#pragma once`
- 字符：宽字符 `wchar_t` + `L""`

## 模块接口
- `config.h` — 配置加载/保存（`Config_Load`、`Config_Save`）
- `hotkey.h` — 热键注册/注销（`Hotkey_Register`、`Hotkey_Unregister`）
- `audio.h` — 音频会话管理（`Audio_ToggleMute`、`Audio_Init`）
- `tray.h` — 托盘图标和菜单（`Tray_Init`、`Tray_ShowContextMenu`）
- `dialog.h` — 设置对话框（`dialog::ShowSettings`）

## 验证
- 编译通过后，用 `build/MuteApp.exe` 测试热键和托盘功能