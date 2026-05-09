#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============= LOG =============
void Log(const char *fmt, ...) {
    FILE *f = fopen("app-mute.log", "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);
        fprintf(f, "\n");
        fclose(f);
    }
}

// ============= CONFIG =============
#define MAX_HOTKEYS 32
char g_config_path[MAX_PATH];
int g_hotkey_count = 0;
struct { int id; char hotkey[64]; char app[64]; } g_hotkeys[MAX_HOTKEYS];

void Trim(char *s) {
    while (*s && (s[strlen(s)-1] == ' ' || s[strlen(s)-1] == '\t' || s[strlen(s)-1] == '\n' || s[strlen(s)-1] == '\r')) s[strlen(s)-1] = 0;
    while (*s && (*s == ' ' || *s == '\t')) s++;
}

int LoadConfig(void) {
    FILE *f = fopen(g_config_path, "r");
    if (!f) {
        Log("Config: cannot open %s", g_config_path);
        return 0;
    }

    g_hotkey_count = 0;
    char line[256];
    int in_hotkeys = 0;

    while (fgets(line, sizeof(line), f) && g_hotkey_count < MAX_HOTKEYS) {
        Trim(line);
        if (line[0] == '#' || line[0] == 0) continue;
        if (strncmp(line, "[hotkeys]", 9) == 0) { in_hotkeys = 1; continue; }
        if (in_hotkeys && line[0] == '[') break;

        if (in_hotkeys) {
            char *eq = strchr(line, '=');
            if (!eq) continue;

            char *key = line;
            while (*key == ' ' || *key == '"') key++;
            char *key_end = key;
            while (*key_end && *key_end != '"') key_end++;
            *key_end = 0;

            char *val = eq + 1;
            while (*val == ' ' || *val == '"') val++;
            char *val_end = val;
            while (*val_end && *val_end != '"') val_end++;
            *val_end = 0;

            if (*key && *val) {
                strncpy(g_hotkeys[g_hotkey_count].hotkey, key, 63);
                strncpy(g_hotkeys[g_hotkey_count].app, val, 63);
                Log("Config: %s -> %s", key, val);
                g_hotkey_count++;
            }
        }
    }
    fclose(f);
    return g_hotkey_count;
}

// ============= HOTKEY PARSING =============
UINT ParseModifier(const char *s) {
    if (strcmp(s, "ctrl") == 0 || strcmp(s, "control") == 0) return MOD_CONTROL;
    if (strcmp(s, "alt") == 0) return MOD_ALT;
    if (strcmp(s, "shift") == 0) return MOD_SHIFT;
    if (strcmp(s, "win") == 0) return MOD_WIN;
    return 0;
}

UINT ParseVK(const char *s) {
    if (strcmp(s, "f1") == 0) return VK_F1;
    if (strcmp(s, "f2") == 0) return VK_F2;
    if (strcmp(s, "f3") == 0) return VK_F3;
    if (strcmp(s, "f4") == 0) return VK_F4;
    if (strcmp(s, "f5") == 0) return VK_F5;
    if (strcmp(s, "f6") == 0) return VK_F6;
    if (strcmp(s, "f7") == 0) return VK_F7;
    if (strcmp(s, "f8") == 0) return VK_F8;
    if (strcmp(s, "f9") == 0) return VK_F9;
    if (strcmp(s, "f10") == 0) return VK_F10;
    if (strcmp(s, "f11") == 0) return VK_F11;
    if (strcmp(s, "f12") == 0) return VK_F12;
    if (strcmp(s, "space") == 0) return VK_SPACE;
    if (strcmp(s, "enter") == 0) return VK_RETURN;
    if (strcmp(s, "tab") == 0) return VK_TAB;
    if (strcmp(s, "esc") == 0) return VK_ESCAPE;
    if (strlen(s) == 1 && isalpha((unsigned char)s[0])) return VkKeyScanA(s[0]) & 0xff;
    if (strlen(s) == 1 && isdigit((unsigned char)s[0])) return s[0];
    return 0;
}

void ParseHotkeyString(const char *hotkey_str, UINT *pMod, UINT *pVK) {
    UINT mod = 0, vk = 0;
    char tmp[128];
    strncpy(tmp, hotkey_str, 127);
    tmp[127] = 0;
    for (char *p = tmp; *p; p++) *p = tolower(*p);

    char *dup = strdup(tmp);
    char *token = strtok(dup, "+");
    while (token) {
        UINT m = ParseModifier(token);
        if (m) mod |= m;
        else {
            vk = ParseVK(token);
        }
        token = strtok(NULL, "+");
    }
    free(dup);
    *pMod = mod;
    *pVK = vk;
}

