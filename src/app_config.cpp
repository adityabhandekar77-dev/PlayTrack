#include "app_config.h"

#include <nlohmann/json.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using json = nlohmann::json;

AppConfig loadAppConfig(const std::filesystem::path& configPath) {
    AppConfig config;
    config.scanFolders = {std::filesystem::path("tracked-apps")};

    std::ifstream file(configPath);
    if (!file) return config;

    try {
        json data;
        file >> data;

        if (data.contains("scan_interval_seconds") &&
            data["scan_interval_seconds"].is_number_integer()) {
            config.scanIntervalSeconds = std::clamp(
                data["scan_interval_seconds"].get<int>(), 5, 3600
            );
        }

        if (data.contains("scan_folders") &&
            data["scan_folders"].is_array()) {
            std::vector<std::filesystem::path> folders;

            for (const auto& item : data["scan_folders"]) {
                if (item.is_string()) {
                    std::string value = item.get<std::string>();
                    if (!value.empty()) folders.emplace_back(value);
                }
            }

            if (!folders.empty()) {
                config.scanFolders = std::move(folders);
            }
        }
    } catch (...) {
        config.scanFolders = {std::filesystem::path("tracked-apps")};
        config.scanIntervalSeconds = 10;
    }

    return config;
}

bool saveAppConfig(const std::filesystem::path& configPath, const AppConfig& config) {
    const std::filesystem::path temporaryPath = configPath.wstring() + L".tmp";

    try {
        json data;
        data["scan_interval_seconds"] = std::clamp(config.scanIntervalSeconds, 5, 3600);
        data["scan_folders"] = json::array();

        for (const auto& folder : config.scanFolders) {
            data["scan_folders"].push_back(folder.string());
        }

        {
            std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
            if (!file) return false;
            file << data.dump(4) << '\n';
            file.flush();
            if (!file) {
                file.close();
                std::error_code ignored;
                std::filesystem::remove(temporaryPath, ignored);
                return false;
            }
        }

        if (!MoveFileExW(
                temporaryPath.c_str(),
                configPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::error_code ignored;
            std::filesystem::remove(temporaryPath, ignored);
            return false;
        }

        return true;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);
        return false;
    }
}
