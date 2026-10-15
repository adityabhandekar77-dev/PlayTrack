
#include "process_detector.h"

#include <windows.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

std::atomic<bool> shutdownRequested{false};

BOOL WINAPI consoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT ||
        signal == CTRL_BREAK_EVENT ||
        signal == CTRL_CLOSE_EVENT)
    {
        shutdownRequested.store(true);
        return TRUE;
    }

    return FALSE;
}

struct Application
{
    std::string name;
    std::wstring executable;
    long long totalSeconds = 0;
    bool running = false;
    Clock::time_point sessionStart{};
    Clock::time_point lastSave{};
};

std::string toUtf8(const std::wstring& value)
{
    if (value.empty())
        return {};

    int size = WideCharToMultiByte(
        CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr
    );

    if (size <= 0)
        return {};

    std::string result(size, '\0');

    WideCharToMultiByte(
        CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr
    );

    return result;
}

std::wstring toWide(const std::string& value)
{
    if (value.empty())
        return {};

    int size = MultiByteToWideChar(
        CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()),
        nullptr, 0
    );

    if (size <= 0)
        return {};

    std::wstring result(size, L'\0');

    MultiByteToWideChar(
        CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()),
        result.data(), size
    );

    return result;
}

void saveApplications(const std::vector<Application>& applications)
{
    json data;
    data["applications"] = json::array();

    for (const auto& app : applications)
    {
        data["applications"].push_back({
            {"name", app.name},
            {"executable", toUtf8(app.executable)},
            {"total_seconds", app.totalSeconds}
        });
    }

    std::ofstream file("playtime.json", std::ios::trunc);

    if (!file)
    {
        std::cerr << "Error: Could not save playtime.json.\n";
        return;
    }

    file << data.dump(4) << '\n';

    if (!file)
        std::cerr << "Error: Failed while writing playtime.json.\n";
}

