#include "tracking_engine.h"
#include "process_detector.h"
#include "app_config.h"

#include <raylib.h>

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>

#include "folder_picker.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using Clock = std::chrono::steady_clock;

static const Color BG = {12, 15, 17, 255};
static const Color PANEL = {17, 21, 24, 255};
static const Color LINE = {151, 159, 161, 255};
static const Color TEXT = {220, 224, 222, 255};
static const Color MUTED = {143, 151, 151, 255};
static const Color PT_GREEN = {105, 207, 112, 255};
static const Color PT_YELLOW = {225, 199, 93, 255};
static const Color SELECTED = {34, 42, 45, 255};
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
    DrawRectangleRec(rectangle, highlighted ? SELECTED : PANEL);
    DrawRectangleLinesEx(rectangle, 1.0f, highlighted ? PT_GREEN : LINE);
    int textWidth = customFontLoaded
        ? static_cast<int>(MeasureTextEx(UI_FONT, label, 15.0f, 0.6f).x)
        : MeasureText(label, 15);
    drawText(label, static_cast<int>(rectangle.x + (rectangle.width - textWidth) / 2),
             static_cast<int>(rectangle.y + (rectangle.height - 15) / 2), 15,
             highlighted ? PT_GREEN : TEXT);
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

    if (length > 0 && length < MAX_PATH) {
        std::filesystem::path appDirectory(modulePath);
        appDirectory = appDirectory.parent_path();

        if (appDirectory.filename() == L"build") {
            appDirectory = appDirectory.parent_path();
        }

        SetCurrentDirectoryW(appDirectory.c_str());
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

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1100, 680, "PlayTrack");
    SetWindowMinSize(900, 580);
    SetTargetFPS(60);

    if (FileExists("C:/Windows/Fonts/consola.ttf")) {
        UI_FONT = LoadFontEx("C:/Windows/Fonts/consola.ttf", 32, nullptr, 0);
        customFontLoaded = UI_FONT.texture.id != 0;
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        (void)dt;
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
                            message = "Background tracker started.";
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
                Rectangle addAppButton = {30.0f, static_cast<float>(footerY + 6), 72.0f, 30.0f};
                Rectangle foldersButton = {110.0f, static_cast<float>(footerY + 6), 94.0f, 30.0f};
                Rectangle trackingButton = {212.0f, static_cast<float>(footerY + 6), 126.0f, 30.0f};
                Rectangle reloadButton = {346.0f, static_cast<float>(footerY + 6), 88.0f, 30.0f};
                Rectangle quitButton = {static_cast<float>(mouseW - 112), static_cast<float>(footerY + 6), 78.0f, 30.0f};
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
                        message = "Background tracker started.";
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
                    int top = 136;
                    int contentH = mouseH - top - footerH - 28;
                    int detailW = std::clamp(static_cast<int>(mouseW * 0.31f), 280, 360);
                    int listW = mouseW - margin * 2 - detailW - 12;
                    int rowY = top + 82;
                    int rowH = 36;
                    int visibleRows = std::max(0, (contentH - 96) / rowH);
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

        DrawRectangleLines(14, 14, w - 28, h - 28, LINE);
        drawText("PlayTrack", 34, 29, 22, TEXT);
        drawText(clockText(), w - 165, 33, 17, MUTED);
        drawLine(24, 62, w - 24, 62);

        DrawCircle(43, 91, 5, tracking ? PT_GREEN : MUTED);
        drawText(tracking ? "Tracking: Active" : "Tracking: Stopped", 58, 79, 18, tracking ? PT_GREEN : TEXT);
   

        int margin = 24;
        int top = 136;
        int footerH = 48;
        int contentH = h - top - footerH - 28;
        int detailW = std::clamp(static_cast<int>(w * 0.31f), 280, 360);
        int gap = 12;
        int listW = w - margin * 2 - detailW - gap;
        int listX = margin;
        int detailX = listX + listW + gap;
        int footerY = h - footerH - 16;

        DrawRectangleLines(listX, top, listW, contentH, LINE);
        long long totalUsageSeconds = 0;

for (const auto& app : applications) {
    totalUsageSeconds += app.totalSeconds;

    if (app.running) {
        totalUsageSeconds +=
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - app.sessionStart
            ).count();
    }
}

std::string summary = std::to_string(applications.size())
    + " applications  |  Total usage: "
    + formatTime(totalUsageSeconds);

