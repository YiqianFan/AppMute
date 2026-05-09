#include "hotkey.h"
#include <windows.h>

static ATOM g_hotkeyAtom = 0;

bool Hotkey_Register(HWND hwnd, UINT modKey, UINT vkCode) {
    if (g_hotkeyAtom != 0) {
        Hotkey_Unregister(hwnd);
    }
    g_hotkeyAtom = GlobalAddAtomW(L"MuteAppHotkey");
    return RegisterHotKey(hwnd, g_hotkeyAtom, modKey, vkCode) != 0;
}

void Hotkey_Unregister(HWND hwnd) {
    if (g_hotkeyAtom != 0) {
        UnregisterHotKey(hwnd, g_hotkeyAtom);
        GlobalDeleteAtom(g_hotkeyAtom);
        g_hotkeyAtom = 0;
    }
}