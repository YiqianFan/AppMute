"""AppMute - Global hotkey per-app mute tool."""
import sys
import threading
import subprocess
import time
from pathlib import Path

src_dir = Path(__file__).parent
sys.path.insert(0, str(src_dir))

if getattr(sys, 'frozen', False) and hasattr(sys, '_MEIPASS'):
    base_dir = Path(sys._MEIPASS).parent
    config_path = (base_dir / "_internal" / "config.toml").resolve()
else:
    base_dir = src_dir.parent
    config_path = (base_dir / "config.toml").resolve()

from audio import AudioManager
from config import Config
from hotkey import HotkeyManager

try:
    import pystray
    from PIL import Image, ImageDraw
    TRAY_AVAILABLE = True
except ImportError:
    TRAY_AVAILABLE = False


class AppMute:
    def __init__(self):
        self.audio = AudioManager()
        self.config = Config.load(str(config_path))
        self.hotkey = HotkeyManager()
        self._tray = None
        self._running = True

    def _make_icon(self) -> Image.Image:
        img = Image.new('RGB', (64, 64), color=(50, 50, 50))
        draw = ImageDraw.Draw(img)
        draw.ellipse([8, 18, 32, 46], fill='white')
        draw.polygon([(28, 14), (58, 4), (58, 60), (28, 50)], fill='white')
        return img

    def _on_tray_quit(self):
        self._running = False
        self.hotkey.unregister_all()
        if self._tray:
            self._tray.stop()
        sys.exit(0)

    def _on_tray_config(self):
        subprocess.Popen([sys.executable, str(src_dir / "main.py"), "--config"])

    def _watch_config(self):
        """Watch config file for changes using stat polling."""
        last_mtime = 0.0
        while self._running:
            try:
                mtime = config_path.stat().st_mtime
                if mtime != last_mtime:
                    last_mtime = mtime
                    self._reload_config()
            except FileNotFoundError:
                pass
            time.sleep(1)  # poll every second (not critical)

    def _reload_config(self):
        self.hotkey.unregister_all()
        self.audio = AudioManager()
        self.config = Config.load(str(config_path))
        for hotkey_str, app_name in self.config.hotkeys.items():
            self.hotkey.register(hotkey_str, app_name, self._on_hotkey)

    def _on_hotkey(self, app_name: str):
        try:
            new_state = self.audio.toggle_process_mute(app_name)
            if self._tray:
                self._tray.title = f"AppMute: {'已静音' if new_state else '无应用静音'}"
        except Exception:
            pass

    def _start_tray(self):
        if not TRAY_AVAILABLE:
            return
        icon_image = self._make_icon()
        menu = pystray.Menu(
            pystray.MenuItem("配置", self._on_tray_config),
            pystray.MenuItem("退出", self._on_tray_quit),
        )
        self._tray = pystray.Icon("app_mute", icon_image, title="AppMute", menu=menu)
        t = threading.Thread(target=self._tray.run, daemon=True)
        t.start()

    def run(self):
        self._reload_config()
        self.hotkey.start()
        self._start_tray()

        t = threading.Thread(target=self._watch_config, daemon=True)
        t.start()

        while self._running:
            time.sleep(1)


def main():
    if "--config" in sys.argv or "-c" in sys.argv:
        from gui import ConfigGUI
        ConfigGUI(str(config_path))
        return
    AppMute().run()


if __name__ == "__main__":
    main()
