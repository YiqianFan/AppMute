use windows::core::Interface;
use windows::Win32::Foundation::CloseHandle;
use windows::Win32::Media::Audio::*;
use windows::Win32::System::Com::{CoCreateInstance, CLSCTX_ALL};
use windows::Win32::System::Threading::{
    OpenProcess, QueryFullProcessImageNameW, PROCESS_NAME_FORMAT, PROCESS_QUERY_LIMITED_INFORMATION,
};
use windows::core::PWSTR;
use log::{info, warn};

/// 切换指定进程的静音状态，返回最终静音状态
pub fn toggle_mute(process_name: &str) -> anyhow::Result<bool> {
    let target = process_name.to_lowercase();

    let enumerator: IMMDeviceEnumerator =
        unsafe { CoCreateInstance(&MMDeviceEnumerator, None, CLSCTX_ALL) }?;

    let device = unsafe { enumerator.GetDefaultAudioEndpoint(eRender, eConsole) }?;

    let session_mgr: IAudioSessionManager2 =
        unsafe { device.Activate::<IAudioSessionManager2>(CLSCTX_ALL, None)? };

    let session_enum: IAudioSessionEnumerator =
        unsafe { session_mgr.GetSessionEnumerator() }?;

    let count = unsafe { session_enum.GetCount()? };

    let mut first_mute: Option<bool> = None;
    let mut found = false;

    for i in 0..count {
        let session: IAudioSessionControl = match unsafe { session_enum.GetSession(i) } {
            Ok(s) => s,
            Err(_) => continue,
        };

        // QI 到 IAudioSessionControl2 获取 PID
        let session2: IAudioSessionControl2 = match session.cast() {
            Ok(s) => s,
            Err(_) => continue,
        };

        let pid = match unsafe { session2.GetProcessId() } {
            Ok(p) if p != 0 => p,
            _ => continue,
        };

        let name = match get_process_name(pid) {
            Some(n) => n,
            None => continue,
        };

        if name != target {
            continue;
        }

        // QI 到 ISimpleAudioVolume
        let volume: ISimpleAudioVolume = match session.cast() {
            Ok(v) => v,
            Err(_) => continue,
        };

        if first_mute.is_none() {
            match unsafe { volume.GetMute() } {
                Ok(m) => first_mute = Some(m.into()),
                Err(_) => continue,
            }
        }

        let new_state = !first_mute.unwrap();
        if let Err(e) = unsafe { volume.SetMute(new_state, std::ptr::null()) } {
            warn!("SetMute 失败: {}", e);
        } else {
            info!("{}: mute={}", process_name, new_state);
            found = true;
        }
    }

    if found {
        Ok(first_mute.map(|m| !m).unwrap_or(false))
    } else {
        anyhow::bail!("未找到 {} 的音频会话", process_name)
    }
}

fn get_process_name(pid: u32) -> Option<String> {
    unsafe {
        let handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid).ok()?;
        let mut buf = [0u16; 260];
        let mut size = 260u32;
        let ok = QueryFullProcessImageNameW(handle, PROCESS_NAME_FORMAT(0), PWSTR(buf.as_mut_ptr()), &mut size);
        let _ = CloseHandle(handle);

        if ok.is_err() {
            return None;
        }

        let path = String::from_utf16_lossy(&buf[..size as usize]);
        let name = path.rsplit('\\').next()?;
        Some(name.trim_end_matches(".exe").to_lowercase())
    }
}
