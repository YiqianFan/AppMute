#pragma once
#include <windows.h>

struct AppConfig {
    wchar_t targetProcess[260];
    UINT    modKey;
    UINT    vkCode;
    bool    showNotification;
};

void Config_Load(const wchar_t* path, AppConfig* cfg);
void Config_Save(const wchar_t* path, const AppConfig* cfg);