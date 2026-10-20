#include "tracking_engine.h"
#include "app_discovery.h"
#include "app_config.h"
#include "data_paths.h"

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <vector>

static std::vector<std::filesystem::path> resolveScanFolders(
    const std::filesystem::path& appDirectory,
    const AppConfig& config) {
    std::vector<std::filesystem::path> folders;
    const auto trackedFolder = (appDirectory / L"tracked-apps").lexically_normal();
    folders.push_back(trackedFolder);

    for (const auto& folder : config.scanFolders) {
        auto resolved = folder.is_absolute() ? folder : appDirectory / folder;
        resolved = resolved.lexically_normal();
        const bool duplicate = std::any_of(
            folders.begin(), folders.end(),
            [&](const std::filesystem::path& existing) {
                return _wcsicmp(existing.wstring().c_str(), resolved.wstring().c_str()) == 0;
            });
        if (!duplicate) folders.push_back(resolved);
    }
    return folders;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t modulePath[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return 1;

    std::filesystem::path appDirectory(modulePath);
    appDirectory = appDirectory.parent_path();
    if (appDirectory.filename() == L"build") {
        appDirectory = appDirectory.parent_path();
    }

    if (!initializeDataDirectory(appDirectory)) return 4;

    const auto dataDirectory = getDataDirectory();
    if (dataDirectory.empty() ||
        !SetCurrentDirectoryW(dataDirectory.c_str())) return 4;

    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\PlayTrack_Tracker_Instance");
    if (!instanceMutex) return 2;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(instanceMutex);
        return 0;
    }

    HANDLE stopEvent = CreateEventW(nullptr, TRUE, FALSE, L"Local\\PlayTrack_Tracker_Stop");
    if (!stopEvent) {
        ReleaseMutex(instanceMutex);
        CloseHandle(instanceMutex);
        return 3;
    }

    std::error_code folderError;
    std::filesystem::create_directories(dataDirectory / L"tracked-apps", folderError);

    AppConfig config = loadAppConfig(dataDirectory / L"config.json");
    auto scanFolders = resolveScanFolders(dataDirectory, config);
    std::vector<Application> applications = loadApplications();
    auto discovered = discoverApplications(scanFolders);
    mergeDiscoveredApplications(applications, discovered);
    startTracking(applications);
    saveApplications(applications);

    auto lastSave = std::chrono::steady_clock::now();
    auto lastDiscovery = lastSave;

    while (WaitForSingleObject(stopEvent, 2000) == WAIT_TIMEOUT) {
        const auto now = std::chrono::steady_clock::now();

        updateTracking(applications);

        if (now - lastDiscovery >= std::chrono::seconds(config.scanIntervalSeconds)) {
            // Reload config so folders added or removed in the dashboard take effect.
            config = loadAppConfig(dataDirectory / L"config.json");
            scanFolders = resolveScanFolders(dataDirectory, config);
            auto newlyDiscovered = discoverApplications(scanFolders);
            mergeDiscoveredApplications(applications, newlyDiscovered);
            saveApplications(applications);
            lastDiscovery = now;
            lastSave = now;
        }

        if (now - lastSave >= std::chrono::seconds(10)) {
            saveApplications(applications);
            lastSave = now;
        }
    }

    stopTracking(applications);
    CloseHandle(stopEvent);
    ReleaseMutex(instanceMutex);
    CloseHandle(instanceMutex);
    return 0;
}
