"""Global hotkey registration using Win32 RegisterHotKey API."""
import ctypes
from ctypes import wintypes
import threading
import time
from typing import Dict, Callable


# Win32 constants
MOD_ALT = 0x0001
MOD_CONTROL = 0x0002
MOD_SHIFT = 0x0004
MOD_WIN = 0x0008
MOD_NOREPEAT = 0x4000

WM_HOTKEY = 0x0312

# ctypes definitions
user32 = ctypes.windll.user32

RegisterHotKey = user32.RegisterHotKey
UnregisterHotKey = user32.UnregisterHotKey
GetMessageW = user32.GetMessageW
PeekMessageW = user32.PeekMessageW
TranslateMessage = user32.TranslateMessage
DispatchMessageW = user32.DispatchMessageW
PostThreadMessageW = user32.PostThreadMessageW


class HotkeyManager:
    def __init__(self):
        self._hotkeys: Dict[int, str] = {}  # id -> app_name
        self._callbacks: Dict[str, Callable] = {}
        self._next_id = 1
        self._running = False
        self._thread = None
        self._hwnd = None

    def register(self, hotkey_str: str, app_name: str, callback: Callable[[str], None]) -> None:
        """Register a hotkey. callback receives app_name."""
        self._callbacks[app_name.lower()] = callback
        # Store for later registration in message loop
        if not hasattr(self, '_pending'):
            self._pending = []
        self._pending.append((hotkey_str, app_name))

    def unregister_all(self) -> None:
        if self._hwnd and self._running:
            for hid, _ in self._hotkeys.items():
                UnregisterHotKey(self._hwnd, hid)
        self._hotkeys.clear()
        self._running = False
        print("[Hotkey] Unregistered all")

    def start(self) -> None:
        self._running = True
        self._thread = threading.Thread(target=self._message_loop, daemon=True)
        self._thread.start()
        print(f"[Hotkey] {len(self._pending)} hotkeys registered")

    def _message_loop(self):
        # Register hotkeys in this thread (which has a message queue)
        self._register_pending_hotkeys()

        while self._running:
            # Process Windows messages
            msg = wintypes.MSG()
            if PeekMessageW(ctypes.byref(msg), None, 0, 0, 1):  # PM_REMOVE
                if msg.message == WM_HOTKEY:
                    hid = msg.wParam
                    if hid in self._hotkeys:
                        app_name = self._hotkeys[hid]
                        if app_name.lower() in self._callbacks:
                            try:
                                self._callbacks[app_name.lower()](app_name)
                            except Exception as e:
                                print(f"[Hotkey] Callback error: {e}")
                TranslateMessage(ctypes.byref(msg))
                DispatchMessageW(ctypes.byref(msg))
            else:
                time.sleep(0.01)

    def _register_pending_hotkeys(self):
        """Register all pending hotkeys. Must be called from the message loop thread."""
        # Create a message-only window
        wc = ctypes.Structure
        class WNDCLASSEX(ctypes.Structure):
            _fields_ = [
                ("cbSize", wintypes.UINT),
                ("style", wintypes.UINT),
                ("lpfnWndProc", ctypes.WINFUNC),
                ("cbClsExtra", ctypes.c_int),
                ("cbWndExtra", ctypes.c_int),
                ("hInstance", wintypes.HANDLE),
                ("hIcon", wintypes.HANDLE),
                ("hCursor", wintypes.HANDLE),
                ("hbrBackground", wintypes.HANDLE),
                ("lpszMenuName", wintypes.LPCWSTR),
                ("lpszClassName", wintypes.LPCWSTR),
                ("hIconSm", wintypes.HANDLE),
            ]

        hinst = user32.GetModuleHandleW(None)

        class_temp = ctypes.WINFUNC(ctypes.c_long, ctypes.c_long, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint)

        def wndproc(hwnd, msg, wparam, lparam):
            return user32.DefWindowProcW(hwnd, msg, wparam, lparam)

        wc = WNDCLASSEX()
        wc.cbSize = ctypes.sizeof(WNDCLASSEX)
        wc.style = 0
        wc.lpfnWndProc = class_temp(wndproc)
        wc.hInstance = hinst
        wc.lpszClassName = "AppMuteHotkeyClass"

        atom = user32.RegisterClassExW(ctypes.byref(wc))
        if atom == 0:
            print(f"[Hotkey] RegisterClass failed: {ctypes.get_last_error()}")
            return

        self._hwnd = user32.CreateWindowExW(
            0, wc.lpszClassName, None, 0,
            0, 0, 0, 0,
            None, None, hinst, None
        )

        if self._hwnd == 0:
            print(f"[Hotkey] CreateWindowEx failed: {ctypes.get_last_error()}")
            return

        for hotkey_str, app_name in getattr(self, '_pending', []):
            self._register_one(self._hwnd, self._next_id, hotkey_str, app_name)
            self._next_id += 1

    def _register_one(self, hwnd, hotkey_id, hotkey_str, app_name):
        mods, vk = self._parse_hotkey(hotkey_str)
        if vk == 0:
            print(f"[Hotkey] Invalid hotkey: {hotkey_str}")
            return

        # Add MOD_NOREPEAT to prevent key-hold repeat events
        result = RegisterHotKey(hwnd, hotkey_id, mods | MOD_NOREPEAT, vk)
        if result == 0:
            print(f"[Hotkey] RegisterHotKey failed for '{hotkey_str}': {ctypes.get_last_error()}")
        else:
            self._hotkeys[hotkey_id] = app_name
            print(f"[Hotkey] Registered: {hotkey_str} -> {app_name}")

    @staticmethod
    def _parse_hotkey(hotkey_str):
        hotkey_str = hotkey_str.lower().strip()
        parts = hotkey_str.split('+')
        parts = [p.strip() for p in parts]

        mods = 0
        vk = 0

        for part in parts:
            if part == 'ctrl' or part == 'control':
                mods |= MOD_CONTROL
            elif part == 'alt':
                mods |= MOD_ALT
            elif part == 'shift':
                mods |= MOD_SHIFT
            elif part == 'win':
                mods |= MOD_WIN
            else:
                vk = HotkeyManager._name_to_vk(part)

        return mods, vk

    @staticmethod
    def _name_to_vk(name):
        vk_map = {
            'f1': 0x70, 'f2': 0x71, 'f3': 0x72, 'f4': 0x73,
            'f5': 0x74, 'f6': 0x75, 'f7': 0x76, 'f8': 0x77,
            'f9': 0x78, 'f10': 0x79, 'f11': 0x7A, 'f12': 0x7B,
            'a': 0x41, 'b': 0x42, 'c': 0x43, 'd': 0x44, 'e': 0x45,
            'f': 0x46, 'g': 0x47, 'h': 0x48, 'i': 0x49, 'j': 0x4A,
            'k': 0x4B, 'l': 0x4C, 'm': 0x4D, 'n': 0x4E, 'o': 0x4F,
            'p': 0x50, 'q': 0x51, 'r': 0x52, 's': 0x53, 't': 0x54,
            'u': 0x55, 'v': 0x56, 'w': 0x57, 'x': 0x58, 'y': 0x59,
            'z': 0x5A,
            '0': 0x30, '1': 0x31, '2': 0x32, '3': 0x33, '4': 0x34,
            '5': 0x35, '6': 0x36, '7': 0x37, '8': 0x38, '9': 0x39,
            'space': 0x20, 'enter': 0x0D, 'tab': 0x09, 'esc': 0x1B,
            'escape': 0x1B, 'backspace': 0x08, 'delete': 0x2E,
            'insert': 0x2D, 'home': 0x24, 'end': 0x23,
            'page up': 0x21, 'page down': 0x22,
            'left': 0x25, 'up': 0x26, 'right': 0x27, 'down': 0x28,
            '-': 0xBD, '=': 0xBB, ',': 0xBC, '.': 0xBE,
            '/': 0xBF, '\\': 0xDC, ';': 0xBA, "'": 0xDE,
            '[': 0xDB, ']': 0xDD, '`': 0xC0,
        }
        return vk_map.get(name, 0)
