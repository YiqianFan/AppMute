# 08-app-mute-rs — Rust 重写版

## 技术栈
- Rust + `windows` 0.62（COM/Core Audio/RegisterHotKey/Shell_NotifyIconW）
- `toml` + `serde` 配置解析
- `log` + `simplelog` 文件日志

## 构建命令
```bash
cargo build              # 调试构建
cargo build --release    # 发布构建（LTO + strip）
```

## 代码风格
- 模块分离：main.rs / audio.rs / config.rs / hotkey.rs
- COM 操作只在主线程（STA），不跨线程
- 不用 `static mut`，状态通过 `SetWindowLongPtr` 传递
- 错误用 `anyhow::Result`，日志用 `log` 宏
- 中文注释，英文标识符

## 配置
- 格式：TOML（config.toml）
- 路径：与 exe 同目录
- 自动热重载（2秒轮询文件修改时间）
