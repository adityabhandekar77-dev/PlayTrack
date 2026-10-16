#include "tracking_engine.h"

#include <raylib.h>

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <string>
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

int main() {
    std::vector<Application> applications = loadApplications();
    int selected = 0;
    bool tracking = false;
    bool addDialog = false;
    int addField = 0;
    std::string newName;
    std::string newExecutable;
    std::string message = "A: add application   S: start/stop   UP/DOWN: select   Q: quit";
    double lastProcessCheck = 0.0;
    double lastAutosave = 0.0;

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
            if (addDialog) {
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
                            saveApplications(applications);
                            message = "Application added and saved.";
                            addDialog = false;
                            newName.clear();
                            newExecutable.clear();
                        } else message = "Executable is empty or already tracked.";
                    } else message = "Enter both an application name and executable.";
                }
            } else {
                if (key == KEY_Q || key == KEY_ESCAPE) {
                    if (tracking) stopTracking(applications);
                    else saveApplications(applications);
                    CloseWindow();
                    return 0;
                } else if (key == KEY_UP && !applications.empty()) {
                    selected = (selected - 1 + static_cast<int>(applications.size())) % static_cast<int>(applications.size());
                } else if (key == KEY_DOWN && !applications.empty()) {
                    selected = (selected + 1) % static_cast<int>(applications.size());
                } else if (key == KEY_A) {
                    addDialog = true;
                    addField = 0;
                    newName.clear();
                    newExecutable.clear();
                    message = "Type a name and executable; TAB switches fields.";
                } else if (key == KEY_S) {
                    tracking = !tracking;
                    if (tracking) {
                        startTracking(applications);
                        lastProcessCheck = 0.0;
                        lastAutosave = GetTime();
                        message = "Tracking enabled.";
                    } else {
                        stopTracking(applications);
                        message = "Tracking stopped. Data saved.";
                    }
                } else if (key == KEY_R) {
                    if (!tracking) applications = loadApplications();
                    selected = applications.empty() ? 0 : std::clamp(selected, 0, static_cast<int>(applications.size()) - 1);
                    message = tracking ? "Tracking refreshes automatically." : "Data reloaded from playtime.json.";
                }
            }
            key = GetKeyPressed();
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
        if (tracking && nowSeconds - lastProcessCheck >= 2.0) {
            updateTracking(applications);
            lastProcessCheck = nowSeconds;
        }

        if (tracking && nowSeconds - lastAutosave >= 10.0) {
            saveApplications(applications);
            lastAutosave = nowSeconds;
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
        drawText(tracking ? "Press [S] to stop" : "Press [S] to start", w - 210, 79, 17, MUTED);

        int margin = 24;
        int top = 112;
        int footerH = 48;
        int contentH = h - top - footerH - 28;
        int detailW = std::clamp(static_cast<int>(w * 0.31f), 280, 360);
        int gap = 12;
        int listW = w - margin * 2 - detailW - gap;
        int listX = margin;
        int detailX = listX + listW + gap;
        int footerY = h - footerH - 16;

        DrawRectangleLines(listX, top, listW, contentH, LINE);
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
        drawText("[A] Add", 36, footerY + 13, 16, PT_YELLOW);
        drawText("[S] Start/Stop", 132, footerY + 13, 16, TEXT);
        drawText("[R] Reload", 285, footerY + 13, 16, TEXT);
        drawText("[UP/DOWN] Select", 410, footerY + 13, 16, TEXT);
        drawText("[Q] Quit", w - 120, footerY + 13, 16, PT_YELLOW);
        // Keep status messages in the tracking strip so they never collide with the footer.
        if (!message.empty() && w > 1050) {
            drawText(fitText(message, std::max(80, w - 570), 12), 330, 84, 12, MUTED);
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

    if (tracking) stopTracking(applications);
    else saveApplications(applications);
    if (customFontLoaded) UnloadFont(UI_FONT);
    CloseWindow();
    return 0;
}