std::vector<Application> loadApplications()
{
    std::vector<Application> applications;
    std::ifstream file("playtime.json");

    if (!file)
        return applications;

    try
    {
        json data;
        file >> data;

        json entries;

        if (data.is_array())
        {
            entries = data;
        }
        else if (data.is_object() &&
                 data.contains("applications") &&
                 data["applications"].is_array())
        {
            entries = data["applications"];
        }
        else
        {
            std::cerr << "Warning: Invalid playtime.json format.\n";
            return applications;
        }

        for (const auto& entry : entries)
        {
            if (!entry.is_object() ||
                !entry.contains("name") ||
                !entry["name"].is_string() ||
                !entry.contains("executable") ||
                !entry["executable"].is_string())
            {
                continue;
            }

            Application app;
            app.name = entry["name"].get<std::string>();
            app.executable = toWide(
                entry["executable"].get<std::string>()
            );

            if (entry.contains("total_seconds") &&
                entry["total_seconds"].is_number_integer())
            {
                app.totalSeconds =
                    entry["total_seconds"].get<long long>();

                if (app.totalSeconds < 0)
                    app.totalSeconds = 0;
            }

            applications.push_back(app);
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error loading playtime.json: "
                  << error.what() << '\n';
    }

    return applications;
}

std::string formatTime(long long totalSeconds)
{
    long long hours = totalSeconds / 3600;
    long long minutes = (totalSeconds % 3600) / 60;
    long long seconds = totalSeconds % 60;

    return std::to_string(hours) + "h " +
           std::to_string(minutes) + "m " +
           std::to_string(seconds) + "s";
}

int main()
{
    SetConsoleCtrlHandler(consoleHandler, TRUE);

    std::vector<Application> applications = loadApplications();

    while (true)
    {
        std::cout << "\n===== PlayTrack =====\n";
        std::cout << "1. Add application\n";
        std::cout << "2. List tracked applications\n";
        std::cout << "3. Start tracking\n";
        std::cout << "4. Exit\n";
        std::cout << "Choose: ";

        int choice;

        if (!(std::cin >> choice))
        {
            if (std::cin.eof())
                break;

            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n'
            );

            std::cout << "Please enter a valid number.\n";
            continue;
        }

        std::cin.ignore(
            std::numeric_limits<std::streamsize>::max(), '\n'
        );

        if (choice == 1)
        {
            Application app;
            std::string executableInput;

            std::cout << "Enter application name: ";
            std::getline(std::cin, app.name);

            std::cout << "Enter executable name (e.g. notepad.exe): ";
            std::getline(std::cin, executableInput);

            if (app.name.empty() || executableInput.empty())
            {
                std::cout << "Name and executable cannot be empty.\n";
                continue;
            }

            app.executable = toWide(executableInput);

            if (app.executable.empty())
            {
                std::cout << "Invalid executable name.\n";
                continue;
            }

            bool duplicate = false;

            for (const auto& existing : applications)
            {
                if (_wcsicmp(
                        existing.executable.c_str(),
                        app.executable.c_str()) == 0)
                {
                    duplicate = true;
                    break;
                }
            }

            if (duplicate)
            {
                std::cout << "This application is already tracked.\n";
                continue;
            }

            applications.push_back(app);
            saveApplications(applications);

            std::cout << "Application added successfully.\n";
        }
        else if (choice == 2)
        {
            if (applications.empty())
            {
                std::cout << "No applications added yet.\n";
                continue;
            }

            std::cout << "\n===== Tracked Applications =====\n";

            for (std::size_t i = 0; i < applications.size(); ++i)
            {
                const auto& app = applications[i];

                std::cout << i + 1 << ". " << app.name
                          << " (" << toUtf8(app.executable) << ")\n";

                std::cout << "   Total time: "
                          << formatTime(app.totalSeconds) << '\n';
            }
        }
        else if (choice == 3)
        {
            if (applications.empty())
            {
                std::cout << "Add an application before tracking.\n";
                continue;
            }

            shutdownRequested.store(false);

            std::cout << "\nTracking started.\n";
            std::cout << "Press Ctrl+C to stop tracking and save.\n";

            for (auto& app : applications)
            {
                app.running = false;
                app.sessionStart = Clock::now();
                app.lastSave = app.sessionStart;
            }

            while (!shutdownRequested.load())
            {
                auto now = Clock::now();

                for (auto& app : applications)
                {
                    bool isRunning = isProcessRunning(app.executable);

                    if (isRunning && !app.running)
                    {
                        app.running = true;
                        app.sessionStart = now;
                        app.lastSave = now;

                        std::cout << app.name << " started.\n";
                    }
                    else if (!isRunning && app.running)
                    {
                        auto elapsed =
                            std::chrono::duration_cast<
                                std::chrono::seconds>(
                                    now - app.sessionStart
                                ).count();

                        app.totalSeconds += elapsed;
                        app.running = false;

                        saveApplications(applications);

                        std::cout << app.name << " stopped. Total: "
                                  << formatTime(app.totalSeconds)
                                  << '\n';
                    }
                    else if (isRunning && app.running)
                    {
                        auto elapsed =
                            std::chrono::duration_cast<
                                std::chrono::seconds>(
                                    now - app.sessionStart
                                ).count();

                        if (now - app.lastSave >=
                            std::chrono::seconds(10))
                        {
                            app.totalSeconds += elapsed;
                            app.sessionStart = now;
                            app.lastSave = now;

                            saveApplications(applications);
                        }
                    }
                }

                std::this_thread::sleep_for(
                    std::chrono::seconds(2)
                );
            }

            auto now = Clock::now();

            for (auto& app : applications)
            {
                if (app.running)
                {
                    auto elapsed =
                        std::chrono::duration_cast<
                            std::chrono::seconds>(
                                now - app.sessionStart
                            ).count();

                    app.totalSeconds += elapsed;
                    app.running = false;
                }
            }

            saveApplications(applications);

            std::cout << "\nPlaytime saved. Tracking stopped.\n";
            return 0;
        }
        else if (choice == 4)
        {
            saveApplications(applications);
            std::cout << "PlayTrack closed.\n";
            return 0;
        }
        else
        {
            std::cout << "Invalid choice. Choose 1, 2, 3, or 4.\n";
        }
    }

    saveApplications(applications);
    return 0;
}
