using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

public class AudioManager
{
    public static Action<string>? Log { get; set; }

    [DllImport("ole32.dll")]
    static extern int CoCreateInstance(ref Guid rclsid, IntPtr pUnkOuter, uint dwClsContext, ref Guid riid, out IntPtr ppv);

    [ComImport]
    [Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    class MMDeviceEnumerator { }

    [ComImport]
    [Guid("D666063F-1587-426E-885D-C478F64BE9F1")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMMDevice
    {
        int Activate(ref Guid iid, uint dwClsCtx, IntPtr pActivationParams, [MarshalAs(UnmanagedType.IUnknown)] out object ppInterface);
    }

    [ComImport]
    [Guid("A95632D2-9614-4F35-A746-DE8DB63617E6")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioSessionManager2
    {
        int GetSessionEnumerator(out IAudioSessionEnumerator SessionEnum);
    }

    [ComImport]
    [Guid("F4B1A599-7266-4319-A8CA-E70ACB11E8CD")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioSessionControl
    {
        int GetProcessId(out int pRetVal);
    }

    [ComImport]
    [Guid("77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface ISimpleAudioVolume
    {
        int SetMute([MarshalAs(UnmanagedType.Bool)] bool mute, ref Guid eventContext);
    }

    [ComImport]
    [Guid("E2F5BB11-0570-40CA-ACDD-3AA01277DEE8")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioSessionEnumerator
    {
        int GetCount(out int pRetVal);
        int GetSession(int SessionIndex, out IAudioSessionControl Session);
    }

    HashSet<string> _muted = new(StringComparer.OrdinalIgnoreCase);

    [DllImport("ole32.dll")]
    static extern int CoInitializeEx(IntPtr pvReserved, uint dwCoInit);
    const uint COINIT_APARTMENTTHREADED = 0x2;

    public AudioManager()
    {
        CoInitializeEx(IntPtr.Zero, COINIT_APARTMENTTHREADED);
    }

    public void SetProcessMute(string processName, bool mute)
    {
        if (mute) _muted.Add(processName);
        else _muted.Remove(processName);

        try
        {
            Guid clsidMMDevice = new Guid("BCDE0395-E52F-467C-8E3D-C4579291692E");
            Guid iidIMMDevice = new Guid("D666063F-1587-426E-885D-C478F64BE9F1");

            IntPtr pDevice = IntPtr.Zero;
            int hr = CoCreateInstance(ref clsidMMDevice, IntPtr.Zero, 1, ref iidIMMDevice, out pDevice);
            Log?.Invoke($"[Audio] CoCreateInstance hr={hr:X}, pDevice={pDevice:X}");
            if (hr < 0 || pDevice == IntPtr.Zero)
            {
                return;
            }

            var device = (IMMDevice)Marshal.GetObjectForIUnknown(pDevice);
            Log?.Invoke("[Audio] MMDevice created");

            Guid iidSession = new Guid("A95632D2-9614-4F35-A746-DE8DB63617E6");
            object o;
            int hr2 = device.Activate(ref iidSession, 0x23, IntPtr.Zero, out o);
            Log?.Invoke($"[Audio] Activate hr={hr2:X}, o={o}");
            if (hr2 < 0 || o == null) return;

            var mgr = (IAudioSessionManager2)o;
            mgr.GetSessionEnumerator(out var sessionEnum);

            sessionEnum.GetCount(out int count);
            bool found = false;

            for (int i = 0; i < count; i++)
            {
                sessionEnum.GetSession(i, out var session);
                session.GetProcessId(out int pid);
                if (pid == 0) continue;

                try
                {
                    var proc = System.Diagnostics.Process.GetProcessById(pid);
                    var pname = proc.ProcessName.ToLower();
                    if (pname.EndsWith(".exe")) pname = pname[..^4];

                    Log?.Invoke($"[Audio] Checking session: pid={pid}, name={pname}, target={processName}");

                    if (pname.Equals(processName, StringComparison.OrdinalIgnoreCase))
                    {
                        var sv = (ISimpleAudioVolume)session;
                        var guid = Guid.Empty;
                        sv.SetMute(mute, ref guid);
                        found = true;
                        Log?.Invoke($"[Audio] Muted '{proc.ProcessName}' = {mute}");
                        break;
                    }
                }
                catch { }
            }

            if (!found)
                Log?.Invoke($"[Audio] No session for '{processName}'");
        }
        catch (Exception e)
        {
            Log?.Invoke($"[Audio] Error: {e.Message}");
        }
    }

    public bool ToggleProcessMute(string processName)
    {
        bool current = _muted.Contains(processName);
        SetProcessMute(processName, !current);
        return !current;
    }

    public bool IsProcessMuted(string processName) => _muted.Contains(processName);

    public void Reset() => _muted.Clear();
}
