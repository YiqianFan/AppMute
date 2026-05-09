#pragma once
#include <windows.h>

bool Hotkey_Register(HWND hwnd, UINT modKey, UINT vkCode);
void Hotkey_Unregister(HWND hwnd);