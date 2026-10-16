#include "tracking_engine.h"
#include "process_detector.h"

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

using json = nlohmann::json;
using Clock = TrackingClock;

std::string toUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring toWide(const std::string& value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

void saveApplications(const std::vector<Application>& applications) {
    json data;
    data["applications"] = json::array();
    for (const auto& app : applications) {
        data["applications"].push_back({
            {"name", app.name},
            {"executable", toUtf8(app.executable)},
            {"total_seconds", app.totalSeconds}
        });
    }
    std::ofstream file("playtime.json", std::ios::trunc);
    if (file) file << data.dump(4) << '\n';
}

std::vector<Application> loadApplications() {
    std::vector<Application> applications;
    std::ifstream file("playtime.json");
    if (!file) return applications;
    try {
        json data;
        file >> data;
        json entries;
        if (data.is_array()) entries = data;
        else if (data.is_object() && data.contains("applications") && data["applications"].is_array()) entries = data["applications"];
        else return applications;

        for (const auto& entry : entries) {
            if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string() ||
                !entry.contains("executable") || !entry["executable"].is_string()) continue;
            Application app;
            app.name = entry["name"].get<std::string>();
            app.executable = toWide(entry["executable"].get<std::string>());
            if (entry.contains("total_seconds") && entry["total_seconds"].is_number_integer())
                app.totalSeconds = std::max(0LL, entry["total_seconds"].get<long long>());
            applications.push_back(app);
        }
    } catch (...) {
        applications.clear();
    }
    return applications;
}

std::string formatTime(long long seconds) {
    seconds = std::max(0LL, seconds);
    long long hours = seconds / 3600;
    long long minutes = (seconds % 3600) / 60;
    long long remainder = seconds % 60;
    if (hours > 0) return std::to_string(hours) + "h " + std::to_string(minutes) + "m " + std::to_string(remainder) + "s";
    if (minutes > 0) return std::to_string(minutes) + "m " + std::to_string(remainder) + "s";
    return std::to_string(remainder) + "s";
}

static void accountSession(Application& app, Clock::time_point now) {
    if (!app.running) return;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - app.sessionStart).count();
    app.totalSeconds += std::max(0LL, elapsed - app.creditedSessionSeconds);
    app.creditedSessionSeconds = elapsed;
}

void startTracking(std::vector<Application>& applications) {
    auto now = Clock::now();
    for (auto& app : applications) {
        app.running = false;
        app.sessionStart = now;
        app.lastSave = now;
        app.creditedSessionSeconds = 0;
    }
}

void stopTracking(std::vector<Application>& applications) {
    auto now = Clock::now();
    for (auto& app : applications) {
        if (app.running) {
            accountSession(app, now);
            app.running = false;
            app.creditedSessionSeconds = 0;
        }
    }
    saveApplications(applications);
}

void updateTracking(std::vector<Application>& applications) {
    auto now = Clock::now();
    bool shouldSave = false;
    for (auto& app : applications) {
        bool processRunning = isProcessRunning(app.executable);
        if (processRunning && !app.running) {
            app.running = true;
            app.sessionStart = now;
            app.lastSave = now;
            app.creditedSessionSeconds = 0;
        } else if (!processRunning && app.running) {
            accountSession(app, now);
            app.running = false;
            app.creditedSessionSeconds = 0;
            shouldSave = true;
        }
    }
    if (shouldSave) saveApplications(applications);
}
