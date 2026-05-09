#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <Audioclient.h>

struct AudioCache {
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice*           pDevice = nullptr;
    IAudioSessionManager2* pSessionMgr = nullptr;
    DWORD  targetPid = 0;
    wchar_t targetExe[260] = {};
};

bool Audio_Init(AudioCache* cache);
bool Audio_ToggleMute(AudioCache* cache, const wchar_t* targetProcess);
void Audio_Shutdown(AudioCache* cache);

static bool MatchProcessName(const wchar_t* fullPath, const wchar_t* targetName);