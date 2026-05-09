# AppMute — 全局快捷键应用静音

技术栈：Python 3 + pycaw + keyboard

## 依赖安装

```bash
pip install -r requirements.txt
```

## 运行

```bash
python src/main.py
```

无参数启动，最小化到托盘（如果 pystray 可用）。

## 配置

编辑 `config.toml`：

```toml
[hotkeys]
"f1" = "chrome"
"f2" = "spotify"
"ctrl+alt+m" = "discord"

[general]
check_interval = 1000
```

## 行为

- 按下配置的快捷键，切换目标应用的静音状态
- 托盘图标双击：显示当前已静音应用
- 右键托盘图标：退出

## 代码风格

- 无异常吞掉，打印到 stdout
- 日志用 print，输出到控制台