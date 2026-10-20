#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#include "tracking_engine.h"

#include "process_detector.h"
#include "app_config.h"
#include "data_paths.h"

#include <raylib.h>



#include "folder_picker.h"
#include "startup_manager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using Clock = std::chrono::steady_clock;

static const Color BG = {10, 13, 17, 255};
static const Color PANEL = {19, 24, 31, 255};
static const Color PANEL_ALT = {24, 31, 39, 255};
static const Color LINE = {45, 56, 67, 255};
static const Color TEXT = {232, 238, 242, 255};
static const Color MUTED = {139, 153, 166, 255};
static const Color PT_GREEN = {111, 224, 157, 255};
static const Color PT_YELLOW = {255, 194, 92, 255};
static const Color SELECTED = {29, 49, 51, 255};
static const Color ACCENT = {76, 210, 163, 255};
static const Color PT_RED = {255, 124, 124, 255};
static Font UI_FONT{};
static bool customFontLoaded = false;

void drawText(const std::string& text, int x, int y, int size = 20, Color color = TEXT) {
    Vector2 position = {static_cast<float>(x), static_cast<float>(y)};
    if (customFontLoaded) {
        DrawTextEx(UI_FONT, text.c_str(), position, static_cast<float>(size), 0.6f, color);
    } else {
        DrawTextEx(GetFontDefault(), text.c_str(), position, static_cast<float>(size), 1.0f, color);
    }
}

void drawCard(Rectangle rectangle, bool accent = false) {
    DrawRectangleRounded(rectangle, 0.12f, 8, PANEL);
    DrawRectangleRoundedLinesEx(rectangle, 0.12f, 8, 1.0f, accent ? ACCENT : LINE);
}

void drawSectionLabel(const std::string& label, int x, int y) {
    drawText(label, x, y, 13, MUTED);
}


bool addScanFolderFromPicker(AppConfig& config, int& selectedFolder, std::string& message) {
    std::filesystem::path folder = chooseFolderInExplorer(GetWindowHandle());
    if (folder.empty()) {
        message = "Folder selection cancelled.";
        return false;
    }

    folder = folder.lexically_normal();
    std::error_code ec;
    if (!std::filesystem::exists(folder, ec) || !std::filesystem::is_directory(folder, ec)) {
        message = "Please select an existing folder.";
        return false;
    }

    for (const auto& existing : config.scanFolders) {
        auto a = existing.lexically_normal().wstring();
        auto b = folder.wstring();
        if (_wcsicmp(a.c_str(), b.c_str()) == 0) {
            message = "That folder is already configured.";
            return false;
        }
    }

    config.scanFolders.push_back(folder);
    if (saveAppConfig("config.json", config)) {
        selectedFolder = static_cast<int>(config.scanFolders.size()) - 1;
        message = "Folder added. The tracker will scan it shortly.";
        return true;
    }

    config.scanFolders.pop_back();
    message = "Could not save config.json.";
    return false;
}

bool removeSelectedScanFolder(AppConfig& config, int& selectedFolder, std::string& message) {
    if (selectedFolder < 0 || selectedFolder >= static_cast<int>(config.scanFolders.size())) {
        message = "Select a folder to remove.";
        return false;
    }

    const auto selectedPath = config.scanFolders[static_cast<std::size_t>(selectedFolder)].lexically_normal().wstring();
    const auto requiredPath = std::filesystem::path("tracked-apps").lexically_normal().wstring();
    if (_wcsicmp(selectedPath.c_str(), requiredPath.c_str()) == 0) {
        message = "tracked-apps is required and cannot be removed.";
        return false;
    }

    auto removed = config.scanFolders[static_cast<std::size_t>(selectedFolder)];
    config.scanFolders.erase(config.scanFolders.begin() + selectedFolder);
    if (saveAppConfig("config.json", config)) {
        selectedFolder = config.scanFolders.empty() ? 0 : std::min(selectedFolder, static_cast<int>(config.scanFolders.size()) - 1);
        message = "Folder removed from scanning configuration.";
        return true;
    }

    config.scanFolders.insert(config.scanFolders.begin() + selectedFolder, removed);
    message = "Could not save config.json.";
    return false;
}

bool pointInRect(Vector2 point, Rectangle rectangle) {
    return CheckCollisionPointRec(point, rectangle);
}

