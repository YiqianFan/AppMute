#![windows_subsystem = "windows"]

mod audio;
mod config;
mod hotkey;

use std::path::PathBuf;
use std::time::SystemTime;
use windows::core::{w, PCWSTR};
use windows::Win32::Foundation::{HWND, LPARAM, LRESULT, WPARAM};
use windows::Win32::System::Com::{CoInitializeEx, CoUninitialize, COINIT_APARTMENTTHREADED};
use windows::Win32::System::LibraryLoader::GetModuleHandleW;
use windows::Win32::UI::Shell::{NIF_ICON, NIF_INFO, NIF_MESSAGE, NIF_TIP, NIM_ADD, NIM_DELETE, NIM_MODIFY, NOTIFYICONDATAW, Shell_NotifyIconW};
use windows::Win32::UI::WindowsAndMessaging::*;
use log::info;

const WM_TRAY: u32 = WM_APP + 1;
const TIMER_RELOAD: usize = 1;
const MENU_EDIT: u16 = 1;
const MENU_EXIT: u16 = 2;

struct AppState {
    config: config::AppConfig,
    hotkey_mgr: hotkey::HotkeyManager,
    config_path: PathBuf,
    last_mtime: SystemTime,
    hwnd: HWND,
    h_menu: HMENU,
    tray_data: NOTIFYICONDATAW,
}

fn main() -> anyhow::Result<()> {
    // 初始化日志（启动时 truncate）
    let log_path = {
        let mut p = std::env::current_exe().unwrap_or_else(|_| PathBuf::from("."));
        p.set_file_name("app-mute.log");
        p
    };
    let log_file = std::fs::OpenOptions::new()
        .create(true).write(true).truncate(true).open(&log_path)?;
    simplelog::WriteLogger::init(
        simplelog::LevelFilter::Info,
        simplelog::Config::default(),
        log_file,
    )?;
    info!("AppMute 启动");

    // 初始化 COM（STA，只调一次）
    unsafe { CoInitializeEx(None, COINIT_APARTMENTTHREADED).ok()?; }

    // 加载配置
    let config_path = {
        let mut p = std::env::current_exe().unwrap_or_else(|_| PathBuf::from("."));
        p.set_file_name("config.toml");
        p
    };
    let cfg = config::load(&config_path).unwrap_or_else(|e| {
        log::warn!("加载配置失败: {}，使用空配置", e);
        config::AppConfig { hotkeys: Default::default() }
    });
    let last_mtime = std::fs::metadata(&config_path)
        .and_then(|m| m.modified())
        .unwrap_or(SystemTime::UNIX_EPOCH);

    // 创建隐藏窗口
    let hwnd = create_message_window()?;

    // 注册热键
    let mut hotkey_mgr = hotkey::HotkeyManager::new();
    for (hk, app) in &cfg.hotkeys {
        hotkey_mgr.register(hwnd, hk, app);
    }
    info!("已注册 {} 个热键", hotkey_mgr.len());

    // 创建托盘图标
    let tray_data = create_tray_icon(hwnd);

    // 创建右键菜单
    let h_menu = unsafe { CreatePopupMenu()? };
    unsafe {
        AppendMenuW(h_menu, MF_STRING, MENU_EDIT as usize, w!("编辑配置"))?;
        AppendMenuW(h_menu, MF_SEPARATOR, 0, None)?;
        AppendMenuW(h_menu, MF_STRING, MENU_EXIT as usize, w!("退出"))?;
    }

    // 设置配置热重载定时器（2秒）
    unsafe { SetTimer(Some(hwnd), TIMER_RELOAD, 2000, None); }

    // 打包状态，通过 SetWindowLongPtr 传给 wndproc
    let state = Box::new(AppState {
        config: cfg,
        hotkey_mgr,
        config_path,
        last_mtime,
        hwnd,
        h_menu,
        tray_data,
    });
    unsafe {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, Box::into_raw(state) as isize);
    }

    info!("进入消息循环");

    // 消息循环
    let mut msg = MSG::default();
    loop {
        let ret = unsafe { GetMessageW(&mut msg, None, 0, 0) };
        if ret.0 == 0 || ret.0 == -1 {
            break;
        }
        unsafe {
            let _ = TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // 清理（拿回 state 所有权）
    let state = unsafe {
        let ptr = GetWindowLongPtrW(hwnd, GWLP_USERDATA) as *mut AppState;
        if !ptr.is_null() { Some(Box::from_raw(ptr)) } else { None }
    };
    if let Some(s) = state {
        cleanup(&s);
    }

    unsafe { CoUninitialize(); }
    info!("AppMute 退出");
    Ok(())
}

fn create_message_window() -> anyhow::Result<HWND> {
    let class_name = w!("AppMuteWnd");
    let hinst = unsafe { GetModuleHandleW(None)? };

    let wc = WNDCLASSEXW {
        cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
        lpfnWndProc: Some(wndproc),
        hInstance: hinst.into(),
        lpszClassName: class_name,
        ..Default::default()
    };
    unsafe { RegisterClassExW(&wc) };

    let hwnd = unsafe {
        CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            class_name,
            w!("AppMute"),
            WS_POPUP,
            0, 0, 0, 0,
            None, None, Some(hinst.into()),
            None,
        )?
    };
    Ok(hwnd)
}

