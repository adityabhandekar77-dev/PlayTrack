
#include "process_detector.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <filesystem>
#include <string>

static std::wstring normalizePath(const std::wstring& path) {
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(path, error);

    if (error) {
        return std::filesystem::path(path).lexically_normal().wstring();
    }

    return normalized.wstring();
}

bool isProcessRunning(const std::wstring& processName) {
    std::filesystem::path target(processName);
    bool compareFullPath = target.has_parent_path();
    std::wstring targetName = target.filename().wstring();
    std::wstring targetPath;

    if (compareFullPath) {
        targetPath = normalizePath(processName);
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, targetName.c_str()) != 0) {
                continue;
            }

            if (!compareFullPath) {
                found = true;
                break;
            }

            HANDLE process = OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION,
                FALSE,
                entry.th32ProcessID
            );

            if (!process) continue;

            wchar_t executablePath[32768]{};
            DWORD pathLength = static_cast<DWORD>(
                sizeof(executablePath) / sizeof(executablePath[0])
            );

            bool pathMatches = false;

            if (QueryFullProcessImageNameW(
                    process, 0, executablePath, &pathLength)) {
                pathMatches =
                    _wcsicmp(
                        normalizePath(executablePath).c_str(),
                        targetPath.c_str()
                    ) == 0;
            }

            CloseHandle(process);

            if (pathMatches) {
                found = true;
                break;
            }

        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
}
