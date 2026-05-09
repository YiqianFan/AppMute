use std::collections::HashMap;
use windows::Win32::UI::WindowsAndMessaging::{
    RegisterHotKey, UnregisterHotKey, HOT_KEY_MODIFIERS,
    MOD_ALT, MOD_CONTROL, MOD_NOREPEAT, MOD_SHIFT, MOD_WIN,
    VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
    WM_HOTKEY,
};
use windows::Win32::Foundation::HWND;
use log::{info, warn};

pub struct HotkeyManager {
    hotkey_ids: HashMap<i32, String>,
    next_id: i32,
}

impl HotkeyManager {
    pub fn new() -> Self {
        Self {
            hotkey_ids: HashMap::new(),
            next_id: 1,
        }
    }

    pub fn parse_hotkey(hotkey_str: &str) -> (HOT_KEY_MODIFIERS, u32) {
        let mut modifiers = HOT_KEY_MODIFIERS(0);
        let mut vk = 0u32;

        let parts: Vec<&str> = hotkey_str.split('+').map(|s| s.trim()).collect();
        for part in &parts {
            match part.to_uppercase().as_str() {
                "CTRL" | "CONTROL" => modifiers |= MOD_CONTROL,
                "ALT" => modifiers |= MOD_ALT,
                "SHIFT" => modifiers |= MOD_SHIFT,
                "WIN" => modifiers |= MOD_WIN,
                _ => {
                    if let Some(vkey) = Self::parse_vkey(part) {
                        vk = vkey;
                    }
                }
            }
        }
        (modifiers, vk)
    }

    pub fn register(&mut self, hwnd: isize, hotkey_str: &str, app_name: &str) -> Option<i32> {
        let (modifiers, vk) = Self::parse_hotkey(hotkey_str);
        if vk == 0 {
            warn!("Invalid hotkey: {}", hotkey_str);
            return None;
        }

        let id = self.next_id;
        self.next_id += 1;

        unsafe {
            let result = RegisterHotKey(HWND(hwnd as *mut _), id, modifiers, vk);
            if result.is_ok() {
                self.hotkey_ids.insert(id, app_name.to_lowercase());
                info!("Registered hotkey '{}' -> {} (id={})", hotkey_str, app_name, id);
                Some(id)
            } else {
                warn!("Failed to register hotkey: {}", hotkey_str);
                None
            }
        }
    }

    pub fn unregister_all(&self, hwnd: isize) {
        unsafe {
            for &id in self.hotkey_ids.keys() {
                let _ = UnregisterHotKey(HWND(hwnd as *mut _), id);
            }
        }
        info!("Unregistered all hotkeys");
    }

    pub fn get_app_for_id(&self, id: i32) -> Option<&String> {
        self.hotkey_ids.get(&id)
    }

    fn parse_vkey(s: &str) -> Option<u32> {
        match s.to_uppercase().as_str() {
            "F1" => Some(VK_F1.0 as u32),
            "F2" => Some(VK_F2.0 as u32),
            "F3" => Some(VK_F3.0 as u32),
            "F4" => Some(VK_F4.0 as u32),
            "F5" => Some(VK_F5.0 as u32),
            "F6" => Some(VK_F6.0 as u32),
            "F7" => Some(VK_F7.0 as u32),
            "F8" => Some(VK_F8.0 as u32),
            "F9" => Some(VK_F9.0 as u32),
            "F10" => Some(VK_F10.0 as u32),
            "F11" => Some(VK_F11.0 as u32),
            "F12" => Some(VK_F12.0 as u32),
            _ => None,
        }
    }
}