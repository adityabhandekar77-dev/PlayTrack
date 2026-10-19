#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include "folder_picker.h"

#include <objbase.h>

std::filesystem::path chooseFolderInExplorer(void* ownerWindow) {
    std::filesystem::path selectedPath;
    HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(initResult);

    if (SUCCEEDED(initResult) || initResult == RPC_E_CHANGED_MODE) {
        IFileOpenDialog* dialog = nullptr;
        HRESULT createResult = CoCreateInstance(
            CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog));

        if (SUCCEEDED(createResult) && dialog) {
            FILEOPENDIALOGOPTIONS options{};
            if (SUCCEEDED(dialog->GetOptions(&options)) &&
                SUCCEEDED(dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST)) &&
                SUCCEEDED(dialog->SetTitle(L"Select a folder for PlayTrack to scan")) &&
                SUCCEEDED(dialog->Show(static_cast<HWND>(ownerWindow)))) {
                IShellItem* item = nullptr;
                if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                        selectedPath = path;
                        CoTaskMemFree(path);
                    }
                    item->Release();
                }
            }
            dialog->Release();
        }
    }

    if (shouldUninitialize) CoUninitialize();
    return selectedPath;
}
