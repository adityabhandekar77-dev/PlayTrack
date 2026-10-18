
#include "app_discovery.h"
#include <cwctype>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>
#include <objbase.h>

#include <algorithm>
#include <system_error>
#include <string>
#include <utility>

static bool hasExtension(
    const std::filesystem::path& path,
    const wchar_t* extension
) {
    return _wcsicmp(path.extension().c_str(), extension) == 0;
}


static bool isHelperExecutable(const std::filesystem::path& path) {
    std::wstring filename = path.stem().wstring();

    std::transform(
        filename.begin(),
        filename.end(),
        filename.begin(),
        [](wchar_t ch) {
            return static_cast<wchar_t>(towlower(ch));
        }
    );

    const std::vector<std::wstring> excludedNames = {
        L"unins",
        L"uninstall",
        L"setup",
        L"installer",
        L"dxwebsetup",
        L"quicksfv",
        L"vcredist",
        L"unitycrashhandler",
        L"crashpad",
        L"easyanticheat_eos_setup",
        L"bb-gpu-capabilities",
        L"bb-probe",
        L"start_protected_game"
    };

    for (const auto& excluded : excludedNames) {
        if (filename.find(excluded) == 0) {
            return true;
        }
    }

    return false;
}



static std::wstring normalizePath(const std::filesystem::path& path) {
    std::error_code error;
    auto result = std::filesystem::weakly_canonical(path, error);

    if (error) {
        error.clear();
        result = std::filesystem::absolute(path, error);
        if (error) result = path;
    }

    return result.lexically_normal().wstring();
}

static bool samePath(
    const std::filesystem::path& first,
    const std::filesystem::path& second
) {
    return _wcsicmp(
        normalizePath(first).c_str(),
        normalizePath(second).c_str()
    ) == 0;
}

static bool resolveShortcut(
    const std::filesystem::path& shortcut,
    std::filesystem::path& target
) {
    HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return false;
    }

    bool uninitialize = SUCCEEDED(initResult);
    IShellLinkW* shellLink = nullptr;
    IPersistFile* persistFile = nullptr;
    bool resolved = false;

    HRESULT result = CoCreateInstance(
        CLSID_ShellLink,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IShellLinkW,
        reinterpret_cast<void**>(&shellLink)
    );

    if (SUCCEEDED(result)) {
        result = shellLink->QueryInterface(
            IID_IPersistFile,
            reinterpret_cast<void**>(&persistFile)
        );
    }

    if (SUCCEEDED(result)) {
        result = persistFile->Load(shortcut.c_str(), STGM_READ);
    }

    if (SUCCEEDED(result)) {
        wchar_t targetBuffer[32768]{};
        WIN32_FIND_DATAW findData{};

        result = shellLink->GetPath(
            targetBuffer,
            static_cast<int>(std::size(targetBuffer)),
            &findData,
            SLGP_RAWPATH
        );

        if (SUCCEEDED(result) && targetBuffer[0] != L'\0') {
            target = targetBuffer;

            if (target.is_relative()) {
                std::error_code error;
                target = std::filesystem::absolute(
                    shortcut.parent_path() / target, error
                );
                if (error) target.clear();
            }

            std::error_code error;
            resolved = !target.empty() &&
                       std::filesystem::is_regular_file(target, error) &&
                       !error &&
                       hasExtension(target, L".exe");
        }
    }

    if (persistFile) persistFile->Release();
    if (shellLink) shellLink->Release();
    if (uninitialize) CoUninitialize();

    return resolved;
}

std::vector<Application> discoverApplications(
    const std::vector<std::filesystem::path>& folders
) {
    std::vector<Application> discovered;

    for (const auto& folder : folders) {
        std::error_code error;

        if (!std::filesystem::is_directory(folder, error)) {
            continue;
        }

        std::filesystem::recursive_directory_iterator iterator(
            folder,
            std::filesystem::directory_options::skip_permission_denied,
            error
        );

        std::filesystem::recursive_directory_iterator end;

        while (iterator != end) {
            if (error) {
                error.clear();
                iterator.increment(error);
                continue;
            }

            const auto& entry = *iterator;
            std::error_code entryError;

            if (entry.is_symlink(entryError) &&
                entry.is_directory(entryError)) {
                iterator.disable_recursion_pending();
            } else if (entry.is_regular_file(entryError)) {
                auto source = entry.path();
                std::filesystem::path executable;

                if (hasExtension(source, L".exe")) {
                    executable = source;
                } else if (hasExtension(source, L".lnk")) {
                    if (!resolveShortcut(source, executable)) {
                        iterator.increment(error);
                        continue;
                    }
                }

                 if (!executable.empty() && !isHelperExecutable(executable)) {
    executable = normalizePath(executable);

                    bool duplicate = false;

                    for (const auto& app : discovered) {
                        if (samePath(app.executable, executable)) {
                            duplicate = true;
                            break;
                        }
                    }

                    if (!duplicate) {
                        Application app;
                        app.name = source.stem().string();
                        app.executable = executable.wstring();
                        discovered.push_back(std::move(app));
                    }
                }
            }

            iterator.increment(error);
        }
    }

    return discovered;
}

void mergeDiscoveredApplications(
    std::vector<Application>& existing,
    const std::vector<Application>& discovered
) {
    for (const auto& candidate : discovered) {
        bool duplicate = false;

        for (const auto& app : existing) {
            if (samePath(app.executable, candidate.executable)) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            existing.push_back(candidate);
        }
    }
}
