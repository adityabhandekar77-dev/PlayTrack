#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>

#include <filesystem>
#include <string>

bool registerTrackerStartup() {
    wchar_t modulePath[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return false;

    std::filesystem::path trackerPath =
        std::filesystem::path(modulePath).parent_path() / L"PlayTrackTracker.exe";
    if (!std::filesystem::exists(trackerPath)) return false;

    PWSTR startupFolderRaw = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Startup, KF_FLAG_CREATE, nullptr, &startupFolderRaw);
    if (FAILED(hr) || startupFolderRaw == nullptr) return false;

    std::filesystem::path shortcutPath = std::filesystem::path(startupFolderRaw) / L"PlayTrack Tracker.lnk";
    CoTaskMemFree(startupFolderRaw);

    const HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) return false;

    IShellLinkW* shellLink = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IShellLinkW, reinterpret_cast<void**>(&shellLink));
    if (FAILED(hr) || shellLink == nullptr) {
        if (shouldUninitialize) CoUninitialize();
        return false;
    }

    shellLink->SetPath(trackerPath.c_str());
    shellLink->SetWorkingDirectory(trackerPath.parent_path().c_str());
    shellLink->SetDescription(L"Starts PlayTrack's background game-time tracker when you sign in.");

    IPersistFile* persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persistFile));
    if (SUCCEEDED(hr) && persistFile != nullptr) {
        hr = persistFile->Save(shortcutPath.c_str(), TRUE);
        persistFile->Release();
    }

    shellLink->Release();
    if (shouldUninitialize) CoUninitialize();
    return SUCCEEDED(hr);
}