unsafe extern "system" fn wndproc(
    hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM,
) -> LRESULT {
    match msg {
        WM_HOTKEY => {
            let id = wparam.0 as i32;
            if let Some(state) = get_state(hwnd) {
                if let Some(app) = state.hotkey_mgr.get_app_name(id) {
                    let app = app.to_owned();
                    match audio::toggle_mute(&app) {
                        Ok(muted) => {
                            info!("{}: {}", app, if muted { "已静音" } else { "已取消静音" });
                            let msg = if muted { format!("已静音: {}", app) } else { format!("已取消静音: {}", app) };
                            show_balloon(&mut state.tray_data, "AppMute", &msg);
                        }
                        Err(e) => log::warn!("切换静音失败: {}", e),
                    }
                }
            }
            LRESULT(0)
        }

        WM_TIMER if wparam.0 == TIMER_RELOAD => {
            check_config_reload(hwnd);
            LRESULT(0)
        }

        msg if msg == WM_TRAY => {
            match lparam.0 as u32 {
                WM_RBUTTONUP => show_context_menu(hwnd),
                WM_LBUTTONDBLCLK => open_config_file(hwnd),
                _ => {}
            }
            LRESULT(0)
        }

        WM_COMMAND => {
            let cmd = (wparam.0 & 0xFFFF) as u16;
            match cmd {
                MENU_EXIT => {
                    let _ = DestroyWindow(hwnd);
                }
                MENU_EDIT => open_config_file(hwnd),
                _ => {}
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

unsafe fn get_state(hwnd: HWND) -> Option<&'static mut AppState> {
    let ptr = GetWindowLongPtrW(hwnd, GWLP_USERDATA) as *mut AppState;
    if ptr.is_null() { None } else { Some(&mut *ptr) }
}

fn check_config_reload(hwnd: HWND) {
    let state = match unsafe { get_state(hwnd) } {
        Some(s) => s,
        None => return,
    };

    let mtime = match std::fs::metadata(&state.config_path) {
        Ok(m) => m.modified().unwrap_or(SystemTime::UNIX_EPOCH),
        Err(_) => return,
    };

    if mtime <= state.last_mtime {
        return;
    }

    state.last_mtime = mtime;
    info!("配置文件变化，重新加载...");

    state.hotkey_mgr.unregister_all(state.hwnd);

    match config::load(&state.config_path) {
        Ok(cfg) => {
            let mut mgr = hotkey::HotkeyManager::new();
            for (hk, app) in &cfg.hotkeys {
                mgr.register(state.hwnd, hk, app);
            }
            info!("重载完成: {} 个热键", mgr.len());
            state.hotkey_mgr = mgr;
            state.config = cfg;
        }
        Err(e) => {
            log::warn!("重载配置失败: {}", e);
        }
    }
}

fn create_tray_icon(hwnd: HWND) -> NOTIFYICONDATAW {
    let mut nid = NOTIFYICONDATAW {
        cbSize: std::mem::size_of::<NOTIFYICONDATAW>() as u32,
        hWnd: hwnd,
        uID: 1,
        uFlags: NIF_ICON | NIF_MESSAGE | NIF_TIP,
        uCallbackMessage: WM_TRAY,
        hIcon: unsafe { LoadIconW(None, IDI_INFORMATION).unwrap_or_default() },
        ..Default::default()
    };
    let tip: Vec<u16> = "AppMute\0".encode_utf16().collect();
    nid.szTip[..tip.len()].copy_from_slice(&tip);

    unsafe { let _ = Shell_NotifyIconW(NIM_ADD, &nid); }
    nid
}

fn show_balloon(nid: &mut NOTIFYICONDATAW, title: &str, msg: &str) {
    nid.uFlags = NIF_INFO;
    let title_w: Vec<u16> = title.encode_utf16().chain(std::iter::once(0)).collect();
    let msg_w: Vec<u16> = msg.encode_utf16().chain(std::iter::once(0)).collect();
    nid.szInfoTitle[..title_w.len()].copy_from_slice(&title_w);
    nid.szInfo[..msg_w.len()].copy_from_slice(&msg_w);
    nid.Anonymous.uTimeout = 2000;
    unsafe { let _ = Shell_NotifyIconW(NIM_MODIFY, nid); }
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.szInfo = [0; 256];
    nid.szInfoTitle = [0; 64];
}

fn show_context_menu(hwnd: HWND) {
    let state = match unsafe { get_state(hwnd) } {
        Some(s) => s,
        None => return,
    };
    unsafe {
        let mut pt = Default::default();
        let _ = GetCursorPos(&mut pt);
        let _ = SetForegroundWindow(hwnd);
        let _ = TrackPopupMenu(
            state.h_menu,
            TPM_RIGHTBUTTON,
            pt.x, pt.y,
            Some(0),
            hwnd,
            None,
        );
    }
}

fn open_config_file(hwnd: HWND) {
    let state = match unsafe { get_state(hwnd) } {
        Some(s) => s,
        None => return,
    };
    let path = &state.config_path;
    let path_w: Vec<u16> = path.to_string_lossy().encode_utf16().chain(std::iter::once(0)).collect();
    unsafe {
        let _ = windows::Win32::UI::Shell::ShellExecuteW(
            Some(hwnd),
            w!("open"),
            PCWSTR(path_w.as_ptr()),
            None,
            None,
            SW_SHOWNORMAL,
        );
    }
}

fn cleanup(state: &AppState) {
    state.hotkey_mgr.unregister_all(state.hwnd);
    unsafe {
        let _ = Shell_NotifyIconW(NIM_DELETE, &state.tray_data);
        let _ = KillTimer(Some(state.hwnd), TIMER_RELOAD);
        let _ = DestroyMenu(state.h_menu);
        let _ = DestroyWindow(state.hwnd);
    }
}
