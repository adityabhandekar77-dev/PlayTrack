
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <filesystem>
#include <vector>

struct AppConfig {
    std::vector<std::filesystem::path> scanFolders;
    int scanIntervalSeconds = 10;
};

AppConfig loadAppConfig(const std::filesystem::path& configPath);

#endif
