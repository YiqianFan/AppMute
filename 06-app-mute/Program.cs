using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace AppMute;

// ──────────────────────────── 配置模型 ────────────────────────────

record AppConfig(Dictionary<string, string> Hotkeys);

[JsonSerializable(typeof(AppConfig))]
partial class AppConfigContext : JsonSerializerContext;

// ──────────────────────────── COM 接口 ────────────────────────────

[ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
class MMDeviceEnumeratorComObject;

[ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDeviceEnumerator
{
    int EnumAudioEndpoints(int dataFlow, int stateMask, out IntPtr devices);
    int GetDefaultAudioEndpoint(int dataFlow, int role, out IMMDevice endpoint);
    int GetDevice(IntPtr id, out IMMDevice device);
    int RegisterEndpointNotificationCallback(IntPtr client);
    int UnregisterEndpointNotificationCallback(IntPtr client);
}

[ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDevice
{
    int Activate(ref Guid iid, uint dwClsCtx, IntPtr activationParams,
        [MarshalAs(UnmanagedType.IUnknown)] out object interfacePtr);
    int OpenPropertyStore(int stgmAccess, out IntPtr properties);
    int GetId(out IntPtr id);
    int GetState(out int state);
}

[ComImport, Guid("77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioSessionManager2
{
    int GetAudioSessionControl(ref Guid sessionId, int streamFlags, out IntPtr session);
    int GetSimpleAudioVolume(ref Guid sessionId, int streamFlags, out IntPtr volume);
    int GetSessionEnumerator(out IAudioSessionEnumerator sessionEnum);
    int RegisterSessionNotification(IntPtr notification);
    int UnregisterSessionNotification(IntPtr notification);
    int RegisterDuckNotification(IntPtr sessionId, IntPtr notification);
    int UnregisterDuckNotification(IntPtr notification);
}

[ComImport, Guid("E2F5BB11-0570-40CA-ACDD-3AA01277DEE8"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioSessionEnumerator
{
    int GetCount(out int sessionCount);
    int GetSession(int index, out IAudioSessionControl session);
}

[ComImport, Guid("F4B1A599-7266-4319-A8CA-E70ACB11E8CD"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioSessionControl
{
    int GetState(out int state);
    int GetDisplayName(out IntPtr name);
    int SetDisplayName(IntPtr name, ref Guid eventContext);
    int GetIconPath(out IntPtr path);
    int SetIconPath(IntPtr path, ref Guid eventContext);
    int GetGroupingParam(out Guid groupingId);
    int SetGroupingParam(ref Guid groupingId, ref Guid eventContext);
    int RegisterAudioSessionNotification(IntPtr client);
    int UnregisterAudioSessionNotification(IntPtr client);
}

[ComImport, Guid("BFB7FF88-7239-4FC9-8FA2-07C950BE9C6D"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioSessionControl2
{
    int GetState(out int state);
    int GetDisplayName(out IntPtr name);
    int SetDisplayName(IntPtr name, ref Guid eventContext);
    int GetIconPath(out IntPtr path);
    int SetIconPath(IntPtr path, ref Guid eventContext);
    int GetGroupingParam(out Guid groupingId);
    int SetGroupingParam(ref Guid groupingId, ref Guid eventContext);
    int RegisterAudioSessionNotification(IntPtr client);
    int UnregisterAudioSessionNotification(IntPtr client);
    int GetSessionIdentifier(out IntPtr retVal);
    int GetSessionInstanceIdentifier(out IntPtr retVal);
    int GetProcessId(out uint retVal);
    int IsSystemSoundsSession();
    int SetDuckingPreference([MarshalAs(UnmanagedType.Bool)] bool optOut);
}

[ComImport, Guid("87CE5498-68D6-44E5-9215-6DA47EF883D8"),
 InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface ISimpleAudioVolume
{
    int SetMasterVolume(float level, ref Guid eventContext);
    int GetMasterVolume(out float level);
    int SetMute([MarshalAs(UnmanagedType.Bool)] bool mute, ref Guid eventContext);
    int GetMute([MarshalAs(UnmanagedType.Bool)] out bool mute);
}

// ──────────────────────────── Win32 声明 ────────────────────────────

[StructLayout(LayoutKind.Sequential)]
struct MSG
{
    public IntPtr hwnd;
    public uint message;
    public IntPtr wParam, lParam;
    public uint time;
    public int ptX, ptY;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
struct WNDCLASSW
{
    public uint style;
    public IntPtr lpfnWndProc; // 函数指针，用 Marshal.GetFunctionPointerForDelegate 转换
    public int cbClsExtra, cbWndExtra;
    public IntPtr hInstance, hIcon, hCursor, hbrBackground;
    public string? lpszMenuName;
    public string lpszClassName;
}

// 完整 NOTIFYICONDATAW（V4），cbSize 决定实际使用版本
[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
struct NOTIFYICONDATAW
{
    public uint cbSize;
    public IntPtr hWnd;
    public uint uID, uFlags, uCallbackMessage;
    public IntPtr hIcon;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
    public string szTip;
    public uint dwState, dwStateMask;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
    public string szInfo;
    public uint uVersion;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
    public string szInfoTitle;
    public uint dwInfoFlags;
    public Guid guidItem;
    public IntPtr hBalloonIcon;
}

[StructLayout(LayoutKind.Sequential)]
struct POINT { public int X, Y; }

delegate IntPtr WndProcDelegate(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

static class W
{
    public const uint WM_DESTROY = 0x0002, WM_COMMAND = 0x0111, WM_TIMER = 0x0113,
        WM_HOTKEY = 0x0312, WM_RBUTTONUP = 0x0205, WM_LBUTTONDBLCLK = 0x0203;
    public const uint NIM_ADD = 0, NIM_DELETE = 2;
    public const uint NIF_MESSAGE = 1, NIF_ICON = 2, NIF_TIP = 4, NIF_SHOWTIP = 0x80;
    public const uint MOD_ALT = 1, MOD_CONTROL = 2, MOD_SHIFT = 4, MOD_WIN = 8,
        MOD_NOREPEAT = 0x4000;
    public const uint COINIT_APARTMENTTHREADED = 2, CLSCTX_ALL = 0x17;
    public const uint VK_SPACE = 0x20, VK_RETURN = 0x0D, VK_ESCAPE = 0x1B, VK_TAB = 0x09;
    // WS_EX_TOOLWINDOW: 不在任务栏显示，不在 Alt-Tab 列表出现
    public const uint WS_EX_TOOLWINDOW = 0x0080;

    [DllImport("user32.dll", SetLastError = true)] public static extern bool RegisterHotKey(IntPtr hWnd, int id, uint mod, uint vk);
    [DllImport("user32.dll")] public static extern bool UnregisterHotKey(IntPtr hWnd, int id);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] public static extern IntPtr CreateWindowExW(uint ex, string cls, string name, uint style, int x, int y, int w, int h, IntPtr parent, IntPtr menu, IntPtr inst, IntPtr param);
    [DllImport("user32.dll")] public static extern bool DestroyWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern void PostQuitMessage(int code);
    [DllImport("user32.dll")] public static extern bool GetMessage(out MSG msg, IntPtr hWnd, uint min, uint max);
    [DllImport("user32.dll")] public static extern bool TranslateMessage(ref MSG msg);
    [DllImport("user32.dll")] public static extern IntPtr DispatchMessage(ref MSG msg);
    [DllImport("user32.dll")] public static extern IntPtr DefWindowProcW(IntPtr hWnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] public static extern ushort RegisterClassW(ref WNDCLASSW wc);
    [DllImport("shell32.dll", CharSet = CharSet.Unicode, SetLastError = true)] public static extern bool Shell_NotifyIconW(uint msg, ref NOTIFYICONDATAW data);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr CreatePopupMenu();
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern bool AppendMenuW(IntPtr menu, uint flags, uint id, string text);
    [DllImport("user32.dll")] public static extern int TrackPopupMenu(IntPtr menu, uint flags, int x, int y, int reserved, IntPtr hWnd, IntPtr rect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool DestroyMenu(IntPtr menu);
    [DllImport("user32.dll")] public static extern IntPtr SetTimer(IntPtr hWnd, uint id, uint ms, IntPtr proc);
    [DllImport("user32.dll")] public static extern bool KillTimer(IntPtr hWnd, uint id);
    [DllImport("user32.dll")] public static extern IntPtr LoadIconW(IntPtr inst, IntPtr name);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT pt);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr GetModuleHandleW(string? name);
    [DllImport("ole32.dll")] public static extern int CoInitializeEx(IntPtr reserved, uint coInit);
    [DllImport("ole32.dll")] public static extern void CoUninitialize();
}

// ──────────────────────────── 主程序 ────────────────────────────

class Program
{
    static IntPtr _hwnd;
    static Dictionary<int, string> _hotkeys = new();
    static int _nextId = 1;
    static string _configPath = "";
    static DateTime _lastConfigWrite;
    static IntPtr _hMenu;
    static NOTIFYICONDATAW _trayData;
    static WndProcDelegate? _wndProc;
    static readonly string _logPath = Path.Combine(AppContext.BaseDirectory, "app-mute.log");

    const uint WM_TRAY = 0x8001;
    const int MENU_EXIT = 1, MENU_EDIT = 2;
    const uint TIMER_CFG = 1;

    static void Main()
    {
        W.CoInitializeEx(IntPtr.Zero, W.COINIT_APARTMENTTHREADED);

        _configPath = Path.Combine(AppContext.BaseDirectory, "config.json");
        EnsureDefaultConfig();

        CreateWindow(); // 先建窗口，才能注册热键

        LoadConfig();
        CreateTrayIcon();
        W.SetTimer(_hwnd, TIMER_CFG, 2000, IntPtr.Zero);

        Log("AppMute 已启动 — 按热键切换应用静音，右键托盘退出");

        while (W.GetMessage(out MSG msg, IntPtr.Zero, 0, 0))
        {
            W.TranslateMessage(ref msg);
            W.DispatchMessage(ref msg);
        }

        foreach (var id in _hotkeys.Keys) W.UnregisterHotKey(_hwnd, id);
        W.Shell_NotifyIconW(W.NIM_DELETE, ref _trayData);
        W.KillTimer(_hwnd, TIMER_CFG);
        if (_hMenu != IntPtr.Zero) W.DestroyMenu(_hMenu);
        W.DestroyWindow(_hwnd);
        W.CoUninitialize();
    }

    static IntPtr WndProc(IntPtr hWnd, uint msg, IntPtr wp, IntPtr lp)
    {
        switch (msg)
        {
            case W.WM_HOTKEY:
                if (_hotkeys.TryGetValue(wp.ToInt32(), out string? proc))
                    ToggleMute(proc);
                return IntPtr.Zero;

            case WM_TRAY:
                switch (lp.ToInt32())
                {
                    case (int)W.WM_RBUTTONUP:
                        ShowTrayMenu();
                        break;
                    case (int)W.WM_LBUTTONDBLCLK:
                        OpenConfig();
                        break;
                }
                return IntPtr.Zero;

            case W.WM_COMMAND:
                int cmd = wp.ToInt32() & 0xFFFF;
                if (cmd == MENU_EXIT) { W.DestroyWindow(_hwnd); W.PostQuitMessage(0); }
                else if (cmd == MENU_EDIT) OpenConfig();
                return IntPtr.Zero;

            case W.WM_TIMER:
                if (wp.ToInt32() == TIMER_CFG) CheckConfigReload();
                return IntPtr.Zero;

            case W.WM_DESTROY:
                W.PostQuitMessage(0);
                return IntPtr.Zero;
        }
        return W.DefWindowProcW(hWnd, msg, wp, lp);
    }

    static void OpenConfig()
    {
        try { Process.Start(new ProcessStartInfo(_configPath) { UseShellExecute = true }); }
        catch (Exception ex) { Log($"打开配置失败: {ex.Message}"); }
    }

    // ──── 音频控制 ────

    static void ToggleMute(string processName)
    {
        try
        {
            var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumeratorComObject();

            int hr = enumerator.GetDefaultAudioEndpoint(0, 0, out IMMDevice? device);
            if (hr != 0) { Log($"获取音频设备失败: 0x{hr:X8}"); return; }

            var iidMgr = typeof(IAudioSessionManager2).GUID;
            hr = device.Activate(ref iidMgr, W.CLSCTX_ALL, IntPtr.Zero, out object? mgrObj);
            if (hr != 0) { Log($"激活 SessionManager 失败: 0x{hr:X8}"); return; }
            var mgr = (IAudioSessionManager2)mgrObj;

            hr = mgr.GetSessionEnumerator(out IAudioSessionEnumerator? sessionEnum);
            if (hr != 0) { Log($"获取 SessionEnumerator 失败: 0x{hr:X8}"); return; }

            hr = sessionEnum.GetCount(out int count);
            if (hr != 0) { Log($"获取会话数量失败: 0x{hr:X8}"); return; }

            bool found = false;
            bool? firstMuteState = null;

            for (int i = 0; i < count; i++)
            {
                hr = sessionEnum.GetSession(i, out IAudioSessionControl? session);
                if (hr != 0) continue;

                var control2 = (IAudioSessionControl2)session;
                hr = control2.GetProcessId(out uint pid);
                if (hr != 0 || pid == 0) continue;

                string procName;
                try { procName = Process.GetProcessById((int)pid).ProcessName; }
                catch { continue; }

                if (!procName.Equals(processName, StringComparison.OrdinalIgnoreCase))
                    continue;

                var volume = (ISimpleAudioVolume)session;
                var ctx = Guid.Empty;

                if (firstMuteState == null)
                {
                    volume.GetMute(out bool currentMute);
                    firstMuteState = currentMute;
                }

                volume.SetMute(!firstMuteState.Value, ref ctx);
                found = true;
            }

            if (found && firstMuteState != null)
                Log($"{processName} → {(!firstMuteState.Value ? "已静音" : "已取消静音")}");
            else
                Log($"未找到 {processName} 的音频会话（应用可能未在播放声音）");
        }
        catch (Exception ex)
        {
            Log($"切换静音失败: {ex.Message}");
        }
    }

    // ──── 配置管理 ────

    static void EnsureDefaultConfig()
    {
        if (File.Exists(_configPath)) return;
        var defaults = new AppConfig(new Dictionary<string, string>
        {
            ["F1"] = "chrome",
            ["ctrl+alt+1"] = "spotify",
            ["ctrl+alt+2"] = "discord",
        });
        File.WriteAllText(_configPath,
            JsonSerializer.Serialize(defaults, AppConfigContext.Default.AppConfig));
        Log($"已创建默认配置: {_configPath}");
    }

    static void LoadConfig()
    {
        try
        {
            var json = File.ReadAllText(_configPath);
            var cfg = JsonSerializer.Deserialize(json, AppConfigContext.Default.AppConfig);
            if (cfg?.Hotkeys == null || cfg.Hotkeys.Count == 0)
            { Log("配置文件中没有热键映射"); return; }

            _lastConfigWrite = File.GetLastWriteTime(_configPath);

            foreach (var id in _hotkeys.Keys) W.UnregisterHotKey(_hwnd, id);
            _hotkeys.Clear();
            _nextId = 1;

            foreach (var (keyStr, procName) in cfg.Hotkeys)
            {
                if (TryParseHotkey(keyStr, out uint mod, out uint vk))
                {
                    int id = _nextId++;
                    if (W.RegisterHotKey(_hwnd, id, mod | W.MOD_NOREPEAT, vk))
                    {
                        _hotkeys[id] = procName;
                        Log($"热键: {keyStr} → {procName}");
                    }
                    else
                        Log($"注册热键失败（可能被占用）: {keyStr}");
                }
                else
                    Log($"无法解析热键: {keyStr}");
            }
        }
        catch (Exception ex)
        {
            Log($"读取配置失败: {ex.Message}");
        }
    }

    static void CheckConfigReload()
    {
        if (!File.Exists(_configPath)) return;
        var wt = File.GetLastWriteTime(_configPath);
        if (wt != _lastConfigWrite)
        {
            Log("检测到配置文件变化，重新加载...");
            LoadConfig();
        }
    }

    // ──── 热键解析 ────

    static bool TryParseHotkey(string keyStr, out uint mod, out uint vk)
    {
        mod = 0; vk = 0;
        var parts = keyStr.Split('+');
        string? keyPart = null;

        foreach (var p in parts)
        {
            var s = p.Trim().ToLowerInvariant();
            if (s == "ctrl" || s == "control") mod |= W.MOD_CONTROL;
            else if (s == "alt") mod |= W.MOD_ALT;
            else if (s == "shift") mod |= W.MOD_SHIFT;
            else if (s == "win" || s == "windows") mod |= W.MOD_WIN;
            else keyPart = s;
        }
        if (keyPart == null) return false;

        if (keyPart.StartsWith('f') && keyPart.Length <= 3)
        {
            if (int.TryParse(keyPart[1..], out int fNum) && fNum >= 1 && fNum <= 12)
            { vk = 0x6F + (uint)fNum; return true; }
        }
        if (keyPart.Length == 1 && keyPart[0] >= 'a' && keyPart[0] <= 'z')
        { vk = (uint)char.ToUpper(keyPart[0]); return true; }
        if (keyPart.Length == 1 && keyPart[0] >= '0' && keyPart[0] <= '9')
        { vk = (uint)keyPart[0]; return true; }
        vk = keyPart switch
        {
            "space" => W.VK_SPACE,
            "enter" or "return" => W.VK_RETURN,
            "esc" or "escape" => W.VK_ESCAPE,
            "tab" => W.VK_TAB,
            _ => 0
        };
        return vk != 0;
    }

    // ──── 窗口与托盘 ────

    static void CreateWindow()
    {
        _wndProc = WndProc;
        IntPtr hInst = W.GetModuleHandleW(null);

        var wc = new WNDCLASSW
        {
            lpfnWndProc = Marshal.GetFunctionPointerForDelegate(_wndProc),
            hInstance = hInst,
            lpszClassName = "AppMuteWnd"
        };
        W.RegisterClassW(ref wc);

        _hwnd = W.CreateWindowExW(
            W.WS_EX_TOOLWINDOW,
            "AppMuteWnd", "AppMute",
            0, 0, 0, 0, 0,
            IntPtr.Zero, IntPtr.Zero, hInst, IntPtr.Zero);

        if (_hwnd == IntPtr.Zero)
            Log($"创建窗口失败: err={Marshal.GetLastWin32Error()}");
    }

    static void CreateTrayIcon()
    {
        IntPtr hIcon = W.LoadIconW(IntPtr.Zero, new IntPtr(32516)); // IDI_INFORMATION
        _trayData = new NOTIFYICONDATAW
        {
            hWnd = _hwnd,
            uID = 1,
            uFlags = W.NIF_MESSAGE | W.NIF_ICON | W.NIF_TIP,
            uCallbackMessage = WM_TRAY,
            hIcon = hIcon,
            szTip = "AppMute — 应用静音",
            szInfo = "",
            szInfoTitle = "",
        };
        _trayData.cbSize = (uint)Marshal.SizeOf<NOTIFYICONDATAW>();

        bool ok = W.Shell_NotifyIconW(W.NIM_ADD, ref _trayData);
        if (!ok) Log($"添加托盘图标失败: err={Marshal.GetLastWin32Error()}");
        else Log("托盘图标已添加");
    }

    static void ShowTrayMenu()
    {
        if (_hMenu == IntPtr.Zero)
        {
            _hMenu = W.CreatePopupMenu();
            W.AppendMenuW(_hMenu, 0, MENU_EDIT, "编辑配置");
            W.AppendMenuW(_hMenu, 0x800, 0, "");
            W.AppendMenuW(_hMenu, 0, MENU_EXIT, "退出");
        }
        W.GetCursorPos(out POINT pt);
        W.SetForegroundWindow(_hwnd);
        W.TrackPopupMenu(_hMenu, 0x100, pt.X, pt.Y, 0, _hwnd, IntPtr.Zero);
    }

    // ──── 日志 ────

    static void Log(string msg)
    {
        string line = $"[{DateTime.Now:HH:mm:ss}] {msg}";
        Console.WriteLine(line);
        try { File.AppendAllText(_logPath, line + "\n"); } catch { }
    }
}
