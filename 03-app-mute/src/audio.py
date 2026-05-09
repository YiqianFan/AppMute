"""Audio session management via Windows Core Audio API."""
from typing import Dict


class AudioManager:
    def __init__(self):
        self.muted_processes: Dict[str, bool] = {}

    def set_process_mute(self, process_name: str, mute: bool) -> None:
        name_lower = process_name.lower()
        self.muted_processes[name_lower] = mute

        try:
            from pycaw.pycaw import AudioUtilities
            sessions = AudioUtilities.GetAllSessions()
            for session in sessions:
                if session.Process:
                    pname = session.Process.name().lower()
                    if pname.endswith('.exe'):
                        pname = pname[:-4]
                    if pname == name_lower:
                        session.SimpleAudioVolume.SetMute(int(mute), None)
                        break
        except Exception as e:
            print(f"[Audio] Error: {e}")

    def toggle_process_mute(self, process_name: str) -> bool:
        current = self.is_process_muted(process_name)
        self.set_process_mute(process_name, not current)
        return not current

    def is_process_muted(self, process_name: str) -> bool:
        return self.muted_processes.get(process_name.lower(), False)

    def get_muted_processes(self) -> list:
        return [name for name, muted in self.muted_processes.items() if muted]