drawText(summary.c_str(), 270, 82, 14, MUTED);
        drawText("Applications", listX + 12, top + 10, 18, TEXT);
        drawLine(listX + 8, top + 38, listX + listW - 8, top + 38);
        drawText("#", listX + 16, top + 48, 16, MUTED);
        drawText("Application", listX + 55, top + 48, 15, MUTED);
        drawText("Total Time", listX + static_cast<int>(listW * 0.53f), top + 48, 15, MUTED);
        drawText("Status", listX + static_cast<int>(listW * 0.78f), top + 48, 15, MUTED);
        drawLine(listX + 8, top + 75, listX + listW - 8, top + 75);

        int rowY = top + 82;
        int rowH = 36;
        int visibleRows = std::max(0, (contentH - 96) / rowH);
        int startIndex = 0;
        if (selected >= visibleRows && visibleRows > 0) startIndex = selected - visibleRows + 1;
        for (int visible = 0; visible < visibleRows; ++visible) {
            int i = startIndex + visible;
            if (i >= static_cast<int>(applications.size())) break;
            const auto& app = applications[static_cast<std::size_t>(i)];
            int y = rowY + visible * rowH;
            if (i == selected) DrawRectangle(listX + 4, y - 2, listW - 8, rowH, SELECTED);
            drawText(std::to_string(i + 1), listX + 16, y + 5, 17, MUTED);
            int nameMaxWidth = static_cast<int>(listW * 0.30f) - 62;
            std::string appName = fitText(app.name, std::max(50, nameMaxWidth), 16);
            drawText(appName, listX + 55, y + 5, 16, i == selected ? TEXT : MUTED);
            long long total = app.totalSeconds;
            if (app.running) total += std::max(0LL, std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - app.sessionStart).count() - app.creditedSessionSeconds);
            drawText(fitText(formatTime(total), static_cast<int>(listW * 0.22f), 15), listX + static_cast<int>(listW * 0.53f), y + 5, 15, TEXT);
            drawText(app.running ? "Running" : "Stopped", listX + static_cast<int>(listW * 0.78f), y + 5, 15, app.running ? PT_GREEN : MUTED);
        }
        if (applications.empty()) drawText("No applications yet. Press [A] to add one.", listX + 16, rowY + 8, 16, MUTED);

        DrawRectangleLines(detailX, top, detailW, contentH, LINE);
        drawText("Selected application", detailX + 14, top + 10, 18, TEXT);
        drawLine(detailX + 8, top + 38, detailX + detailW - 8, top + 38);
        if (!applications.empty() && selected >= 0 && selected < static_cast<int>(applications.size())) {
            const auto& app = applications[static_cast<std::size_t>(selected)];
            drawText(fitText(app.name, detailW - 32, 18), detailX + 16, top + 58, 18, PT_YELLOW);
            drawText("Executable", detailX + 16, top + 100, 15, MUTED);
            std::string exe = fitText(toUtf8(app.executable), detailW - 32, 15);
            drawText(exe, detailX + 16, top + 121, 15, TEXT);
            long long total = app.totalSeconds;
            long long session = 0;
            if (app.running) {
                session = std::max(0LL, std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - app.sessionStart).count());
                total += std::max(0LL, session - app.creditedSessionSeconds);
            }
            drawText("Total time", detailX + 16, top + 165, 16, MUTED);
            drawText(formatTime(total), detailX + 16, top + 188, 19, TEXT);
            drawText("Status", detailX + 16, top + 229, 15, MUTED);
            drawText(app.running ? "Running" : "Not running", detailX + 105, top + 229, 15, app.running ? PT_GREEN : TEXT);
            drawLine(detailX + 12, top + 265, detailX + detailW - 12, top + 265);
            drawText("Current session", detailX + 16, top + 280, 16, MUTED);
            drawText("Elapsed", detailX + 16, top + 309, 16, MUTED);
            drawText(app.running ? formatTime(session) : "-", detailX + 105, top + 309, 16, TEXT);
        } else {
            drawText("Select an application", detailX + 16, top + 62, 16, MUTED);
        }

        drawLine(24, footerY, w - 24, footerY);
        drawButton({30.0f, static_cast<float>(footerY + 6), 72.0f, 30.0f}, "Add", true);
        drawButton({110.0f, static_cast<float>(footerY + 6), 94.0f, 30.0f}, "Folders", true);
        drawButton({212.0f, static_cast<float>(footerY + 6), 126.0f, 30.0f}, tracking ? "Stop Tracking" : "Start Tracking");
        drawButton({346.0f, static_cast<float>(footerY + 6), 88.0f, 30.0f}, "Reload");
        drawText("Click an app to select  |  UP/DOWN: Select", 452, footerY + 13, 14, MUTED);
        drawButton({static_cast<float>(w - 112), static_cast<float>(footerY + 6), 78.0f, 30.0f}, "Quit", true);

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
