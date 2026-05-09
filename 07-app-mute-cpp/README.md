# AppMute

Windows 托盘程序：通过全局热键快速切换系统静音状态。

## 功能
- **全局热键**：默认 `Ctrl+Alt+M` 切换静音
- **托盘运行**：最小化到系统托盘，右键菜单
- **设置对话框**：可修改目标进程和热键
- **单实例**：重复运行会提示

## 构建
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

## 使用
1. 运行 `MuteApp.exe`
2. 使用 `Ctrl+Alt+M` 切换静音
3. 右键托盘图标 → "设置..." 修改配置