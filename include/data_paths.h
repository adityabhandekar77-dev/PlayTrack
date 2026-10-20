#ifndef DATA_PATHS_H
#define DATA_PATHS_H

#include <filesystem>

std::filesystem::path getDataDirectory();
bool initializeDataDirectory(const std::filesystem::path& appDirectory);

#endif