// ============= CORE AUDIO =============
// Function pointers
static HRESULT (WINAPI *pCoInitializeEx)(LPVOID, DWORD) = NULL;
static HMODULE hOle32 = NULL;
static HMODULE hCoreAudio = NULL;

static BOOL InitCom(void) {
    if (!hOle32) {
        hOle32 = LoadLibraryA("ole32.dll");
        if (hOle32) {
            pCoInitializeEx = (void*)GetProcAddress(hOle32, "CoInitializeEx");
        }
    }
    if (pCoInitializeEx) pCoInitializeEx(NULL, 0x2);
    return hOle32 != NULL;
}

// COM interface - just use void* for IUnknown
typedef HRESULT (WINAPI *fnQueryInterface)(void*, const IID*, void**);
typedef ULONG (WINAPI *fnAddRef)(void*);
typedef ULONG (WINAPI *fnRelease)(void*);

static void** GetVtbl(void *obj) { return *(void***)obj; }

// MMDeviceEnumerator CLSID {A95632D2-9614-4F35-A746-DE8DB63617E6}
static CLSID CLSID_MMDeviceEnumerator = {0xa95632d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}};

// IAudioSessionManager2 {77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F}
static IID IID_IAudioSessionManager2 = {0x77aa99a0, 0x1bd6, 0x484f, {0x8b, 0xc7, 0x2c, 0x65, 0xc9, 0xa9, 0xb6, 0xf6}};
static IID IID_ISimpleAudioVolume = {0x77aa99a0, 0x1bd6, 0x484f, {0x8b, 0xc7, 0x2c, 0x65, 0xc9, 0xa9, 0xb6, 0xf6}};
static IID IID_IAudioSessionEnumerator = {0xe2f5bb11, 0x0570, 0x40ca, {0xac, 0xdd, 0x3a, 0xa0, 0x12, 0x77, 0xde, 0xe8}};

// DllGetClassObject from audioclient.dll
static HRESULT (WINAPI *pDllGetClassObject)(REFCLSID rclsid, REFIID riid, void **ppv) = NULL;

static BOOL InitCoreAudio(void) {
    if (!hCoreAudio) {
        hCoreAudio = LoadLibraryA("Audioclient.dll");
        if (hCoreAudio) {
            pDllGetClassObject = (void*)GetProcAddress(hCoreAudio, "DllGetClassObject");
        }
    }
    return hCoreAudio != NULL && pDllGetClassObject != NULL;
}

