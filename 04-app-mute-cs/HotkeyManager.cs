using System.Runtime.InteropServices;
using System.Windows.Forms;

public class HotkeyManager : IDisposable
{
    [DllImport("user32.dll", SetLastError = true)]
    static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);

    [DllImport("user32.dll", SetLastError = true)]
    static extern bool UnregisterHotKey(IntPtr hWnd, int id);

    // Modifier keys
    const uint MOD_ALT = 0x0001;
    const uint MOD_CONTROL = 0x0002;
    const uint MOD_SHIFT = 0x0004;
    const uint MOD_WIN = 0x0008;
    const uint MOD_NOREPEAT = 0x4000;

    const int WM_HOTKEY = 0x0312;

    // Virtual key codes
    const uint VK_F1 = 0x70, VK_F2 = 0x71, VK_F3 = 0x72, VK_F4 = 0x73;
    const uint VK_F5 = 0x74, VK_F6 = 0x75, VK_F7 = 0x76, VK_F8 = 0x77;
    const uint VK_F9 = 0x78, VK_F10 = 0x79, VK_F11 = 0x7A, VK_F12 = 0x7B;

    IntPtr _hwnd;
    int _nextId = 1;
    Dictionary<int, string> _hotkeyIds = new(); // id -> appName
    Dictionary<string, Action> _callbacks = new(); // appName -> callback

    public void Register(string hotkeyStr, string appName, Action callback)
    {
        var (mods, vk) = ParseHotkey(hotkeyStr);
        if (vk == 0) return;

        int id = _nextId++;
        if (RegisterHotKey(_hwnd, id, mods | MOD_NOREPEAT, vk))
        {
            _hotkeyIds[id] = appName;
            _callbacks[appName] = callback;
            Console.WriteLine($"[Hotkey] Registered: {hotkeyStr} -> {appName}");
        }
        else
        {
            Console.WriteLine($"[Hotkey] Failed to register: {hotkeyStr}");
        }
    }

    public void SetHwnd(IntPtr hwnd)
    {
        _hwnd = hwnd;
    }

    public bool ProcessMessage(int msg, IntPtr wParam)
    {
        if (msg != WM_HOTKEY) return false;

        int id = wParam.ToInt32();
        if (_hotkeyIds.TryGetValue(id, out var appName) && _callbacks.TryGetValue(appName, out var cb))
        {
            try { cb(); } catch { }
            return true;
        }
        return false;
    }

    public void UnregisterAll()
    {
        foreach (var id in _hotkeyIds.Keys)
            UnregisterHotKey(_hwnd, id);
        _hotkeyIds.Clear();
        _callbacks.Clear();
        Console.WriteLine("[Hotkey] All unregistered");
    }

    public void Dispose()
    {
        UnregisterAll();
    }

    static (uint mods, uint vk) ParseHotkey(string hotkeyStr)
    {
        hotkeyStr = hotkeyStr.ToLower().Trim();
        var parts = hotkeyStr.Split('+');
        uint mods = 0, vk = 0;

        foreach (var p in parts)
        {
            var part = p.Trim();
            switch (part)
            {
                case "ctrl": case "control": mods |= MOD_CONTROL; break;
                case "alt": mods |= MOD_ALT; break;
                case "shift": mods |= MOD_SHIFT; break;
                case "win": mods |= MOD_WIN; break;
                case "f1": vk = VK_F1; break;
                case "f2": vk = VK_F2; break;
                case "f3": vk = VK_F3; break;
                case "f4": vk = VK_F4; break;
                case "f5": vk = VK_F5; break;
                case "f6": vk = VK_F6; break;
                case "f7": vk = VK_F7; break;
                case "f8": vk = VK_F8; break;
                case "f9": vk = VK_F9; break;
                case "f10": vk = VK_F10; break;
                case "f11": vk = VK_F11; break;
                case "f12": vk = VK_F12; break;
                case "space": vk = 0x20; break;
                case "enter": vk = 0x0D; break;
                case "tab": vk = 0x09; break;
                case "esc": case "escape": vk = 0x1B; break;
                case "a": case "b": case "c": case "d": case "e": case "f":
                case "g": case "h": case "i": case "j": case "k": case "l":
                case "m": case "n": case "o": case "p": case "q": case "r":
                case "s": case "t": case "u": case "v": case "w": case "x":
                case "y": case "z":
                    vk = (uint)(part[0] - 'a' + 0x41); break;
                default:
                    if (part.Length == 1 && char.IsDigit(part[0]))
                        vk = (uint)(part[0] - '0' + 0x30);
                    break;
            }
        }

        return (mods, vk);
    }
}
