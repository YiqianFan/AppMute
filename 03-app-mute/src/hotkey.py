"""Global hotkey registration using keyboard.add_hotkey (Windows low-level hook)."""
import time
from typing import Dict, Callable


class HotkeyManager:
    def __init__(self):
        self._callbacks: Dict[str, Callable] = {}
        self._last_fired: Dict[str, float] = {}

    def register(self, hotkey_str: str, app_name: str, callback: Callable[[str], None]) -> None:
        import keyboard

        key = hotkey_str.lower().strip()

        def wrapped():
            now = time.time()
            if now - self._last_fired.get(key, 0) < 0.4:
                return
            self._last_fired[key] = now
            try:
                callback(app_name)
            except Exception as e:
                print(f"[Hotkey] Error: {e}")

        try:
            keyboard.add_hotkey(key, wrapped, suppress=True)
            self._callbacks[key] = (callback, app_name)
        except ValueError:
            pass

    def unregister_all(self) -> None:
        import keyboard
        keyboard.unhook_all()
        self._callbacks.clear()
        self._last_fired.clear()

    def start(self) -> None:
        pass