static void ToggleMuteForApp(const char *app_lower) {
    if (!InitCom() || !InitCoreAudio()) {
        Log("Audio: init failed");
        return;
    }

    // DllGetClassObject(CLSID_MMDeviceEnumerator, IID_IClassFactory, &cf)
    // IID_IClassFactory = {00000001-0000-0000-C0-00-000000000046}
    static IID IID_IClassFactory = {0x00000001, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

    void *cf = NULL;
    HRESULT hr = pDllGetClassObject(&CLSID_MMDeviceEnumerator, &IID_IClassFactory, &cf);
    if (hr < 0 || !cf) {
        Log("Audio: DllGetClassObject failed: %08X", hr);
        return;
    }

    // IClassFactory::CreateInstance(IClassFactory* this, IUnknown* outer, REFIID iid, void** ppv)
    // vtbl[3] = CreateInstance
    typedef HRESULT (WINAPI *fnCreateInstance)(void*, void*, REFIID, void**);
    fnCreateInstance CreateInstance = *(fnCreateInstance*)GetVtbl(cf)[3];

    // Create IMMDeviceEnumerator
    // IID_IMMDeviceEnumerator = {D666063F-1587-426E-885D-C478F64BE9F1}
    static IID IID_IMMDeviceEnumerator = {0xd666063f, 0x1587, 0x426e, {0x88, 0x5d, 0xc4, 0x78, 0xf6, 0x4b, 0xe9, 0xf1}};

    void *pEnum = NULL;
    hr = CreateInstance(cf, NULL, &IID_IMMDeviceEnumerator, &pEnum);
    // Release cf
    ((fnRelease)GetVtbl(cf)[2])(cf);
    cf = NULL;

    if (hr < 0 || !pEnum) {
        Log("Audio: CreateInstance IMMDeviceEnumerator failed: %08X", hr);
        return;
    }

    // IMMDeviceEnumerator::GetDefaultAudioEndpoint(int dataFlow, int role, IMMDevice** ppEndpoint)
    // dataFlow=0(eRender), role=1(eConsole)
    // vtbl[3] = GetDefaultAudioEndpoint
    typedef HRESULT (WINAPI *fnGetDefault)(void*, int, int, void**);
    fnGetDefault GetDefault = (fnGetDefault)GetVtbl(pEnum)[3];

    void *pDevice = NULL;
    hr = GetDefault(pEnum, 0, 1, &pDevice);
    ((fnRelease)GetVtbl(pEnum)[2])(pEnum);
    pEnum = NULL;

    if (hr < 0 || !pDevice) {
        Log("Audio: GetDefaultAudioEndpoint failed: %08X", hr);
        return;
    }

    // IMMDevice::Activate(REFIID riid, DWORD dwClsCtx, PROPVARIANT* pActivationParams, void** ppInterface)
    // vtbl[3] = Activate
    typedef HRESULT (WINAPI *fnActivate)(void*, REFIID, DWORD, void*, void**);
    fnActivate Activate = (fnActivate)GetVtbl(pDevice)[3];

    void *pSessionMgr = NULL;
    hr = Activate(pDevice, &IID_IAudioSessionManager2, 0x23, NULL, &pSessionMgr);
    ((fnRelease)GetVtbl(pDevice)[2])(pDevice);
    pDevice = NULL;

    if (hr < 0 || !pSessionMgr) {
        Log("Audio: Activate IAudioSessionManager2 failed: %08X", hr);
        return;
    }

    // IAudioSessionManager2::GetSessionEnumerator
    // vtbl[3] = GetSessionEnumerator
    typedef HRESULT (WINAPI *fnGetSessionEnum)(void*, void**);
    fnGetSessionEnum GetSessionEnum = (fnGetSessionEnum)GetVtbl(pSessionMgr)[3];

    void *pSessionEnum = NULL;
    hr = GetSessionEnum(pSessionMgr, &pSessionEnum);
    ((fnRelease)GetVtbl(pSessionMgr)[2])(pSessionMgr);
    pSessionMgr = NULL;

    if (hr < 0 || !pSessionEnum) {
        Log("Audio: GetSessionEnumerator failed: %08X", hr);
        return;
    }

    // IAudioSessionEnumerator::GetCount
    // vtbl[3] = GetCount
    typedef int (WINAPI *fnGetCount)(void*);
    fnGetCount GetCount = (fnGetCount)GetVtbl(pSessionEnum)[3];
    int count = GetCount(pSessionEnum);
    Log("Audio: %d sessions for '%s'", count, app_lower);

    int found = 0;

    // IAudioSessionEnumerator::GetSession
    // vtbl[4] = GetSession
    typedef HRESULT (WINAPI *fnGetSession)(void*, int, void**);
    fnGetSession GetSession = (fnGetSession)GetVtbl(pSessionEnum)[4];

    for (int i = 0; i < count; i++) {
        void *pSession = NULL;
        hr = GetSession(pSessionEnum, i, &pSession);
        if (hr < 0 || !pSession) continue;

        // IAudioSessionControl::GetProcessId
        // vtbl[8] = GetProcessId
        typedef DWORD (WINAPI *fnGetProcessId)(void*);
        fnGetProcessId GetProcessId = (fnGetProcessId)GetVtbl(pSession)[8];
        DWORD pid = GetProcessId(pSession);

        ((fnRelease)GetVtbl(pSession)[2])(pSession);
        pSession = NULL;

        if (pid == 0) continue;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProc) continue;

        wchar_t pathW[MAX_PATH];
        DWORD pathLen = MAX_PATH;
        BOOL ok = QueryFullProcessImageNameW(hProc, 0, pathW, &pathLen);
        CloseHandle(hProc);

        if (!ok) continue;

        char path[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, pathW, -1, path, MAX_PATH, NULL, NULL);
        char *bn = strrchr(path, '\\');
        if (!bn) bn = path; else bn++;

        char *dot = strstr(bn, ".exe");
        if (dot) *dot = 0;

        for (char *p = bn; *p; p++) *p = tolower(*p);

        if (strcmp(bn, app_lower) != 0) continue;

        Log("Audio: Found '%s' (pid=%d)", bn, pid);

        // Re-get the session since we released it above
        hr = GetSession(pSessionEnum, i, &pSession);
        if (hr < 0 || !pSession) continue;

        // IAudioSessionControl::QueryInterface(ISimpleAudioVolume)
        fnQueryInterface QueryInterface = (fnQueryInterface)GetVtbl(pSession)[0];
        void *pVol = NULL;
        hr = QueryInterface(pSession, &IID_ISimpleAudioVolume, &pVol);
        ((fnRelease)GetVtbl(pSession)[2])(pSession);
        pSession = NULL;

        if (hr >= 0 && pVol) {
            // ISimpleAudioVolume::GetMute
            // vtbl[4] = GetMute
            typedef BOOL (WINAPI *fnGetMute)(void*, BOOL*);
            // vtbl[3] = SetMute
            typedef HRESULT (WINAPI *fnSetMute)(void*, BOOL, void*);
            fnGetMute GetMute = (fnGetMute)GetVtbl(pVol)[4];
            fnSetMute SetMute = (fnSetMute)GetVtbl(pVol)[3];

            BOOL mute = FALSE;
            GetMute(pVol, &mute);
            SetMute(pVol, !mute, NULL);
            Log("Audio: Mute=%d -> %d", mute, !mute);

            ((fnRelease)GetVtbl(pVol)[2])(pVol);
            pVol = NULL;
        }

        found = 1;
        break;
    }

    if (!found) Log("Audio: No session for '%s'", app_lower);

    ((fnRelease)GetVtbl(pSessionEnum)[2])(pSessionEnum);
    pSessionEnum = NULL;
}

