#ifndef TRACKING_ENGINE_H
#define TRACKING_ENGINE_H

#include <chrono>
#include <string>
#include <vector>

using TrackingClock = std::chrono::steady_clock;

struct Application {
    std::string name;
    std::wstring executable;
    long long totalSeconds = 0;
    bool running = false;
    TrackingClock::time_point sessionStart{};
    TrackingClock::time_point lastSave{};
    long long creditedSessionSeconds = 0;
};

std::string toUtf8(const std::wstring& value);
std::wstring toWide(const std::string& value);
std::string formatTime(long long seconds);
std::vector<Application> loadApplications();
void saveApplications(const std::vector<Application>& applications);
void startTracking(std::vector<Application>& applications);
void stopTracking(std::vector<Application>& applications);
void updateTracking(std::vector<Application>& applications);

#endif
