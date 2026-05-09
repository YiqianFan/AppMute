#include "dialog.h"
#include <windows.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace {
    HWND g_dialogHwnd = nullptr;
    HWND g_hEditProcess = nullptr;
    HWND g_hHotkey = nullptr;
    INT  g_dlgResult = IDCANCEL;

    enum {
        IDC_EDIT_PROCESS  = 101,
        IDC_HOTKEY        = 102,
        IDC_BTN_OK        = 103,
        IDC_BTN_CANCEL    = 104,
        IDC_LABEL_PROCESS = 201,
        IDC_LABEL_HOTKEY  = 202,
    };

    UINT HotkeyCtrlToMod(UINT hkFlags) {
        UINT mod = 0;
        if (hkFlags & HOTKEYF_SHIFT) mod |= MOD_SHIFT;
        if (hkFlags & HOTKEYF_CTRL)  mod |= MOD_CONTROL;
        if (hkFlags & HOTKEYF_ALT)    mod |= MOD_ALT;
        return mod;
    }

    UINT ModToHotkeyCtrl(UINT mod) {
        UINT flags = 0;
        if (mod & MOD_SHIFT)   flags |= HOTKEYF_SHIFT;
        if (mod & MOD_CONTROL) flags |= HOTKEYF_CTRL;
        if (mod & MOD_ALT)     flags |= HOTKEYF_ALT;
        return flags;
    }

    void CenterWindow(HWND hwnd, HWND hwndOwner) {
        RECT rc, rcOwner;
        GetWindowRect(hwnd, &rc);
        GetWindowRect(hwndOwner, &rcOwner);
        int x = rcOwner.left + (rcOwner.right - rcOwner.left - (rc.right - rc.left)) / 2;
        int y = rcOwner.top + (rcOwner.bottom - rcOwner.top - (rc.bottom - rc.top)) / 2;
        SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    LRESULT CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        (void)lp;
        switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wp) == IDC_BTN_OK) {
                wchar_t buf[260];
                GetWindowTextW(g_hEditProcess, buf, 260);
                if (wcslen(buf) == 0) {
                    MessageBoxW(hwnd, L"请输入目标进程名", L"提示", MB_OK);
                    return 0;
                }
                g_dlgResult = IDOK;
                DestroyWindow(hwnd);
            } else if (LOWORD(wp) == IDC_BTN_CANCEL) {
                g_dlgResult = IDCANCEL;
                DestroyWindow(hwnd);
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        return 0;
    }
}

bool dialog::ShowSettings(HWND hwndOwner, AppConfig* config) {
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_HOTKEY_CLASS };
    InitCommonControlsEx(&icex);

    g_dlgResult = IDCANCEL;
    g_hEditProcess = nullptr;
    g_hHotkey = nullptr;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);

    g_dialogHwnd = CreateWindowExW(
        0, L"#32770", L"设置",
        WS_POPUP | WS_BORDER | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 200,
        hwndOwner, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    if (!g_dialogHwnd) return false;

    SetWindowLongPtrW(g_dialogHwnd, GWLP_WNDPROC, (LONG_PTR)&DlgProc);

    HFONT hFont = CreateFontW(-12, 0, 0, 0, 400, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"MS Sans Serif");

    auto MakeStatic = [&](int id, const wchar_t* text, int x, int y, int w, int h) {
        HWND hw = CreateWindowW(L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, w, h, g_dialogHwnd, (HMENU)id, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(hw, WM_SETFONT, (WPARAM)hFont, TRUE);
        return hw;
    };

    MakeStatic(IDC_LABEL_PROCESS, L"目标进程:", 20, 20, 100, 20);
    g_hEditProcess = CreateWindowExW(0, L"EDIT", config->targetProcess,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        20, 42, 280, 24, g_dialogHwnd, (HMENU)IDC_EDIT_PROCESS,
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(g_hEditProcess, WM_SETFONT, (WPARAM)hFont, TRUE);

    MakeStatic(IDC_LABEL_HOTKEY, L"静音热键:", 20, 80, 100, 20);
    g_hHotkey = CreateWindowExW(0, HOTKEY_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        20, 102, 280, 24, g_dialogHwnd, (HMENU)IDC_HOTKEY,
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(g_hHotkey, HKM_SETHOTKEY,
        MAKELONG(config->vkCode, ModToHotkeyCtrl(config->modKey)), 0);

    HWND hBtnOk = CreateWindowW(L"BUTTON", L"确定",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        130, 150, 80, 28, g_dialogHwnd, (HMENU)IDC_BTN_OK,
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(hBtnOk, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hBtnCancel = CreateWindowW(L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        220, 150, 80, 28, g_dialogHwnd, (HMENU)IDC_BTN_CANCEL,
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

    EnableWindow(hwndOwner, FALSE);
    CenterWindow(g_dialogHwnd, hwndOwner);
    ShowWindow(g_dialogHwnd, SW_SHOW);
    SetFocus(g_hEditProcess);

    bool result = false;
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            break;
        }
        if (!IsDialogMessageW(g_dialogHwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!IsWindow(g_dialogHwnd)) break;
    }

    if (g_dlgResult == IDOK) {
        wchar_t procBuf[260];
        GetWindowTextW(g_hEditProcess, procBuf, 260);
        wcscpy_s(config->targetProcess, procBuf);

        DWORD hk = (DWORD)SendMessageW(g_hHotkey, HKM_GETHOTKEY, 0, 0);
        config->vkCode = LOBYTE(hk);
        config->modKey = HotkeyCtrlToMod(HIBYTE(hk));
        result = true;
    }

    EnableWindow(hwndOwner, TRUE);
    SetForegroundWindow(hwndOwner);

    if (IsWindow(g_dialogHwnd)) DestroyWindow(g_dialogHwnd);
    DeleteObject(hFont);

    return result;
}