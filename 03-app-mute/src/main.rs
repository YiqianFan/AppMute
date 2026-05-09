mod audio;
mod config;
mod hotkey;

use std::path::Path;
use std::sync::{Arc, Mutex};
use windows::Win32::Foundation::{HWND, LPARAM, LRESULT, WPARAM};
use windows::Win32::System::Com::{CoInitializeEx, COINIT_APARTMENTTHREADED};
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, DefWindowProcW, DispatchMessageW, GetMessageW, PostQuitMessage,
    TranslateMessage, MSG, WS_EX_TOOLWINDOW, WS_OVERLAPPEDWINDOW,
    RegisterClassW, WNDCLASSEXW, CW_USEDEFAULT, IDI_APPLICATION, IDC_ARROW,
    LoadCursorW, LoadIconW, WM_HOTKEY, WM_DESTROY, WM_USER,
};
use windows::core::{PCWSTR, w};
use log::{info, warn, LevelFilter};
use simplelog::{Config, WriteLogger};

use audio::AudioManager;
use config::Config as AppConfig;
use hotkey::HotkeyManager;

unsafe extern "system" fn wndproc(hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    match msg {
        WM_HOTKEY => {
            let id = wparam.0 as i32;
            if let Some(ref hotkeys) = *HOTKEY_MGR.lock().unwrap() {
                if let Some(app_name) = hotkeys.get_app_for_id(id) {
                    if let Ok(mut a) = AUDIO_MGR.lock().unwrap() {
                        match a.toggle_process_mute(app_name) {
                            Ok(new_state) => info!("Toggled '{}' to mute={}", app_name, new_state),
                            Err(e) => warn!("Toggle failed: {}", e),
                        }
                    }
                }
            }
            LRESULT(0)
        }
        WM_DESTROY => {
            PostQuitMessage(0);
            LRESULT(0)
        }
        _ => DefWindowProcW(hwnd, msg, wparam, lparam),
    }
}

static mut AUDIO_MGR: Option<Arc<Mutex<AudioManager>>> = None;
static mut HOTKEY_MGR: Option<HotkeyManager> = None;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let log_file = std::fs::OpenOptions::new()
        .create(true)
        .write(true)
        .truncate(true)
        .open("app-mute.log")?;
    WriteLogger::init(LevelFilter::Info, Config::default(), log_file)?;

    info!("AppMute starting...");

    let config = AppConfig::load(Path::new("config.toml"))?;
    let audio = Arc::new(Mutex::new(AudioManager::new()?));

    unsafe {
        CoInitializeEx(None, COINIT_APARTMENTTHREADED)?;

        let window_class = w!("AppMuteClass");
        let wnd_class = WNDCLASSEXW {
            cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
            style: windows::Win32::UI::WindowsAndMessaging::CS_HREDRAW | windows::Win32::UI::WindowsAndMessaging::CS_VREDRAW,
            lpfnWndProc: Some(wndproc),
            hInstance: None,
            hIcon: LoadIconW(None, IDI_APPLICATION)?,
            hCursor: LoadCursorW(None, IDC_ARROW)?,
            lpszClassName: PCWSTR(window_class.0),
            ..Default::default()
        };

        RegisterClassW(&wnd_class);

        let hwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            window_class,
            PCWSTR::null(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 200, 100,
            None,
            None,
            None,
            None,
        )?;

        let mut hotkey_mgr = HotkeyManager::new();
        for (hotkey_str, app_name) in &config.hotkeys {
            hotkey_mgr.register(hwnd.0 as isize, hotkey_str, app_name);
        }

        AUDIO_MGR = Some(audio);
        HOTKEY_MGR = Some(hotkey_mgr);

        info!("Hotkeys registered, entering message loop...");

        let mut msg = MSG::default();
        loop {
            if GetMessageW(&mut msg, Some(hwnd), 0, 0).0 == 0 {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    info!("AppMute exiting...");
    Ok(())
}