#include "audio.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <Audioclient.h>
#include <shobjidl.h>
#include <Psapi.h>
#include <cstdio>

static bool MatchProcessName(const wchar_t* fullPath, const wchar_t* targetName) {
    const wchar_t* sep = wcsrchr(fullPath, L'\\');
    const wchar_t* name = sep ? sep + 1 : fullPath;
    return _wcsicmp(name, targetName) == 0;
}

static int ToggleMuteByPid(IAudioSessionManager2* mgr, DWORD targetPid) {
    IAudioSessionEnumerator* pEnum = nullptr;
    HRESULT hr = mgr->GetSessionEnumerator(&pEnum);
    if (FAILED(hr)) return -1;

    int count = 0;
    pEnum->GetCount(&count);
    int result = -1;

    for (int i = 0; i < count; i++) {
        IAudioSessionControl* pSession = nullptr;
        hr = pEnum->GetSession(i, &pSession);
        if (FAILED(hr) || !pSession) continue;

        IAudioSessionControl2* pSession2 = nullptr;
        hr = pSession->QueryInterface(IID_PPV_ARGS(&pSession2));
        pSession->Release();
        if (FAILED(hr) || !pSession2) continue;

        DWORD pid = pSession2->GetProcessId();
        if (pid == targetPid) {
            IAudioEndpointVolume* pVol = nullptr;
            hr = pSession2->QueryInterface(IID_PPV_ARGS(&pVol));
            if (SUCCEEDED(hr) && pVol) {
                BOOL mute = FALSE;
                pVol->GetMute(&mute);
                pVol->SetMute(mute ? FALSE : TRUE, nullptr);
                pVol->Release();
                result = 0;
                // no break:同一进程可能有多个会话,静掉所有
            }
        }
        pSession2->Release();
    }
    pEnum->Release();
    return result;
}

static int ToggleMuteByEnumeration(IAudioSessionManager2* mgr, const wchar_t* targetName, DWORD* outPid) {
    IAudioSessionEnumerator* pEnum = nullptr;
    HRESULT hr = mgr->GetSessionEnumerator(&pEnum);
    if (FAILED(hr)) return -1;

    int count = 0;
    pEnum->GetCount(&count);
    int result = -1;

    for (int i = 0; i < count; i++) {
        IAudioSessionControl* pSession = nullptr;
        hr = pEnum->GetSession(i, &pSession);
        if (FAILED(hr) || !pSession) continue;

        IAudioSessionControl2* pSession2 = nullptr;
        hr = pSession->QueryInterface(IID_PPV_ARGS(&pSession2));
        pSession->Release();
        if (FAILED(hr) || !pSession2) continue;

        DWORD pid = pSession2->GetProcessId();
        if (pid == 0) { pSession2->Release(); continue; }

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProc) { pSession2->Release(); continue; }

        wchar_t path[260];
        DWORD len = 260;
        BOOL ok = QueryFullProcessImageNameW(hProc, 0, path, &len);
        CloseHandle(hProc);

        if (ok && MatchProcessName(path, targetName)) {
            IAudioEndpointVolume* pVol = nullptr;
            hr = pSession2->QueryInterface(IID_PPV_ARGS(&pVol));
            if (SUCCEEDED(hr) && pVol) {
                BOOL mute = FALSE;
                pVol->GetMute(&mute);
                pVol->SetMute(mute ? FALSE : TRUE, nullptr);
                pVol->Release();
                result = 0;
                if (outPid) *outPid = pid;
            }
        }
        pSession2->Release();
    }
    pEnum->Release();
    return result;
}

bool Audio_Init(AudioCache* cache) {
    if (!cache) return false;

    HRESULT hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
        IID_IMMDeviceEnumerator, (void**)&cache->pEnumerator
    );
    if (FAILED(hr)) return false;

    hr = cache->pEnumerator->GetDefaultAudioEndpoint(
        eRender, eConsole, &cache->pDevice
    );
    if (FAILED(hr)) return false;

    hr = cache->pDevice->Activate(
        IID_IAudioSessionManager2, CLSCTX_ALL, nullptr, (void**)&cache->pSessionMgr
    );
    return SUCCEEDED(hr);
}

bool Audio_ToggleMute(AudioCache* cache, const wchar_t* targetProcess) {
    if (!cache || !cache->pSessionMgr || !targetProcess) return false;

    // Fast path: verify cached PID
    if (cache->targetPid != 0) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, cache->targetPid);
        if (hProc) {
            wchar_t path[260];
            DWORD len = 260;
            if (QueryFullProcessImageNameW(hProc, 0, path, &len) &&
                MatchProcessName(path, targetProcess)) {
                CloseHandle(hProc);
                // PID valid and matches → fast path
                int r = ToggleMuteByPid(cache->pSessionMgr, cache->targetPid);
                return r == 0;
            }
            CloseHandle(hProc);
        }
        // PID stale, clear it
        cache->targetPid = 0;
    }

    // Slow path: enumerate sessions, find target, cache PID
    DWORD newPid = 0;
    int r = ToggleMuteByEnumeration(cache->pSessionMgr, targetProcess, &newPid);
    if (r == 0 && newPid != 0) {
        cache->targetPid = newPid;
    }
    return r == 0;
}

void Audio_Shutdown(AudioCache* cache) {
    if (!cache) return;
    if (cache->pSessionMgr) { cache->pSessionMgr->Release(); cache->pSessionMgr = nullptr; }
    if (cache->pDevice) { cache->pDevice->Release(); cache->pDevice = nullptr; }
    if (cache->pEnumerator) { cache->pEnumerator->Release(); cache->pEnumerator = nullptr; }
    cache->targetPid = 0;
}