// ============= WINDOW =============
char CLASS_NAME[] = "AppMuteC";

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_HOTKEY) {
        int id = (int)wParam;
        for (int i = 0; i < g_hotkey_count; i++) {
            if (g_hotkeys[i].id == id) {
                Log("Hotkey: %s -> %s", g_hotkeys[i].hotkey, g_hotkeys[i].app);
                char app_lower[64];
                strncpy(app_lower, g_hotkeys[i].app, 63);
                app_lower[63] = 0;
                for (char *pp = app_lower; *pp; pp++) *pp = tolower(*pp);
                ToggleMuteForApp(app_lower);
                break;
            }
        }
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============= MAIN =============
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // open log (truncate)
    FILE *lf = fopen("app-mute.log", "w");
    if (lf) fclose(lf);

    // config path = exe dir + config.toml
    GetModuleFileNameA(NULL, g_config_path, MAX_PATH);
    char *pp = strrchr(g_config_path, '\\');
    if (pp) {
        pp[1] = 0;
        strcat(g_config_path, "config.toml");
    }

    Log("AppMute starting...");

    // load config
    if (!LoadConfig()) {
        MessageBox(NULL, "No hotkeys configured.\nEdit config.toml and restart.", "AppMute", MB_ICONWARNING);
        return 0;
    }

    // register window class
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClassA(&wc)) {
        Log("RegisterClass failed: %d", GetLastError());
        return 1;
    }

    // create hidden window
    HWND hwnd = CreateWindowExA(0, CLASS_NAME, "AppMute", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 200, 100, NULL, NULL, hInstance, NULL);
    if (!hwnd) {
        Log("CreateWindow failed: %d", GetLastError());
        return 1;
    }
    ShowWindow(hwnd, SW_HIDE);

    // register hotkeys
    for (int i = 0; i < g_hotkey_count; i++) {
        UINT mod = 0, vk = 0;
        ParseHotkeyString(g_hotkeys[i].hotkey, &mod, &vk);

        if (!vk) {
            Log("Invalid hotkey: %s", g_hotkeys[i].hotkey);
            continue;
        }

        int id = i + 1;
        g_hotkeys[i].id = id;

        if (RegisterHotKey(hwnd, id, mod | MOD_NOREPEAT, vk))
            Log("Registered [%d]: %s -> %s", id, g_hotkeys[i].hotkey, g_hotkeys[i].app);
        else
            Log("Failed to register: %s (error %d)", g_hotkeys[i].hotkey, GetLastError());
    }

    Log("Entering message loop...");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // cleanup
    for (int i = 0; i < g_hotkey_count; i++) {
        UnregisterHotKey(hwnd, g_hotkeys[i].id);
    }

    Log("AppMute exiting.");
    return 0;
}