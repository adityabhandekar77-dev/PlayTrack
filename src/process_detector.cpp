
#include "process_detector.h"

#include <windows.h>
#include <tlhelp32.h>
#include <cwchar>

bool isProcessRunning(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W processEntry{};
    processEntry.dwSize = sizeof(PROCESSENTRY32W);

    bool found = false;

    if (Process32FirstW(snapshot, &processEntry)) {
        do {
            if (_wcsicmp(processEntry.szExeFile, processName.c_str()) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return found;
}
