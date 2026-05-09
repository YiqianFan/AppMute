#include "tray.h"
#include <windows.h>
#include <shellapi.h>

static NOTIFYICONDATAW g_nid;
static HWND g_trayHwnd = nullptr;

#define IDM_SETTINGS  1001
#define IDM_EXIT      1002

void Tray_Init(HWND hwnd) {
    g_trayHwnd = hwnd;

    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    wcscpy_s(g_nid.szTip, L"AppMute - 右键设置");
    g_nid.uCallbackMessage = WM_USER + 1;

    LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
    g_nid.hIcon = (HICON)LoadImageW(
        GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
        IMAGE_ICON, 16, 16, LR_SHARED
    );
    if (!g_nid.hIcon) {
        g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }

    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void Tray_ShowBalloon(HWND, const wchar_t* title, const wchar_t* msg) {
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_INFO;
    wcscpy_s(g_nid.szInfoTitle, title);
    wcscpy_s(g_nid.szInfo, msg);
    g_nid.uTimeout = 3000;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void Tray_Destroy() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

void Tray_ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"设置...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY,
                             pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
    case IDM_SETTINGS:
        onSettingsClicked(hwnd);
        break;
    case IDM_EXIT:
        PostMessageW(hwnd, WM_DESTROY, 0, 0);
        break;
    }
}