#include "config.h"
#include <windows.h>
#include <stdio.h>

static int modStringToFlags(const wchar_t* str) {
    int flags = 0;
    if (wcsstr(str, L"Ctrl")) flags |= 0x02; // MOD_CONTROL = 0x02
    if (wcsstr(str, L"Alt"))   flags |= 0x01; // MOD_ALT = 0x01
    if (wcsstr(str, L"Shift")) flags |= 0x04; // MOD_SHIFT = 0x04
    return flags;
}

void Config_Load(const wchar_t* path, AppConfig* cfg) {
    wchar_t buf[512];

    GetPrivateProfileStringW(L"mute", L"target", L"chrome.exe",
        cfg->targetProcess, 260, path);

    GetPrivateProfileStringW(L"mute", L"mod", L"Ctrl+Alt",
        buf, 512, path);
    cfg->modKey = modStringToFlags(buf);

    GetPrivateProfileStringW(L"mute", L"key", L"M",
        buf, 512, path);
    cfg->vkCode = (UINT)buf[0];

    cfg->showNotification = (GetPrivateProfileIntW(L"mute", L"notify", 1, path) != 0);
}

void Config_Save(const wchar_t* path, const AppConfig* cfg) {
    wchar_t modStr[64] = L"";
    if (cfg->modKey & 0x02) wcscat_s(modStr, L"Ctrl+");
    if (cfg->modKey & 0x01) wcscat_s(modStr, L"Alt+");
    if (cfg->modKey & 0x04) wcscat_s(modStr, L"Shift+");

    wchar_t keyStr[2] = { (wchar_t)cfg->vkCode, L'\0' };

    WritePrivateProfileStringW(L"mute", L"target", cfg->targetProcess, path);
    WritePrivateProfileStringW(L"mute", L"mod", modStr, path);
    WritePrivateProfileStringW(L"mute", L"key", keyStr, path);
    WritePrivateProfileStringW(L"mute", L"notify",
        cfg->showNotification ? L"1" : L"0", path);
}