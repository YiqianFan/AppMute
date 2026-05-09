use std::collections::HashMap;
use windows::Win32::System::Com::{CoCreateInstance, CoInitializeEx, CLSCTX_ALL};
use windows::Win32::Media::Audio::{IMMDeviceEnumerator, MMDeviceEnumerator, eConsole, eRender};
use windows::core::Interface;
use log::{info, warn};

pub struct AudioManager {
    muted_processes: HashMap<String, bool>,
}

impl AudioManager {
    pub fn new() -> Result<Self, Box<dyn std::error::Error>> {
        Ok(Self {
            muted_processes: HashMap::new(),
        })
    }

    pub fn set_process_mute(&mut self, process_name: &str, mute: bool) -> Result<(), Box<dyn std::error::Error>> {
        let name_lower = process_name.to_lowercase();
        self.muted_processes.insert(name_lower, mute);

        unsafe {
            if let Err(e) = self.apply_mute_to_process(process_name, mute) {
                warn!("Could not apply mute to {}: {}", process_name, e);
            }
        }

        info!("Set '{}' mute = {}", process_name, mute);
        Ok(())
    }

    pub fn is_process_muted(&self, process_name: &str) -> bool {
        self.muted_processes.get(&process_name.to_lowercase()).copied().unwrap_or(false)
    }

    pub fn toggle_process_mute(&mut self, process_name: &str) -> Result<bool, Box<dyn std::error::Error>> {
        let current = self.is_process_muted(process_name);
        self.set_process_mute(process_name, !current)?;
        Ok(!current)
    }

    unsafe fn apply_mute_to_process(&self, process_name: &str, mute: bool) -> Result<(), Box<dyn std::error::Error>> {
        CoInitializeEx(None, windows::Win32::System::Com::COINIT_MULTITHREADED)?;

        let enumerator: IMMDeviceEnumerator = CoCreateInstance(&MMDeviceEnumerator, None, CLSCTX_ALL)?;

        let device = enumerator.GetDefaultAudioEndpoint(eRender, eConsole)?;

        let session_manager: windows::Win32::Media::Audio::Session::IAudioSessionManager2 = device.Activate(None)?;

        let session_enum: windows::Win32::Media::Audio::Session::IAudioSessionEnumerator = session_manager.GetSessionEnumerator()?;

        let count = session_enum.GetCount()?;

        for i in 0..count {
            if let Ok(session) = session_enum.GetSession(i) {
                let pid = session.GetProcessId().unwrap_or(0);
                if pid == 0 {
                    continue;
                }

                use windows::Win32::System::Threading::{OpenProcess, QueryFullProcessImageNameW, PROCESS_QUERY_LIMITED_INFORMATION, CloseHandle};

                if let Ok(handle) = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid as u32) {
                    let mut name_buf = [0u16; 260];
                    let mut size = 260u32;
                    if QueryFullProcessImageNameW(handle, 0, &mut name_buf, &mut size).is_ok() {
                        let full_name = String::from_utf16_lossy(&name_buf[..size as usize]);
                        if let Some(file_name) = full_name.rsplit('\\').next() {
                            let name = file_name.trim_end_matches(".exe").to_lowercase();
                            if name == process_name.to_lowercase() {
                                info!("Found matching process: {} (pid={})", name, pid);
                                if let Ok(sv) = session.cast::<windows::Win32::Media::Audio::Session::ISimpleAudioVolume>() {
                                    sv.SetMute(mute, None).ok();
                                    info!("Set mute={} for {}", mute, name);
                                }
                            }
                        }
                    }
                    CloseHandle(handle).ok();
                }
            }
        }

        Ok(())
    }
}