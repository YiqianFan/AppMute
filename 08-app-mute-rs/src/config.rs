use std::collections::HashMap;
use std::path::Path;
use serde::Deserialize;

// Win32 modifier key constants (from WinUser.h)
const MOD_ALT: u32 = 0x0001;
const MOD_CONTROL: u32 = 0x0002;
const MOD_SHIFT: u32 = 0x0004;
const MOD_WIN: u32 = 0x0008;

#[derive(Debug, Deserialize)]
pub struct AppConfig {
    pub hotkeys: HashMap<String, String>,
}

pub fn load(path: &Path) -> anyhow::Result<AppConfig> {
    let content = std::fs::read_to_string(path)
        .map_err(|e| anyhow::anyhow!("读取配置 {}: {}", path.display(), e))?;
    let config: AppConfig = toml::from_str(&content)
        .map_err(|e| anyhow::anyhow!("解析配置 {}: {}", path.display(), e))?;
    Ok(config)
}

/// 解析热键字符串，返回 (修饰键位掩码, 虚拟键码)
pub fn parse_hotkey(s: &str) -> Option<(u32, u32)> {
    let lower = s.to_lowercase();
    let parts: Vec<&str> = lower.split('+').map(str::trim).collect();

    let mut mods: u32 = 0;
    let mut vk: u32 = 0;

    for part in &parts {
        match *part {
            "ctrl" | "control" => mods |= MOD_CONTROL,
            "alt" => mods |= MOD_ALT,
            "shift" => mods |= MOD_SHIFT,
            "win" | "windows" => mods |= MOD_WIN,
            _ => vk = parse_vk(part)?,
        }
    }

    if vk == 0 { None } else { Some((mods, vk)) }
}

fn parse_vk(s: &str) -> Option<u32> {
    // F1-F12
    if let Some(rest) = s.strip_prefix('f') {
        if let Ok(n) = rest.parse::<u32>() {
            if (1..=12).contains(&n) {
                return Some(0x6F + n); // VK_F1=0x70
            }
        }
    }

    // 单字符 a-z, 0-9
    if s.len() == 1 {
        let b = s.as_bytes()[0];
        if b.is_ascii_lowercase() {
            return Some((b - b'a' + b'A') as u32); // VK_A=0x41
        }
        if b.is_ascii_digit() {
            return Some(b as u32); // VK_0=0x30 .. VK_9=0x39
        }
    }

    // 命名键
    match s {
        "space" => Some(0x20),
        "enter" | "return" => Some(0x0D),
        "esc" | "escape" => Some(0x1B),
        "tab" => Some(0x09),
        "backspace" => Some(0x08),
        "delete" | "del" => Some(0x2E),
        "insert" | "ins" => Some(0x2D),
        "home" => Some(0x24),
        "end" => Some(0x23),
        "pageup" | "page up" => Some(0x21),
        "pagedown" | "page down" => Some(0x22),
        "left" => Some(0x25),
        "up" => Some(0x26),
        "right" => Some(0x27),
        "down" => Some(0x28),
        _ => None,
    }
}
