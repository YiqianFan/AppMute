# app-mute — 全局快捷键应用静音 (C version)

## 编译

需要 MSYS2 + MinGW：

```bash
gcc -O2 -s -static app-mute.c -o app-mute.exe -luser32 -lgdi32 -lole32 -lcomctl32 -lshell32 -mwindows
```

## 运行

```bash
./app-mute.exe
```

## 配置

编辑 `config.toml`：

```toml
[hotkeys]
"f1" = "chrome"
"ctrl+alt+1" = "chrome"
"ctrl+alt+2" = "spotify"
```