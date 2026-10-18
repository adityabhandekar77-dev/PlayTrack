
#include "app_config.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <string>

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
