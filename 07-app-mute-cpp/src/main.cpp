#include "config.h"
#include "hotkey.h"
#include "audio.h"
#include "tray.h"
#include "dialog.h"
#include <windows.h>
#include <cstdio>

#pragma comment(linker,"\"/manifestdependency:type='win32', \
    name='Microsoft.Windows.Common-Controls', version='6.0.0.0', \
    processorArchitecture='*', publicKeyToken='6595b64144ccf1df', language='*'\"")

HINSTANCE g_hInst = nullptr;
AppConfig g_config = {};
AudioCache g_audioCache = {};
wchar_t g_configPath[512] = L"config.ini";

static HANDLE g_singleInstanceMutex = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_HOTKEY:
        Audio_ToggleMute(&g_audioCache, g_config.targetProcess);
        if (g_config.showNotification) {
            Tray_ShowBalloon(hwnd, L"AppMute", L"已切换静音状态");
        }
        return 0;

    case WM_USER + 1:
        if (LOWORD(lp) == WM_RBUTTONUP) {
            Tray_ShowContextMenu(hwnd);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void onSettingsClicked(HWND hwnd) {
    AppConfig newConfig = g_config;

    if (dialog::ShowSettings(hwnd, &newConfig)) {
        Hotkey_Unregister(hwnd);

        if (!Hotkey_Register(hwnd, newConfig.modKey, newConfig.vkCode)) {
            MessageBoxW(hwnd, L"热键被占用，设置未生效", L"错误", MB_ICONERROR);
            Hotkey_Register(hwnd, g_config.modKey, g_config.vkCode);
            return;
        }

        Config_Save(g_configPath, &newConfig);
        g_config = newConfig;
        // invalidate PID cache when target process changes
        g_audioCache.targetPid = 0;
        Tray_ShowBalloon(hwnd, L"设置已保存", L"新热键和目标进程已生效");
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, INT) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    g_singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"MuteApp_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"AppMute 已在运行", L"提示", MB_OK);
        return 0;
    }

    g_hInst = hInstance;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MuteAppClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"AppMute",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        300, 200, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, SW_HIDE);

    Config_Load(g_configPath, &g_config);
    Audio_Init(&g_audioCache);
    Tray_Init(hwnd);

    if (!Hotkey_Register(hwnd, g_config.modKey, g_config.vkCode)) {
        MessageBoxW(nullptr, L"热键注册失败", L"警告", MB_ICONWARNING);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Hotkey_Unregister(hwnd);
    Tray_Destroy();
    Audio_Shutdown(&g_audioCache);

    CloseHandle(g_singleInstanceMutex);
    CoUninitialize();
    return 0;
}