#include "tracking_engine.h"

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
    startTracking(applications);
    auto lastSave = std::chrono::steady_clock::now();

    while (WaitForSingleObject(stopEvent, 2000) == WAIT_TIMEOUT) {
        updateTracking(applications);
        auto now = std::chrono::steady_clock::now();
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
