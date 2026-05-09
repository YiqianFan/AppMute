#pragma once
#include <windows.h>
#include "config.h"

namespace dialog {
    bool ShowSettings(HWND hwndOwner, AppConfig* config);
}