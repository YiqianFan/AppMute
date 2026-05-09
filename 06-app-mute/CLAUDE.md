# 06-app-mute — C# 重写版

## 技术栈
- C# .NET 8，单文件 WinExe
- Win32 P/Invoke：RegisterHotKey + Shell_NotifyIconW（系统托盘）
- COM Interop：Windows Core Audio API（IAudioSessionManager2 / ISimpleAudioVolume）
- System.Text.Json + Source Generator 读取配置

## 构建命令
```bash
dotnet build                            # 调试构建
dotnet publish -c Release               # 单文件发布（12MB，输出到 publish/）
```

## 代码风格
- 全部逻辑在一个 Program.cs 中，按功能区域用注释分块
- COM 接口定义：完整 vtable 布局（不平铺截断），GUID 来自 Windows SDK
- P/Invoke 声明集中在 static class W
- 注释只写「为什么」，不写「是什么」
- 中文注释，代码标识符用英文

## 关键修复（相比旧版本）
1. COM 线程：主线程 COINIT_APARTMENTTHREADED 初始化，所有 COM 操作在同一线程
2. 音频状态：直接查询 Windows 实际静音状态（GetMute），不维护内存副本
3. COM GUID：全部从 Windows SDK 头文件确认，旧 C# 版的 GUID 全是错的
4. Vtable 布局：完整声明所有方法，不平铺截断

## 配置
- 格式：JSON（config.json）
- 路径：与 exe 同目录
- 自动热重载（2秒定时检查文件修改时间）

## 发布
- 单文件自包含 exe，不需要安装 .NET 运行时
- TrimmerRootAssembly 保留 COM 类型不被裁剪
