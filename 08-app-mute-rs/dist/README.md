# AppMute — 全局快捷键应用静音工具

一键静音/取消静音任意应用程序，不干扰系统音量。

## 快速开始

1. 把 `app-mute.exe` 和 `config.toml` 放在同一个文件夹
2. 双击 `app-mute.exe` 启动
3. 系统托盘出现图标，热键立即生效

## 配置热键

编辑 `config.toml`，格式：

```toml
[hotkeys]
"快捷键" = "应用名"
```

- **应用名**：不区分大小写，不带 `.exe` 后缀
- 修改后保存即可，程序每 2 秒自动检测，无需重启

示例：

```toml
[hotkeys]
"f1" = "chrome"
"ctrl+alt+1" = "spotify"
"ctrl+alt+2" = "discord"
"ctrl+shift+m" = "steam"
```

### 支持的快捷键

| 修饰键 | 写法 |
|--------|------|
| Ctrl | `ctrl` |
| Alt | `alt` |
| Shift | `shift` |
| Win | `win` |

| 主键 | 写法 |
|------|------|
| F1-F12 | `f1` ~ `f12` |
| 字母 | `a` ~ `z` |
| 数字 | `0` ~ `9` |
| 空格 | `space` |
| 回车 | `enter` 或 `return` |
| Esc | `esc` 或 `escape` |
| Tab | `tab` |
| 方向键 | `up` / `down` / `left` / `right` |
| Insert | `insert` 或 `ins` |
| Delete | `delete` 或 `del` |
| Home/End | `home` / `end` |
| Page Up/Down | `pageup` / `pagedown` |

组合方式：`修饰键+主键`，如 `ctrl+alt+1`、`shift+f5`。

## 使用方式

| 操作 | 效果 |
|------|------|
| 按配置的快捷键 | 切换对应应用的静音状态 |
| 右键托盘图标 | 打开菜单（编辑配置 / 退出） |
| 双击托盘图标 | 用记事本打开 config.toml |

### 一键静音多个程序

同一个快捷键绑定一个应用名，如果该应用有多个进程（例如 Chrome 开了多个标签页），按下快捷键会**同时静音所有匹配的进程**。

如果你需要一个快捷键静音多个不同应用，配置多条映射指向不同的键即可：

```toml
[hotkeys]
"ctrl+alt+1" = "chrome"
"ctrl+alt+2" = "discord"
"ctrl+alt+3" = "spotify"
```

按 `Ctrl+Alt+1` 只静音 Chrome，`Ctrl+Alt+2` 只静音 Discord，互不干扰。

如果想用**一个键同时控制多个应用**，目前需要为每个应用分配独立快捷键。程序按进程名匹配，不支持一个快捷键绑定多个应用名。

## 日志

程序运行时会在同目录生成 `app-mute.log`，每次启动清空重写。排查问题时可以查看。

## 注意事项

- 应用名必须与任务管理器中显示的进程名一致（不含 `.exe`）
- 如果快捷键注册失败（被其他程序占用），日志会提示
- 不需要管理员权限
- 仅支持 Windows
