#pragma once
#include <windows.h>
#include <shellapi.h>

#define WM_TRAY_NOTIFY (WM_USER + 1)
#define IDM_SETTINGS   1001
#define IDM_EXIT       1002

extern HINSTANCE g_hInst;
void onSettingsClicked(HWND hwnd);
void Tray_Init(HWND hwnd);
void Tray_ShowBalloon(HWND hwnd, const wchar_t* title, const wchar_t* msg);
void Tray_Destroy();
void Tray_ShowContextMenu(HWND hwnd);