void drawButton(Rectangle rectangle, const char* label, bool highlighted = false) {
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), rectangle);
    Color fill = highlighted ? SELECTED : PANEL_ALT;
    Color edge = highlighted ? ACCENT : LINE;
    Color ink = highlighted ? ACCENT : TEXT;
    if (hovered) {
        fill = highlighted ? Color{36, 67, 61, 255} : Color{34, 45, 56, 255};
        edge = ACCENT;
        ink = TEXT;
    }
    DrawRectangleRounded(rectangle, 0.18f, 8, fill);
    DrawRectangleRoundedLinesEx(rectangle, 0.18f, 8, 1.0f, edge);
    int textWidth = customFontLoaded
        ? static_cast<int>(MeasureTextEx(UI_FONT, label, 15.0f, 0.6f).x)
        : MeasureText(label, 15);
    drawText(label, static_cast<int>(rectangle.x + (rectangle.width - textWidth) / 2),
             static_cast<int>(rectangle.y + (rectangle.height - 15) / 2), 15, ink);
}

std::string fitText(const std::string& text, int maxWidth, int fontSize) {
    std::string result = text;
    while (!result.empty()) {
        Vector2 measured;
        if (customFontLoaded) measured = MeasureTextEx(UI_FONT, result.c_str(), static_cast<float>(fontSize), 0.6f);
        else measured = MeasureTextEx(GetFontDefault(), result.c_str(), static_cast<float>(fontSize), 1.0f);
        if (measured.x <= static_cast<float>(maxWidth)) return result;
        if (result.size() <= 3) return "...";
        result.resize(result.size() - 1);
    }
    return result;
}

void drawLine(int x1, int y1, int x2, int y2, Color color = LINE) {
    DrawLine(x1, y1, x2, y2, color);
}

std::string clockText() {
    SYSTEMTIME time;
    GetLocalTime(&time);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", time.wHour, time.wMinute, time.wSecond);
    return buffer;
}

bool isBackgroundTrackerRunning() {
    HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\PlayTrack_Tracker_Instance");
    if (!mutex) return false;
    CloseHandle(mutex);
    return true;
}

