#include "tracking_engine.h"
#include "app_discovery.h"
#include <system_error>

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t modulePath[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return 1;
    std::filesystem::path appDirectory(modulePath);
appDirectory = appDirectory.parent_path();

if (appDirectory.filename() == L"build") {
    appDirectory = appDirectory.parent_path();
}

SetCurrentDirectoryW(appDirectory.c_str());
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

   std::vector<Application> applications = loadApplications();

std::filesystem::path trackedFolder = appDirectory / L"tracked-apps";
std::error_code folderError;
std::filesystem::create_directories(trackedFolder, folderError);

std::vector<std::filesystem::path> scanFolders = {
    trackedFolder
};

auto discovered = discoverApplications(scanFolders);
mergeDiscoveredApplications(applications, discovered);

startTracking(applications);
saveApplications(applications);

auto lastSave = std::chrono::steady_clock::now();
auto lastDiscovery = lastSave;

    while (WaitForSingleObject(stopEvent, 2000) == WAIT_TIMEOUT) {
      auto now = std::chrono::steady_clock::now();

if (now - lastDiscovery >= std::chrono::seconds(10)) {
    auto discovered = discoverApplications(scanFolders);
    mergeDiscoveredApplications(applications, discovered);
    lastDiscovery = now;
    saveApplications(applications);
}

updateTracking(applications);
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
