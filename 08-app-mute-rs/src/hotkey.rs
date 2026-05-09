use std::collections::HashMap;
use windows::Win32::Foundation::HWND;
use windows::Win32::UI::Input::KeyboardAndMouse::*;
use log::{info, warn};

use crate::config;

const MOD_NOREPEAT: u32 = 0x4000;

pub struct HotkeyManager {
    hotkeys: HashMap<i32, String>, // id -> app_name
    next_id: i32,
}

impl HotkeyManager {
    pub fn new() -> Self {
        Self {
            hotkeys: HashMap::new(),
            next_id: 1,
        }
    }

    /// 注册一个热键，返回是否成功
    pub fn register(&mut self, hwnd: HWND, hotkey_str: &str, app_name: &str) -> bool {
        let (mods, vk) = match config::parse_hotkey(hotkey_str) {
            Some(v) => v,
            None => {
                warn!("无法解析热键: {}", hotkey_str);
                return false;
            }
        };

        let id = self.next_id;
        self.next_id += 1;

        let ok = unsafe {
            RegisterHotKey(Some(hwnd), id, HOT_KEY_MODIFIERS(mods | MOD_NOREPEAT), vk)
        };
        if ok.is_ok() {
            self.hotkeys.insert(id, app_name.to_lowercase());
            info!("注册热键: {} -> {} (id={})", hotkey_str, app_name, id);
            true
        } else {
            warn!("注册热键失败（可能被占用）: {}", hotkey_str);
            false
        }
    }

    /// 注销所有热键
    pub fn unregister_all(&self, hwnd: HWND) {
        for &id in self.hotkeys.keys() {
            unsafe { let _ = UnregisterHotKey(Some(hwnd), id); }
        }
        info!("已注销全部热键");
    }

    /// 根据热键 ID 查找对应的应用名
    pub fn get_app_name(&self, id: i32) -> Option<&str> {
        self.hotkeys.get(&id).map(|s| s.as_str())
    }

    pub fn len(&self) -> usize {
        self.hotkeys.len()
    }
}
