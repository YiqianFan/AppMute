using System.Runtime.InteropServices;
using System.Windows.Forms;
using System.Drawing;
using System.IO;

class Program : ApplicationContext
{
    HotkeyManager _hotkey = new();
    AudioManager _audio = new();
    Config _config;
    NotifyIcon _tray = new();
    string _configPath = "";
    DateTime _lastConfigMtime;
    static string _logPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "app-mute.log");

    static void Log(string msg) => File.AppendAllText(_logPath, $"[{DateTime.Now:HH:mm:ss}] {msg}\n");

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    static extern ushort RegisterClass(ref WNDCLASS lpWndClass);

    [DllImport("user32.dll")]
    static extern bool DestroyWindow(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    static extern IntPtr CreateWindowExW(uint dwExStyle, string lpClassName, string? lpWindowName,
        uint dwStyle, int X, int Y, int nWidth, int nHeight, IntPtr hWndParent, IntPtr hMenu,
        IntPtr hInstance, IntPtr lpParam);

    [DllImport("user32.dll")]
    static extern IntPtr DefWindowProc(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    static extern bool GetMessage(ref MSG lpMsg, IntPtr hWnd, uint wMsgFilterMin, uint wMsgFilterMax);

    [DllImport("user32.dll")]
    static extern bool TranslateMessage(ref MSG lpMsg);

    [DllImport("user32.dll")]
    static extern IntPtr DispatchMessage(ref MSG lpMsg);

    const int WM_HOTKEY = 0x0312;
    const int WM_DESTROY = 0x0002;

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    delegate IntPtr WndProcDelegate(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

    IntPtr _hwnd;
    Thread _msgThread = null!;
    volatile bool _running = true;

    WndProcDelegate _wndProc;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct WNDCLASS
    {
        public uint style;
        public IntPtr lpfnWndProc;
        public int cbClsExtra;
        public int cbWndExtra;
        public IntPtr hInstance;
        public IntPtr hIcon;
        public IntPtr hCursor;
        public IntPtr hbrBackground;
        public string? lpszMenuName;
        public string? lpszClassName;
        public IntPtr hIconSm;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct MSG
    {
        public IntPtr hwnd;
        public uint message;
        public IntPtr wParam;
        public IntPtr lParam;
        public uint time;
        public Point pt;
    }

    static void Main(string[] args)
    {
        Log("Program starting");
        string baseDir = AppDomain.CurrentDomain.BaseDirectory;
        string configPath = Path.Combine(baseDir, "config.json");

        if (args.Contains("--config") || args.Contains("-c"))
        {
            MessageBox.Show("请直接编辑 config.json 文件配置快捷键。", "AppMute");
            return;
        }

        try
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Log("Application context initialized");

            var program = new Program(configPath);
            Log("Program created, entering message loop");
            Application.Run(program);
            Log("Application.Run returned");
        }
        catch (Exception ex)
        {
            Log($"FATAL: {ex}");
            MessageBox.Show($"启动失败:\n{ex}", "AppMute 错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    Program(string configPath)
    {
        _configPath = configPath;
        AudioManager.Log = Log;
        Log($"Config path: {configPath}");

        try
        {
            _wndProc = WndProc;
            _hwnd = CreateMessageWindow();
            Log($"Window created: {_hwnd}");

            _config = Config.Load(_configPath);
            Log($"Config loaded: {_config.Hotkeys.Count} hotkeys");

            _lastConfigMtime = GetConfigMtime();
            RegisterHotkeys();
            Log("Hotkeys registered");

            InitTray();
            Log("Tray initialized");

            StartMessageLoop();
            StartConfigWatcher();
            Log("Threads started");
        }
        catch (Exception ex)
        {
            Log($"Constructor error: {ex}");
            throw;
        }
    }

    IntPtr CreateMessageWindow()
    {
        IntPtr hInstance = Marshal.GetHINSTANCE(typeof(Program).Assembly.GetModules()[0]);

        var wc = new WNDCLASS();
        wc.style = 0;
        wc.lpfnWndProc = Marshal.GetFunctionPointerForDelegate(_wndProc);
        wc.hInstance = hInstance;
        wc.lpszClassName = "AppMuteCS";

        ushort atom = RegisterClass(ref wc);
        if (atom == 0)
            throw new Exception("RegisterClass failed: " + Marshal.GetLastWin32Error());

        IntPtr hwnd = CreateWindowExW(0, "AppMuteCS", null!, 0x80000000,
            -100, -100, 1, 1, IntPtr.Zero, IntPtr.Zero, hInstance, IntPtr.Zero);

        if (hwnd == IntPtr.Zero)
            throw new Exception("CreateWindow failed: " + Marshal.GetLastWin32Error());

        _hotkey.SetHwnd(hwnd);
        return hwnd;
    }

    IntPtr WndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
    {
        if (msg == WM_HOTKEY)
        {
            Log($"WM_HOTKEY received: wParam={wParam}");
            if (_hotkey.ProcessMessage((int)msg, wParam))
                return IntPtr.Zero;
        }
        else if (msg == WM_DESTROY)
        {
            _running = false;
            return IntPtr.Zero;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void RegisterHotkeys()
    {
        foreach (var kv in _config.Hotkeys)
        {
            _hotkey.Register(kv.Key, kv.Value, () => OnHotkey(kv.Value));
        }
    }

    void OnHotkey(string appName)
    {
        Log($"OnHotkey triggered: {appName}");
        try
        {
            bool muted = _audio.ToggleProcessMute(appName);
            Log($"ToggleProcessMute result: muted={muted}");
            Console.WriteLine($"[AppMute] '{appName}' muted={muted}");
            string title = muted ? $"已静音: {appName}" : "AppMute";
            if (title.Length > 63) title = title[..63];
            _tray.Text = title;
            _tray.ShowBalloonTip(1000, "AppMute", muted ? $"已静音: {appName}" : $"已取消静音: {appName}", ToolTipIcon.Info);
        }
        catch (Exception ex)
        {
            Log($"OnHotkey error: {ex}");
        }
    }

    void InitTray()
    {
        _tray.Icon = CreateSpeakerIcon();
        _tray.Text = "AppMute";
        _tray.Visible = true;

        var menu = new ContextMenuStrip();
        menu.Items.Add("配置", null, (_, _) =>
            MessageBox.Show("请编辑 config.json 后重启程序。", "配置"));
        menu.Items.Add("-");
        menu.Items.Add("退出", null, (_, _) => { _running = false; Application.Exit(); });
        _tray.ContextMenuStrip = menu;
        _tray.DoubleClick += (_, _) => { };
    }

    Icon CreateSpeakerIcon()
    {
        using var bmp = new Bitmap(32, 32);
        using var g = Graphics.FromImage(bmp);
        g.Clear(Color.Transparent);
        using var brush = new SolidBrush(Color.White);
        g.FillEllipse(brush, 4, 10, 16, 18);
        g.FillPolygon(brush, new System.Drawing.Point[] {
            new(18, 8), new(30, 2), new(30, 30), new(18, 24) });
        return Icon.FromHandle(bmp.GetHicon());
    }

    DateTime GetConfigMtime()
    {
        try { return File.GetLastWriteTime(_configPath); }
        catch { return DateTime.MinValue; }
    }

    void StartMessageLoop()
    {
        _msgThread = new Thread(() =>
        {
            MSG msg = new();
            while (_running && GetMessage(ref msg, IntPtr.Zero, 0, 0))
            {
                TranslateMessage(ref msg);
                DispatchMessage(ref msg);
            }
        }) { IsBackground = true };
        _msgThread.Start();
    }

    void StartConfigWatcher()
    {
        new Thread(() =>
        {
            while (_running)
            {
                Thread.Sleep(2000);
                try
                {
                    var mtime = File.GetLastWriteTime(_configPath);
                    if (mtime != _lastConfigMtime)
                    {
                        _lastConfigMtime = mtime;
                        Log("Config changed, reloading...");
                        ReloadConfig();
                    }
                }
                catch { }
            }
        }) { IsBackground = true }.Start();
    }

    void ReloadConfig()
    {
        _hotkey.Dispose();
        _hotkey = new HotkeyManager();
        _hotkey.SetHwnd(_hwnd);
        _audio.Reset();
        _config = Config.Load(_configPath);
        RegisterHotkeys();
    }

    protected override void Dispose(bool disposing)
    {
        _running = false;
        _hotkey.Dispose();
        _tray.Visible = false;
        _tray.Dispose();
        if (_hwnd != IntPtr.Zero)
            DestroyWindow(_hwnd);
        base.Dispose(disposing);
    }
}