bool startBackgroundTracker() {
    wchar_t modulePath[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return false;

    std::filesystem::path directory = std::filesystem::path(modulePath).parent_path();
    std::filesystem::path trackerPath = directory / L"PlayTrackTracker.exe";
    if (!std::filesystem::exists(trackerPath)) return false;

    HANDLE stopEvent = CreateEventW(nullptr, TRUE, FALSE, L"Local\\PlayTrack_Tracker_Stop");
    if (!stopEvent) return false;
    ResetEvent(stopEvent);

    std::wstring command = L"\"" + trackerPath.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    BOOL created = CreateProcessW(
        trackerPath.c_str(), command.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process
    );
    if (created) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    CloseHandle(stopEvent);
    return created != FALSE;
}

bool stopBackgroundTracker() {
    HANDLE stopEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\PlayTrack_Tracker_Stop");
    if (!stopEvent) return false;
    bool signalled = SetEvent(stopEvent) != FALSE;
    CloseHandle(stopEvent);
    return signalled;
}


void refreshDashboardData(std::vector<Application>& applications) {
    auto saved = loadApplications();
    auto now = Clock::now();

    for (auto& fresh : saved) {
        for (const auto& old : applications) {
            if (_wcsicmp(fresh.executable.c_str(), old.executable.c_str()) == 0) {
                fresh.running = old.running;
                fresh.sessionStart = old.sessionStart;
                fresh.creditedSessionSeconds = old.creditedSessionSeconds;
                break;
            }
        }

        bool runningNow = isProcessRunning(fresh.executable);

        if (runningNow && !fresh.running) {
            fresh.running = true;
            fresh.sessionStart = now;
            fresh.creditedSessionSeconds = 0;
        } else if (!runningNow) {
            fresh.running = false;
            fresh.creditedSessionSeconds = 0;
        }
    }

    std::sort(saved.begin(), saved.end(),
        [](const Application& a, const Application& b) {
            return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
        });

    applications = std::move(saved);
}



int main() {
    wchar_t modulePath[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return 1;

    std::filesystem::path appDirectory(modulePath);
    appDirectory = appDirectory.parent_path();
    if (appDirectory.filename() == L"build") {
        appDirectory = appDirectory.parent_path();
    }

    if (!initializeDataDirectory(appDirectory)) {
    return 1;
}

const auto dataDirectory = getDataDirectory();

if (dataDirectory.empty() ||
    !SetCurrentDirectoryW(dataDirectory.c_str())) {
    return 1;
}

    
    std::vector<Application> applications = loadApplications();
    int selected = 0;
    bool tracking = isBackgroundTrackerRunning();
    bool addDialog = false;
    bool folderManager = false;
    int selectedFolder = 0;
    AppConfig appConfig = loadAppConfig("config.json");
    int addField = 0;
    std::string newName;
    std::string newExecutable;
    std::string message;
    double lastRefresh = 0.0;
    double lastStatusCheck = 0.0;
    float selectionHighlightY = -1.0f;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1100, 680, "PlayTrack");
    SetWindowMinSize(900, 580);
    SetTargetFPS(60);

    if (FileExists("C:/Windows/Fonts/consola.ttf")) {
        UI_FONT = LoadFontEx("C:/Windows/Fonts/consola.ttf", 32, nullptr, 0);
        customFontLoaded = UI_FONT.texture.id != 0;
    }

    if (isBackgroundTrackerRunning()) {
        tracking = true;
        if (!registerTrackerStartup()) {
            message = "Tracker is running, but Windows startup could not be configured.";
        }
    } else if (startBackgroundTracker()) {
        tracking = true;
        message = registerTrackerStartup()
            ? "Tracker started automatically. Windows startup enabled."
            : "Tracker started, but Windows startup could not be configured.";
    } else {
        tracking = false;
        message = "Dashboard ready. Press S to start the tracker.";
    }

    while (!WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.05f);
        int key = GetKeyPressed();
        while (key > 0) {
            if (folderManager) {
                if (key == KEY_ESCAPE || key == KEY_F) {
                    folderManager = false;
                    message = "Folder manager closed.";
                } else if (key == KEY_UP && !appConfig.scanFolders.empty()) {
                    selectedFolder = (selectedFolder - 1 + static_cast<int>(appConfig.scanFolders.size())) % static_cast<int>(appConfig.scanFolders.size());
                } else if (key == KEY_DOWN && !appConfig.scanFolders.empty()) {
                    selectedFolder = (selectedFolder + 1) % static_cast<int>(appConfig.scanFolders.size());
                } else if (key == KEY_A) {
                    addScanFolderFromPicker(appConfig, selectedFolder, message);
                } else if (key == KEY_DELETE || key == KEY_BACKSPACE) {
                    removeSelectedScanFolder(appConfig, selectedFolder, message);
                }
            } else if (addDialog) {
                if (key == KEY_TAB) addField = 1 - addField;
                else if (key == KEY_ESCAPE) { addDialog = false; newName.clear(); newExecutable.clear(); }
                else if (key == KEY_BACKSPACE) {
                    std::string& target = addField == 0 ? newName : newExecutable;
                    if (!target.empty()) target.pop_back();
                } else if (key == KEY_ENTER) {
                    if (!newName.empty() && !newExecutable.empty()) {
                        std::wstring executable = toWide(newExecutable);
                        bool duplicate = executable.empty();
                        for (const auto& app : applications)
                            if (_wcsicmp(app.executable.c_str(), executable.c_str()) == 0) duplicate = true;
                        if (!duplicate) {
                            Application app;
                            app.name = newName;
                            app.executable = executable;
                            applications.push_back(app);
                            selected = static_cast<int>(applications.size()) - 1;
                            if (!isBackgroundTrackerRunning()) saveApplications(applications);
                            message = "Application added and saved.";
                            addDialog = false;
                            newName.clear();
                            newExecutable.clear();
                        } else message = "Executable is empty or already tracked.";
                    } else message = "Enter both an application name and executable.";
                }
            } else {
                if (key == KEY_Q || key == KEY_ESCAPE) {
                    if (!isBackgroundTrackerRunning()) saveApplications(applications);
                    CloseWindow();
                    return 0;
                } else if (key == KEY_UP && !applications.empty()) {
                    selected = (selected - 1 + static_cast<int>(applications.size())) % static_cast<int>(applications.size());
                } else if (key == KEY_DOWN && !applications.empty()) {
                    selected = (selected + 1) % static_cast<int>(applications.size());
                } else if (key == KEY_F) {
                    folderManager = true;
                    selectedFolder = appConfig.scanFolders.empty() ? 0 : std::clamp(selectedFolder, 0, static_cast<int>(appConfig.scanFolders.size()) - 1);
                    message = "Manage scan folders.";
                } else if (key == KEY_A) {
                    if (isBackgroundTrackerRunning()) {
                        message = "Stop tracking before adding an application.";
                    } else {
                        addDialog = true;
                        addField = 0;
                        newName.clear();
                        newExecutable.clear();
                        message = "Type a name and executable; TAB switches fields.";
                    }
                } else if (key == KEY_S) {
                    if (isBackgroundTrackerRunning()) {
                        if (stopBackgroundTracker()) {
                            tracking = false;
                            message = "Stop requested. Saving tracked time...";
                        } else {
                            message = "Could not signal the background tracker.";
                        }
                    } else {
                        if (startBackgroundTracker()) {
                            tracking = true;
                            message = registerTrackerStartup()
                                ? "Tracker started. Windows startup enabled."
                                : "Tracker started, but Windows startup could not be configured.";
                        } else {
                            tracking = false;
                            message = "Could not start tracker. Check PlayTrackTracker.exe.";
                        }
                    }
                } else if (key == KEY_R) {
                    if (!isBackgroundTrackerRunning()) applications = loadApplications();
                    selected = applications.empty() ? 0 : std::clamp(selected, 0, static_cast<int>(applications.size()) - 1);
                    message = tracking ? "Tracking refreshes automatically." : "Data reloaded from playtime.json.";
                }
            }
            key = GetKeyPressed();
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            int mouseW = GetScreenWidth();
            int mouseH = GetScreenHeight();
            if (folderManager) {
                int boxW = std::min(760, mouseW - 60);
                int boxH = std::min(470, mouseH - 60);
                int boxX = (mouseW - boxW) / 2;
                int boxY = (mouseH - boxH) / 2;
                Rectangle addButton = {static_cast<float>(boxX + 20), static_cast<float>(boxY + boxH - 66), 142.0f, 34.0f};
                Rectangle removeButton = {static_cast<float>(boxX + 174), static_cast<float>(boxY + boxH - 66), 174.0f, 34.0f};
                Rectangle closeButton = {static_cast<float>(boxX + boxW - 132), static_cast<float>(boxY + boxH - 66), 112.0f, 34.0f};
                if (pointInRect(mouse, addButton)) {
                    addScanFolderFromPicker(appConfig, selectedFolder, message);
                } else if (pointInRect(mouse, removeButton)) {
                    removeSelectedScanFolder(appConfig, selectedFolder, message);
                } else if (pointInRect(mouse, closeButton)) {
                    folderManager = false;
                    message = "Folder manager closed.";
                } else {
                    int listY = boxY + 90;
                    int maxRows = std::max(1, (boxH - 190) / 29);
                    int first = selectedFolder >= maxRows ? selectedFolder - maxRows + 1 : 0;
                    for (int i = first; i < static_cast<int>(appConfig.scanFolders.size()) && i < first + maxRows; ++i) {
                        Rectangle row = {static_cast<float>(boxX + 12), static_cast<float>(listY + (i - first) * 29 - 2), static_cast<float>(boxW - 24), 26.0f};
                        if (pointInRect(mouse, row)) {
                            selectedFolder = i;
                            break;
                        }
                    }
                }
            } else if (!addDialog) {
                int footerH = 48;
                int footerY = mouseH - footerH - 16;
                Rectangle addAppButton = {30.0f, static_cast<float>(footerY + 6), 92.0f, 30.0f};
                Rectangle foldersButton = {130.0f, static_cast<float>(footerY + 6), 98.0f, 30.0f};
                Rectangle trackingButton = {236.0f, static_cast<float>(footerY + 6), 126.0f, 30.0f};
                Rectangle reloadButton = {370.0f, static_cast<float>(footerY + 6), 88.0f, 30.0f};
                Rectangle quitButton = {static_cast<float>(mouseW - 106), static_cast<float>(footerY + 6), 74.0f, 30.0f};
                if (pointInRect(mouse, addAppButton)) {
                    if (isBackgroundTrackerRunning()) {
                        message = "Stop tracking before adding an application.";
                    } else {
                        addDialog = true;
                        addField = 0;
                        newName.clear();
                        newExecutable.clear();
                        message = "Type a name and executable; TAB switches fields.";
                    }
                } else if (pointInRect(mouse, foldersButton)) {
                    folderManager = true;
                    selectedFolder = appConfig.scanFolders.empty() ? 0 : std::clamp(selectedFolder, 0, static_cast<int>(appConfig.scanFolders.size()) - 1);
                    message = "Manage scan folders.";
                } else if (pointInRect(mouse, trackingButton)) {
                    if (isBackgroundTrackerRunning()) {
                        if (stopBackgroundTracker()) {
                            tracking = false;
                            message = "Stop requested. Saving tracked time...";
                        } else message = "Could not signal the background tracker.";
                    } else if (startBackgroundTracker()) {
                        tracking = true;
                        message = registerTrackerStartup()
                            ? "Tracker started. Windows startup enabled."
                            : "Tracker started, but Windows startup could not be configured.";
                    } else {
                        tracking = false;
                        message = "Could not start tracker. Check PlayTrackTracker.exe.";
                    }
                } else if (pointInRect(mouse, reloadButton)) {
                    if (!isBackgroundTrackerRunning()) applications = loadApplications();
                    selected = applications.empty() ? 0 : std::clamp(selected, 0, static_cast<int>(applications.size()) - 1);
                    message = tracking ? "Tracking refreshes automatically." : "Data reloaded from playtime.json.";
                } else if (pointInRect(mouse, quitButton)) {
                    if (!isBackgroundTrackerRunning()) saveApplications(applications);
                    CloseWindow();
                    return 0;
                } else {
                    int margin = 24;
                    int top = 164;
                    int contentH = mouseH - top - footerH - 28;
                    int detailW = std::clamp(static_cast<int>(mouseW * 0.31f), 280, 360);
                    int listW = mouseW - margin * 2 - detailW - 12;
                    int rowY = top + 88;
                    int rowH = 40;
                    int visibleRows = std::max(0, (contentH - 110) / rowH);
                    int startIndex = selected >= visibleRows && visibleRows > 0 ? selected - visibleRows + 1 : 0;
                    for (int visible = 0; visible < visibleRows; ++visible) {
                        int i = startIndex + visible;
                        if (i >= static_cast<int>(applications.size())) break;
                        Rectangle row = {static_cast<float>(margin + 4), static_cast<float>(rowY + visible * rowH - 2), static_cast<float>(listW - 8), static_cast<float>(rowH)};
                        if (pointInRect(mouse, row)) {
                            selected = i;
                            break;
                        }
                    }
                }
            }
        }

        if (addDialog) {
            int ch = GetCharPressed();
            while (ch > 0) {
                if (ch >= 32 && ch <= 126) {
                    std::string& target = addField == 0 ? newName : newExecutable;
                    if (target.size() < 80) target.push_back(static_cast<char>(ch));
                }
                ch = GetCharPressed();
            }
        }

        double nowSeconds = GetTime();
        if (nowSeconds - lastStatusCheck >= 1.0) {
            tracking = isBackgroundTrackerRunning();
            lastStatusCheck = nowSeconds;
        }

        
if (tracking && nowSeconds - lastRefresh >= 2.0) {
    std::wstring selectedExecutable;

    if (selected >= 0 && selected < static_cast<int>(applications.size())) {
        selectedExecutable = applications[selected].executable;
    }

    refreshDashboardData(applications);

    selected = 0;

    for (int i = 0; i < static_cast<int>(applications.size()); i++) {
       if (_wcsicmp(applications[i].executable.c_str(), selectedExecutable.c_str()) == 0) {
            selected = i;
            break;
        }
    }

    lastRefresh = nowSeconds;
}


        int w = GetScreenWidth();
        int h = GetScreenHeight();
        BeginDrawing();
        ClearBackground(BG);

        DrawRectangleRounded({12.0f, 12.0f, static_cast<float>(w - 24), static_cast<float>(h - 24)}, 0.035f, 8, BG);
        drawText("PLAYTRACK", 30, 24, 23, TEXT);
        drawText("APPLICATION ACTIVITY", 31, 49, 11, MUTED);
        drawText(clockText(), w - 132, 29, 17, MUTED);
        DrawCircle(w - 151, 37, 4, tracking ? PT_GREEN : PT_RED);
        drawLine(24, 68, w - 24, 68, LINE);

        long long totalUsageSeconds = 0;
        int runningCount = 0;
        for (const auto& app : applications) {
            totalUsageSeconds += app.totalSeconds;
            if (app.running) {
                ++runningCount;
                totalUsageSeconds += std::max(0LL, std::chrono::duration_cast<std::chrono::seconds>(
                    Clock::now() - app.sessionStart).count() - app.creditedSessionSeconds);
            }
        }

        int statGap = 12;
        int statMargin = 24;
        int statW = (w - statMargin * 2 - statGap * 2) / 3;
        int statY = 80;
        int statH = 66;
        drawCard({static_cast<float>(statMargin), static_cast<float>(statY), static_cast<float>(statW), static_cast<float>(statH)});
        drawSectionLabel("TRACKING STATUS", statMargin + 15, statY + 10);
        DrawCircle(statMargin + 19, statY + 43, 4, tracking ? PT_GREEN : PT_RED);
        drawText(tracking ? "Active" : "Stopped", statMargin + 31, statY + 32, 20, tracking ? PT_GREEN : PT_RED);

        int stat2X = statMargin + statW + statGap;
        drawCard({static_cast<float>(stat2X), static_cast<float>(statY), static_cast<float>(statW), static_cast<float>(statH)});
        drawSectionLabel("TOTAL PLAYTIME", stat2X + 15, statY + 10);
        drawText(formatTime(totalUsageSeconds), stat2X + 15, statY + 31, 20, TEXT);

        int stat3X = stat2X + statW + statGap;
        drawCard({static_cast<float>(stat3X), static_cast<float>(statY), static_cast<float>(statW), static_cast<float>(statH)});
        drawSectionLabel("TRACKED APPLICATIONS", stat3X + 15, statY + 10);
        drawText(std::to_string(applications.size()) + " total  /  " + std::to_string(runningCount) + " running",
                 stat3X + 15, statY + 32, 19, TEXT);

        int margin = 24;
        int top = 164;
        int footerH = 48;
        int contentH = h - top - footerH - 28;
        int detailW = std::clamp(static_cast<int>(w * 0.31f), 280, 360);
        int gap = 12;
        int listW = w - margin * 2 - detailW - gap;
        int listX = margin;
        int detailX = listX + listW + gap;
        int footerY = h - footerH - 16;

        drawCard({static_cast<float>(listX), static_cast<float>(top), static_cast<float>(listW), static_cast<float>(contentH)});
        drawCard({static_cast<float>(detailX), static_cast<float>(top), static_cast<float>(detailW), static_cast<float>(contentH)});
        drawText("Applications", listX + 16, top + 13, 19, TEXT);
        drawText(std::to_string(applications.size()) + " tracked", listX + listW - 104, top + 17, 12, MUTED);
        drawLine(listX + 12, top + 43, listX + listW - 12, top + 43, LINE);
        drawText("APPLICATION", listX + 18, top + 55, 11, MUTED);
        drawText("PLAYTIME", listX + static_cast<int>(listW * 0.54f), top + 55, 11, MUTED);
        drawText("STATUS", listX + static_cast<int>(listW * 0.79f), top + 55, 11, MUTED);
        drawLine(listX + 12, top + 78, listX + listW - 12, top + 78, LINE);

        int rowY = top + 88;
        int rowH = 40;
        int visibleRows = std::max(0, (contentH - 110) / rowH);
        int startIndex = 0;
        if (selected >= visibleRows && visibleRows > 0) startIndex = selected - visibleRows + 1;

        if (visibleRows > 0 && selected >= startIndex && selected < startIndex + visibleRows &&
            selected < static_cast<int>(applications.size())) {
            const float targetY = static_cast<float>(rowY + (selected - startIndex) * rowH - 3);
            if (selectionHighlightY < 0.0f) selectionHighlightY = targetY;
            const float easing = 1.0f - std::exp(-18.0f * dt);
            selectionHighlightY += (targetY - selectionHighlightY) * easing;
            if (std::abs(selectionHighlightY - targetY) < 0.35f) selectionHighlightY = targetY;
            Rectangle animatedSelection = {static_cast<float>(listX + 8), selectionHighlightY,
                                            static_cast<float>(listW - 16), static_cast<float>(rowH - 2)};
            DrawRectangleRounded(animatedSelection, 0.18f, 8, SELECTED);
            DrawRectangleRounded({animatedSelection.x, animatedSelection.y + 5, 3.0f,
                                  animatedSelection.height - 10}, 0.5f, 4, ACCENT);
        } else {
            selectionHighlightY = -1.0f;
        }

        for (int visible = 0; visible < visibleRows; ++visible) {
            int i = startIndex + visible;
            if (i >= static_cast<int>(applications.size())) break;
            const auto& app = applications[static_cast<std::size_t>(i)];
            int y = rowY + visible * rowH;
            Rectangle rowRect = {static_cast<float>(listX + 8), static_cast<float>(y - 3),
                                  static_cast<float>(listW - 16), static_cast<float>(rowH - 2)};
            if (i != selected && CheckCollisionPointRec(GetMousePosition(), rowRect)) {
                DrawRectangleRounded(rowRect, 0.18f, 8, PANEL_ALT);
            }
            drawText(std::to_string(i + 1), listX + 18, y + 7, 13, MUTED);
            int nameMaxWidth = static_cast<int>(listW * 0.46f) - 45;
            drawText(fitText(app.name, std::max(55, nameMaxWidth), 15), listX + 49, y + 7, 15,
                     i == selected ? TEXT : Color{190, 201, 211, 255});
            long long total = app.totalSeconds;
            if (app.running) total += std::max(0LL, std::chrono::duration_cast<std::chrono::seconds>(
                Clock::now() - app.sessionStart).count() - app.creditedSessionSeconds);
            drawText(fitText(formatTime(total), static_cast<int>(listW * 0.23f), 14),
                     listX + static_cast<int>(listW * 0.54f), y + 8, 14, TEXT);
            DrawCircle(listX + static_cast<int>(listW * 0.79f) + 3, y + 17, 3, app.running ? PT_GREEN : MUTED);
            drawText(app.running ? "Running" : "Idle", listX + static_cast<int>(listW * 0.79f) + 13,
                     y + 8, 13, app.running ? PT_GREEN : MUTED);
        }
        if (applications.empty()) {
            drawText("Nothing tracked yet", listX + 20, rowY + 12, 17, TEXT);
            drawText("Use Add App to add an application, or add a scan folder.", listX + 20, rowY + 39, 13, MUTED);
        }

        drawText("APP DETAILS", detailX + 16, top + 14, 12, MUTED);
        drawLine(detailX + 12, top + 43, detailX + detailW - 12, top + 43, LINE);
        if (!applications.empty() && selected >= 0 && selected < static_cast<int>(applications.size())) {
            const auto& app = applications[static_cast<std::size_t>(selected)];
            drawText(fitText(app.name, detailW - 32, 19), detailX + 16, top + 61, 19, PT_YELLOW);
            drawText("EXECUTABLE", detailX + 16, top + 101, 11, MUTED);
            drawText(fitText(toUtf8(app.executable), detailW - 32, 13), detailX + 16, top + 120, 13, TEXT);

            long long total = app.totalSeconds;
            long long session = 0;
            if (app.running) {
                session = std::max(0LL, std::chrono::duration_cast<std::chrono::seconds>(
                    Clock::now() - app.sessionStart).count());
                total += std::max(0LL, session - app.creditedSessionSeconds);
            }

            DrawRectangleRounded({static_cast<float>(detailX + 14), static_cast<float>(top + 157),
                                  static_cast<float>(detailW - 28), 83.0f}, 0.12f, 8, PANEL_ALT);
            drawSectionLabel("TOTAL PLAYTIME", detailX + 27, top + 169);
            drawText(formatTime(total), detailX + 27, top + 190, 22, TEXT);
            drawSectionLabel("CURRENT SESSION", detailX + 16, top + 238);
            drawText(app.running ? formatTime(session) : "Not active", detailX + 16, top + 258, 18,
                     app.running ? PT_GREEN : MUTED);
            drawLine(detailX + 12, top + 291, detailX + detailW - 12, top + 291, LINE);
            drawText("PROCESS STATUS", detailX + 16, top + 302, 11, MUTED);
            DrawCircle(detailX + 20, top + 326, 4, app.running ? PT_GREEN : MUTED);
            drawText(app.running ? "Running now" : "Not running", detailX + 32, top + 317, 15,
                     app.running ? PT_GREEN : MUTED);
        } else {
            drawText("Select an application", detailX + 16, top + 65, 16, TEXT);
            drawText("Its playtime and process", detailX + 16, top + 92, 13, MUTED);
            drawText("status will appear here.", detailX + 16, top + 112, 13, MUTED);
        }

        drawLine(24, footerY, w - 24, footerY, LINE);
        drawButton({30.0f, static_cast<float>(footerY + 6), 92.0f, 30.0f}, "+ Add App", true);
        drawButton({130.0f, static_cast<float>(footerY + 6), 98.0f, 30.0f}, "Folders", false);
        drawButton({236.0f, static_cast<float>(footerY + 6), 126.0f, 30.0f}, tracking ? "Stop Tracking" : "Start Tracking", tracking);
        drawButton({370.0f, static_cast<float>(footerY + 6), 88.0f, 30.0f}, "Reload", false);
        const std::string footerStatus = message.empty() ? "Select an application to view its details" : message;
        drawText(fitText(footerStatus, std::max(120, w - 610), 12), 474, footerY + 14, 12, MUTED);
        drawButton({static_cast<float>(w - 106), static_cast<float>(footerY + 6), 74.0f, 30.0f}, "Quit", false);

        if (folderManager) {
            DrawRectangle(0, 0, w, h, Fade(BLACK, 0.78f));
            int boxW = std::min(760, w - 60);
            int boxH = std::min(470, h - 60);
            int boxX = (w - boxW) / 2;
            int boxY = (h - boxH) / 2;
            DrawRectangle(boxX, boxY, boxW, boxH, PANEL);
            DrawRectangleLines(boxX, boxY, boxW, boxH, LINE);
            drawText("Manage scan folders", boxX + 20, boxY + 16, 21, TEXT);
            drawText("Choose folders for automatic application discovery", boxX + 20, boxY + 48, 14, MUTED);
            drawLine(boxX + 16, boxY + 76, boxX + boxW - 16, boxY + 76);
            int listY = boxY + 90;
            int maxRows = std::max(1, (boxH - 190) / 29);
            int first = selectedFolder >= maxRows ? selectedFolder - maxRows + 1 : 0;
            for (int i = first; i < static_cast<int>(appConfig.scanFolders.size()) && i < first + maxRows; ++i) {
                int y = listY + (i - first) * 29;
                if (i == selectedFolder) DrawRectangle(boxX + 12, y - 2, boxW - 24, 26, SELECTED);
                drawText(i == selectedFolder ? ">" : " ", boxX + 20, y + 2, 15, PT_GREEN);
                drawText(fitText(appConfig.scanFolders[static_cast<std::size_t>(i)].string(), boxW - 65, 15), boxX + 42, y + 2, 15, i == selectedFolder ? TEXT : MUTED);
            }
            if (appConfig.scanFolders.empty()) drawText("No configured folders.", boxX + 22, listY + 5, 15, MUTED);
            drawLine(boxX + 16, boxY + boxH - 92, boxX + boxW - 16, boxY + boxH - 92);
            Rectangle addButton = {static_cast<float>(boxX + 20), static_cast<float>(boxY + boxH - 66), 142.0f, 34.0f};
            Rectangle removeButton = {static_cast<float>(boxX + 174), static_cast<float>(boxY + boxH - 66), 174.0f, 34.0f};
            Rectangle closeButton = {static_cast<float>(boxX + boxW - 132), static_cast<float>(boxY + boxH - 66), 112.0f, 34.0f};
            drawButton(addButton, "+ Add Folder", true);
            drawButton(removeButton, "Remove Selected");
            drawButton(closeButton, "Close");
            drawText("Click a folder to select it. Only folders can be chosen.", boxX + 20, boxY + boxH - 25, 13, MUTED);
        }

        if (addDialog) {
            int boxW = std::min(560, w - 60);
            int boxH = 210;
            int boxX = (w - boxW) / 2;
            int boxY = (h - boxH) / 2;
            DrawRectangle(0, 0, w, h, Fade(BLACK, 0.68f));
            DrawRectangle(boxX, boxY, boxW, boxH, PANEL);
            DrawRectangleLines(boxX, boxY, boxW, boxH, LINE);
            drawText("Add application", boxX + 18, boxY + 14, 19, TEXT);
            drawText("Name", boxX + 18, boxY + 54, 15, addField == 0 ? PT_YELLOW : MUTED);
            DrawRectangleLines(boxX + 110, boxY + 49, boxW - 130, 29, addField == 0 ? PT_YELLOW : LINE);
            drawText(newName + (addField == 0 ? "_" : ""), boxX + 120, boxY + 55, 16, TEXT);
            drawText("Executable", boxX + 18, boxY + 94, 15, addField == 1 ? PT_YELLOW : MUTED);
            DrawRectangleLines(boxX + 110, boxY + 89, boxW - 130, 29, addField == 1 ? PT_YELLOW : LINE);
            drawText(newExecutable + (addField == 1 ? "_" : ""), boxX + 120, boxY + 95, 16, TEXT);
            drawText("TAB: switch field    ENTER: save    ESC: cancel", boxX + 18, boxY + 157, 14, MUTED);
        }

        EndDrawing();
    }

    if (!isBackgroundTrackerRunning()) saveApplications(applications);
    if (customFontLoaded) UnloadFont(UI_FONT);
    CloseWindow();
    return 0;
}
