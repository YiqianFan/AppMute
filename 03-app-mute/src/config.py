"""Configuration file parsing."""
import toml
from pathlib import Path
from typing import Dict


class Config:
    """App configuration."""

    def __init__(self, hotkeys: Dict[str, str], check_interval: int = 1000):
        self.hotkeys = hotkeys  # hotkey_str -> app_name
        self.check_interval = check_interval

    @classmethod
    def load(cls, path: str = "config.toml") -> "Config":
        """Load config from TOML file."""
        p = Path(path)
        if not p.exists():
            print(f"[Config] No config.toml found, using defaults")
            return cls({})

        data = toml.load(p)
        hotkeys = {}
        if "hotkeys" in data:
            for key, val in data["hotkeys"].items():
                hotkeys[key.lower()] = val.lower()

        interval = 1000
        if "general" in data and "check_interval" in data["general"]:
            interval = int(data["general"]["check_interval"])

        print(f"[Config] Loaded {len(hotkeys)} hotkeys, interval={interval}ms")
        return cls(hotkeys, interval)
